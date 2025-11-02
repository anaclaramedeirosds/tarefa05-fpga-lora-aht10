#include "lora_RFM95.h"

#include <stdio.h>
#include <string.h>
#include <generated/csr.h>
#include <system.h>

#define TX_TIMEOUT_MS 5000
#define SPI_MODE_MANUAL (1 << 16)
#define SPI_CS_MASK     0x0001

/* registradores omitidos por brevidade (igual ao original) */
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_ADDR 0x0E
#define REG_FIFO_RX_BASE_ADDR 0x0F
#define REG_IRQ_FLAGS_MASK 0x11
#define REG_IRQ_FLAGS 0x12
#define REG_MODEM_CONFIG_1 0x1D
#define REG_MODEM_CONFIG_2 0x1E
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_SYNC_WORD 0x39
#define REG_DIO_MAPPING_1 0x40
#define REG_VERSION 0x42
#define REG_PA_DAC 0x4D
#define REG_OCP 0x0B

#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX    0x03
#define IRQ_TX_DONE_MASK 0x08

/* helpers de delay */
static void delay_ms_local(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for (volatile int j = 0; j < 2000; j++);
#endif
    }
}

/* SPI (mantive nomes compatíveis com CSR) */
static void spi_master_init(void) {
    spi_cs_write(SPI_MODE_MANUAL | 0x0000);
#ifdef CSR_SPI_LOOPBACK_ADDR
    spi_loopback_write(0);
#endif
    delay_ms_local(1);
}

static inline void spi_sel(void) {
    spi_cs_write(SPI_MODE_MANUAL | SPI_CS_MASK);
    busy_wait_us(2);
}
static inline void spi_unsel(void) {
    spi_cs_write(SPI_MODE_MANUAL | 0x0000);
    busy_wait_us(2);
}
static inline uint8_t spi_xfer(uint8_t tx) {
    uint32_t rv;
    spi_mosi_write((uint32_t)tx);
    spi_control_write(
        (1 << CSR_SPI_CONTROL_START_OFFSET) |
        (8 << CSR_SPI_CONTROL_LENGTH_OFFSET)
    );
    while ((spi_status_read() & (1 << CSR_SPI_STATUS_DONE_OFFSET)) == 0) ;
    rv = spi_miso_read();
    return (uint8_t)(rv & 0xFF);
}

static void lora_write_fifo_bytes(const uint8_t *data, uint8_t len) {
    spi_sel();
    spi_xfer(REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) spi_xfer(data[i]);
    spi_unsel();
}

/* registrador read/write (público) */
uint8_t lora_read_reg(uint8_t reg) {
    uint8_t v;
    spi_sel();
    spi_xfer(reg & 0x7F);
    v = spi_xfer(0x00);
    spi_unsel();
    return v;
}

void lora_write_reg(uint8_t reg, uint8_t value) {
    spi_sel();
    spi_xfer(reg | 0x80);
    spi_xfer(value);
    spi_unsel();
}

/* set mode (público) */
void lora_set_mode(uint8_t mode) {
    lora_write_reg(REG_OP_MODE, (0x80 | mode));
}

/* init (público) - apenas texto da mensagem mudou */
bool lora_init(void) {
    spi_master_init();
    uint8_t ver;

#ifdef CSR_LORA_RESET_BASE
    lora_reset_out_write(0); delay_ms_local(5);
    lora_reset_out_write(1); delay_ms_local(10);
#endif

    ver = lora_read_reg(REG_VERSION);
    if (ver != 0x12) {
        printf("[LORA] versão inesperada 0x%02X\n", ver);
        return false;
    }

    lora_set_mode(MODE_SLEEP);
    uint64_t frf = ((uint64_t)915000000 << 19) / 32000000;
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));

    /* configurações de rádio (mantidas) */
    lora_write_reg(REG_PA_CONFIG, 0xFF);
    lora_write_reg(REG_PA_DAC, 0x87);
    lora_write_reg(REG_MODEM_CONFIG_1, 0x78);
    lora_write_reg(REG_MODEM_CONFIG_2, 0xC4);
    lora_write_reg(REG_MODEM_CONFIG_3, 0x0C);
    lora_write_reg(REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(REG_PREAMBLE_LSB, 0x0C);
    lora_write_reg(REG_SYNC_WORD, 0x12);
    lora_write_reg(REG_OCP, 0x37);
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
    lora_write_reg(REG_LNA, 0x23);
    lora_write_reg(REG_IRQ_FLAGS_MASK, 0x00);
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);

    lora_set_mode(MODE_STDBY);
    delay_ms_local(10);

    printf("[LORA] inicializado em 915 MHz\n");
    return true;
}

/* envia bytes brutos (público) */
bool lora_send_bytes(const uint8_t *data, size_t len) {
    if (len == 0 || len > 255) {
        printf("[LORA] tamanho inválido %d\n", (int)len);
        return false;
    }

    lora_set_mode(MODE_STDBY);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    lora_write_fifo_bytes(data, (uint8_t)len);
    lora_write_reg(REG_PAYLOAD_LENGTH, (uint8_t)len);
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_DIO_MAPPING_1, 0x40);

    lora_set_mode(MODE_TX);

    int to = TX_TIMEOUT_MS;
    while (to-- > 0) {
        if (lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
            lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
            lora_set_mode(MODE_STDBY);
            return true;
        }
        delay_ms_local(1);
    }

    lora_set_mode(MODE_STDBY);
    printf("[LORA] timeout no TX\n");
    return false;
}

/* --- NOVA FUNÇÃO PÚBLICA: envia string textual (wrapper) --- */
bool lora_send_text(const char *s, size_t len) {
    // converte texto para bytes e chama o envio de bytes
    return lora_send_bytes((const uint8_t *)s, len);
}