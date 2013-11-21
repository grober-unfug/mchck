#include <mchck.h>
#include "nRF24L01plus.h"

static struct nrf_status_t nrf_status;
static struct timeout_ctx t;

static void
send_command(struct nrf_transaction_t *trans, spi_cb *cb)
{
	sg_init(trans->tx_sg,
		(void *)&trans->cmd, 1, trans->tx_data, trans->tx_len);
	if (trans->rx_data)
		sg_init(trans->rx_sg,
			(void *)&trans->status, 1, trans->rx_data, trans->rx_len);
	else
		sg_init(trans->rx_sg,
			(void *)&trans->status, 1);

	spi_queue_xfer_sg(&trans->sp_ctx, NRF_SPI_CS,
			trans->tx_sg, trans->rx_sg,
			cb, trans);
}

static void
nrf_read_status_done(void *cbdata)
{
	struct nrf_status_t *status = cbdata;

	nrf_status = *status;
	printf("RX_DR: %x\r\n", nrf_status.RX_DR);
	printf("TX_DS: %x\r\n", nrf_status.TX_DS);
	printf("MAX_RT: %x\r\n", nrf_status.MAX_RT);
	printf("RX_P_NO: %x\r\n", nrf_status.RX_P_NO);
	printf("TX_FULL: %x\r\n", nrf_status.TX_FULL);
}

void
nrf_read_status(void)
{
	static struct spi_ctx rdsr_ctx;
	static const enum NRF_CMD cmd = NRF_CMD_R_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_STATUS);
	static uint8_t rxbuf[2];

	spi_queue_xfer(&rdsr_ctx, NRF_SPI_CS,
		       &cmd, 1, rxbuf, 2,
		       nrf_read_status_done, &rxbuf[1]);
}

static void
nrf_read_register_done(void *cbdata)
{
	static struct nrf_transaction_t *trans;
	trans = cbdata;
	printf("reg: %x\r\n", *(uint8_t *)trans->rx_data);
}

void
nrf_read_register(enum NRF_REG_ADDR reg_addr)
{
	static struct nrf_transaction_t trans;
	static uint8_t rx_data;

	trans.cmd = NRF_CMD_R_REGISTER | (NRF_REG_MASK & reg_addr);
	trans.tx_data = NULL;
	trans.tx_len = 0;
	trans.rx_len = 1;
	trans.rx_data = (void *) &rx_data;

	send_command(&trans, &nrf_read_register_done);
}

/*

static void
nrf_set_power_rxtx(uint8_t up, uint8_t rx)
{
	static struct nrf_config_t config = {
		.pad = 0
	};
	config.PRIM_RX = rx;
	config.PWR_UP = up;
	nrf_ctx.trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_CONFIG);
	nrf_ctx.trans.tx_len = 1;
	nrf_ctx.trans.tx_data = &config;
	nrf_ctx.trans.rx_len = 0;
	send_command(&nrf_ctx.trans, NULL);
}

static void
nrf_set_datapipe_payload_size(uint8_t pipe, uint8_t size)
{
	static struct nrf_datapipe_payload_size_t dps = {
		.pad = 0
	};
	nrf_ctx.trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & (NRF_REG_ADDR_RX_PW_P0 + pipe));
	nrf_ctx.trans.tx_len = 1;
	nrf_ctx.trans.tx_data = &dps;
	nrf_ctx.trans.rx_len = 0;
	dps.size = size;
	send_command(&nrf_ctx.trans, NULL);
}

static void
nrf_set_rx_address(uint8_t pipe, struct nrf_addr_t *addr)
{
	nrf_ctx.trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & (NRF_REG_ADDR_RX_ADDR_P0 + pipe));
	nrf_ctx.trans.tx_len = addr->size;
	nrf_ctx.trans.tx_data = addr->value;
	nrf_ctx.trans.rx_len = 0;
	send_command(&nrf_ctx.trans, NULL);
}

static void
handle_read_payload(void *data)
{
	nrf_ctx.trans.user_cb(NULL, nrf_ctx.trans.user_data, nrf_ctx.trans.user_data_len);
}

static void
handle_status(void *data)
{
	struct nrf_transaction_t *trans = data;

	if (trans->status.RX_DR) {
		gpio_write(NRF_CE, NRF_CE_TX);
		nrf_ctx.trans.cmd = NRF_CMD_R_RX_PAYLOAD;
		nrf_ctx.trans.tx_len = 0;
		nrf_ctx.trans.rx_len = nrf_ctx.user_data_len;
		nrf_ctx.trans.rx_data = nrf_ctx.trans.user_data;
		send_command(&nrf_ctx.trans, handle_read_payload);
	}

	if (trans->status.TX_DS) {
		status.TX_DS = 1;
	}

	if (trans->status.MAX_RT) {
		status.MAX_RT = 1;
	}

	// clear interrupt
	trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_STATUS);
	trans.tx_len = 1;
	trans.tx_data = &trans->status;
	trans.rx_len = 0;
	send_command(&trans_ci, NULL);
}

void
PORTC_Handler(void)
{
	static struct nrf_transaction_t trans = {
		.cmd = NRF_CMD_NOP,
		.tx_len = 0,
		.rx_len = 0
	};
	send_command(&trans, handle_status);
}

void
nrf_receive(struct nrf_addr_t *addr, void *data, uint8_t len, nrf_data_callback cb)
{
	nrf_ctx.user_cb = cb;
	nrf_ctx.user_data = data;
	nrf_ctx.user_data_len = len;
	nrf_ctx.state = NRF_RX;

	nrf_set_datapipe_payload_size(0, len);
	nrf_set_rx_address(0, addr);
	send_command(&nrf_ctx.trans, NULL);
	nrf_set_power_rxtx(1, 1);
	gpio_write(NRF_CE, NRF_CE_RX);
}

void
nrf_send(struct nrf_addr_t *addr, void *data, uint8_t len, nrf_data_callback cb)
{
}
*/

void
nrf_set_retry(uint8_t ard, uint8_t arc)
{
	static struct nrf_transaction_t trans;
	static struct nrf_setup_retr_t setup_retr;

	trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_SETUP_RETR);
	trans.tx_data = (void *) &setup_retr;
	trans.tx_len = 1;

	setup_retr.ARD = ard;
	setup_retr.ARC = arc;

	send_command(&trans, NULL);
}

void
nrf_set_channel(uint8_t channel)
{
	static struct nrf_transaction_t trans;
	static struct nrf_rf_ch_t rf_ch = {
		.pad = 0
	};

	trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_RF_CH);
	trans.tx_data = (void *) &rf_ch;
	trans.tx_len = 1;

	rf_ch.RF_CH = channel;

	send_command(&trans, NULL);
}

void
nrf_set_rate_and_power(enum nrf_data_rate_t data_rate, enum nrf_tx_output_power_t output_power)
{
	static struct nrf_transaction_t trans;
	static struct nrf_rf_setup_t rf_setup = {
		.CONT_WAVE = 0,
		.pad0 = 0,
		.pad6 = 0,
		.PLL_LOCK = 0
	};

	trans.cmd = NRF_CMD_W_REGISTER | (NRF_REG_MASK & NRF_REG_ADDR_RF_SETUP);
	trans.tx_data = (void *) &rf_setup;
	trans.tx_len = 0;

	rf_setup.RF_PWR = output_power;
	rf_setup.RF_DR_HIGH = data_rate;

	send_command(&trans, NULL);
}

void
nrf_setup(void *cbdata)
{
	static struct nrf_ctx *nrf;
	nrf = cbdata;
	nrf_set_retry(nrf->ard, nrf->arc);
	nrf_set_channel(nrf->channel);
	nrf_set_rate_and_power(nrf->rate, nrf->power);
}

void
nrf_init(struct nrf_ctx *nrf)
{
	spi_init();

	pin_mode(NRF_CSN, PIN_MODE_MUX_ALT2);
	pin_mode(NRF_SCK, PIN_MODE_MUX_ALT2);
	pin_mode(NRF_MOSI, PIN_MODE_MUX_ALT2);
	pin_mode(NRF_MISO, PIN_MODE_MUX_ALT2);

	gpio_dir(NRF_CE, GPIO_OUTPUT);
	gpio_write(NRF_CE, NRF_CE_TX);


//	pin_mode(NRF_IRQ, PIN_MODE_PULLDOWN);
//	pin_physport_from_pin(NRF_IRQ)->pcr[pin_physpin_from_pin(NRF_IRQ)].irqc = PCR_IRQC_INT_RISING;
//	int_enable(IRQ_PORTC);

	timeout_add(&t, 200, nrf_setup, nrf);
}
