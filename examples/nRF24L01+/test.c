#include <mchck.h>
#include <usb/usb.h>
#include <usb/cdc-acm.h>
#include "nRF24L01plus.h"

#define RX_SIZE 32
#define CHANNEL 3

static struct cdc_ctx cdc;
static struct nrf_ctx nrf;
//static struct timeout_ctx t;

/*
static uint8_t rx_buffer[RX_SIZE];
static uint8_t tx_buffer = 0;

static struct nrf_addr_t test_addr = {
	.value =  { 0xaa, 0xde, 0xad, 0xbe, 0xef },
	.size = 5
};

static void
data_received(struct nrf_addr_t *sender, void *data, uint8_t len)
{
	onboard_led(*(uint8_t*)data);
}

static void
data_sent(struct nrf_addr_t *receiver, void *data, uint8_t len)
{
	// len == 0 when tx fifo full
	// len < 0 when max retransmit
	nrf_receive(rx_buffer, RX_SIZE, data_received);
}

static void
ping(void *data)
{
	tx_buffer ^= 1;
	nrf_send(&test_addr, &tx_buffer, 1, data_sent);
	timeout_add(&t, 200, ping, NULL);
}
*/





/* Communication over USB */

static void
new_data(uint8_t *data, size_t len)
{
	nrf_read_buffer(NRF_REG_ADDR_RF_CH);
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
                        u"nRF24L01plus test",/* product" */
                        (init_vcdc,          /* init */
                         CDC)                /* functions */
        );



int
main(void)
{
	nrf.channel = CHANNEL;
	timeout_init();
	usb_init(&cdc_device);
	nrf_init(&nrf);
//	nrf_set_channel(CHANNEL);
//	nrf_set_rate_and_power(NRF_DATA_RATE_1MBPS, NRF_TX_POWER_0DBM);
//	ping(NULL);
	sys_yield_for_frogs();
}
