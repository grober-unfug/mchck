#include <mchck.h>

#include <usb/usb.h>
#include <usb/cdc-acm.h>



#define SysTick_MAX_TICKS	(0xFFFFFFUL)



static struct cdc_ctx cdc;
static struct timeout_ctx t;

static int tick;
static int count;
static int16_t data[3];



/* SysTick */

static void
systick_init(uint32_t ticks)
{
	/* Set reload register. -1 is used to have ticks ticks from 0 to 0. */
	SYST_RVR = (ticks - 1) & SysTick_MAX_TICKS;
	/* Reset counter value */
	SYST_CVR = 0;
	/* Enable SysTick counter with processor clock. */
	SYST_CSR = (1UL << 2) | (1UL << 0);
}

static void
systick_restart(void)
{
	tick = SYST_CVR;
}

static uint32_t
systick_diff(void)
{
	/* this code only works, if the time from 0 to 0 is larger than the measured time. */
	int32_t tick_diff = tick - SYST_CVR;
	if (SYST_CSR & (1UL << 16)) /* counted to 0 since last time this was read */
		return tick_diff + SYST_RVR + 1;
	else
		return tick_diff;
}



/* ADC based Sensors */

/* onboard temperature */
static void
onbtemp_done(uint16_t data, int error, void *cbdata)
{
        unsigned accum volt = adc_as_voltage(data);
        accum volt_diff = volt - 0.719k;
        accum temp_diff = volt_diff * (1000K / 1.715K);
        accum temp_deg = 25k - temp_diff;

        printf("OnbTemp: %.1k°C\r\n\n", temp_deg);

        onboard_led(ONBOARD_LED_TOGGLE);
}

/* gas sensor MQ-135 on a sainsmart board connected to adc port B1 */
static void
gas_done(uint16_t data, int error, void *cbdata)
{
        unsigned accum volt = adc_as_voltage(data);

        printf("Gas: %.2KV\r\n", volt);

	adc_sample_start(ADC_TEMP, onbtemp_done, NULL);
}

/* photo resistor connected to adc port D1 with voltage divider,
 * voltate is measured at the serial resistance of 100kOhm */
static void
light_done(uint16_t data, int error, void *cbdata)
{
	unsigned accum volt = adc_as_voltage(data);
	accum resistance = 100k * (3.3/volt - 1);

	printf("Bright: %.2kkOhm\r\n", resistance);

	adc_sample_start(ADC_PTB1, gas_done, NULL);
}



/* AM2302/DHT22 humidity and temperature sensor connected to port D0;
 * digital communication, where the falling flanks are detected */

static void
humitemp_done(void)
{
	/* The first 16 bit of the transmission hold the humidity,
	 * the second 16 bit the temperature in sign and magnitude
	 * representation, and the last 8 bit are a checksum */
	uint8_t cs = data[0] + (data[0] >> 8) + data[1] + (data[1] >> 8);
	if (cs != data[2])
		printf("zonk\r\n");

	accum humi  = data[0] / 10;
	printf("Hum: %.1k%\r\n",humi);

	accum temp = data[1] / 10;
	if (temp < 0) /* MSB set */
		/* convert from sign and magnitude representation
		 * to standard signed integer with two's complement */
		temp = -((uint16_t)temp & 0x7fff);
	printf("Temp: %.1k°C\r\n",temp);

	adc_sample_start(ADC_PTD1, light_done, NULL);
}

static void
humitemp_read(uint32_t tick_diff)
{
	/* count starts at -2, the first two falling flanks signal start of transmission */
	if (count >= 0)
	{
		/* duration between two falling flanks,
		   around 76µs should be a zero and 120µs should be a one */
		data[count/16] = (data[count/16] << 1) | (tick_diff/48 > 100);
	}

	/* 40 bits are transmitted */
	if ( count == 39)
	{
		humitemp_done();
	}

	count++;
}

void
PORTD_Handler(void)
{
	if (pin_physport_from_pin(GPIO_PTD0)->pcr[pin_physpin_from_pin(GPIO_PTD0)].isf)
	{
                pin_physport_from_pin(GPIO_PTD0)->pcr[pin_physpin_from_pin(GPIO_PTD0)].raw |= 0; /* clear isf */
		uint32_t tick_diff = systick_diff();
		systick_restart();
		humitemp_read(tick_diff);
	}
}

static void
humitemp_startreading(void)
{
	/* setup pin for reading */
	gpio_dir(GPIO_PTD0, GPIO_INPUT);
	systick_restart();

	/* set digital debounce/filter */
	pin_physport_from_pin(GPIO_PTD0)->dfcr.cs = PORT_CS_LPO;
	pin_physport_from_pin(GPIO_PTD0)->dfwr.filt = 31;

	/* falling flank interrupt */
	pin_physport_from_pin(GPIO_PTD0)->pcr[pin_physpin_from_pin(GPIO_PTD0)].irqc = PCR_IRQC_INT_FALLING;

	/* start reading */
	int_enable(IRQ_PORTD);
}

static void
humitemp_start(void)
{
	/* reset */
	int_disable(IRQ_PORTD);
	count = -2;
	data[0] = data[1] = data[2] = 0;

	/* 1ms LOW to initiate transmission */
	gpio_dir(GPIO_PTD0, GPIO_OUTPUT);
	gpio_write(GPIO_PTD0, GPIO_LOW);
	timeout_add(&t, 1, humitemp_startreading, NULL);
}



/* Communication over USB */

static void
new_data(uint8_t *data, size_t len)
{
	humitemp_start();
        cdc_read_more(&cdc);
}

static void
init_vcdc(int config)
{
        cdc_init(new_data, NULL, &cdc);
        cdc_set_stdout(&cdc);
}

static const struct usbd_device cdc_device =
	USB_INIT_DEVICE(0x2323,              /* vid */
			3,                   /* pid */
			u"mchck.org",        /* vendor */
			u"weather station",  /* product" */
			(init_vcdc,          /* init */
			 CDC)                /* functions */
	);



void
main(void)
{
	timeout_init();
        adc_init();
	systick_init(SysTick_MAX_TICKS); /* SysTick runs for about 349ms between 0 and 0 */
        usb_init(&cdc_device);
        sys_yield_for_frogs();
}
