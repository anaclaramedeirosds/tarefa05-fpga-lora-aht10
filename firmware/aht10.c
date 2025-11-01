#include "aht10.h"
#include <stdio.h>
#include <stdlib.h>
#include <generated/csr.h>
#include <system.h>

static void local_delay_ms(unsigned int ms)
{
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for (volatile int j = 0; j < 1000; j++) ;
#endif
    }
}

/* estado de saída do bitbang I2C */
static uint32_t i2c_out_state = 0;

static inline void i2c_set_line_scl(int v) {
    if (v) i2c_out_state |= (1 << CSR_I2C_W_SCL_OFFSET);
    else   i2c_out_state &= ~(1 << CSR_I2C_W_SCL_OFFSET);
    i2c_w_write(i2c_out_state);
}

static inline void i2c_set_line_sda(int v) {
    if (v) i2c_out_state |= (1 << CSR_I2C_W_SDA_OFFSET);
    else   i2c_out_state &= ~(1 << CSR_I2C_W_SDA_OFFSET);
    i2c_w_write(i2c_out_state);
}

static inline void i2c_set_oe(int v) {
    if (v) i2c_out_state |= (1 << CSR_I2C_W_OE_OFFSET);
    else   i2c_out_state &= ~(1 << CSR_I2C_W_OE_OFFSET);
    i2c_w_write(i2c_out_state);
}

static inline int i2c_read_sda_line(void) {
    return (i2c_r_read() & (1 << CSR_I2C_R_SDA_OFFSET)) != 0;
}

void i2c_init(void)
{
    i2c_set_oe(1);
    i2c_set_line_scl(1);
    i2c_set_line_sda(1);
    local_delay_ms(1);
}

/* operações básicas I2C (start/stop/read/write) */
static inline void i2c_start_cond(void) {
    i2c_set_line_sda(1); i2c_set_oe(1); i2c_set_line_scl(1);
    busy_wait_us(5);
    i2c_set_line_sda(0); busy_wait_us(5);
    i2c_set_line_scl(0); busy_wait_us(5);
}

static inline void i2c_stop_cond(void) {
    i2c_set_line_sda(0); i2c_set_oe(1); i2c_set_line_scl(0);
    busy_wait_us(5);
    i2c_set_line_scl(1); busy_wait_us(5);
    i2c_set_line_sda(1); busy_wait_us(5);
}

static bool i2c_write_byte(uint8_t b) {
    i2c_set_oe(1);
    for (int i = 0; i < 8; i++) {
        i2c_set_line_sda((b & 0x80) != 0);
        busy_wait_us(5);
        i2c_set_line_scl(1); busy_wait_us(5);
        i2c_set_line_scl(0); busy_wait_us(5);
        b <<= 1;
    }
    // ACK phase
    i2c_set_oe(0);
    i2c_set_line_sda(1);
    busy_wait_us(5);
    i2c_set_line_scl(1); busy_wait_us(5);
    bool ack = !i2c_read_sda_line();
    i2c_set_line_scl(0); busy_wait_us(5);
    return ack;
}

static uint8_t i2c_read_byte(bool ack) {
    uint8_t b = 0;
    i2c_set_oe(0); i2c_set_line_sda(1); busy_wait_us(5);
    for (int i = 0; i < 8; i++) {
        b <<= 1;
        i2c_set_line_scl(1); busy_wait_us(5);
        if (i2c_read_sda_line()) b |= 1;
        i2c_set_line_scl(0); busy_wait_us(5);
    }
    i2c_set_oe(1); i2c_set_line_sda(!ack); busy_wait_us(5);
    i2c_set_line_scl(1); busy_wait_us(5);
    i2c_set_line_scl(0); busy_wait_us(5);
    return b;
}

/* escaneia endereços I2C (debug) */
void i2c_scan(void) {
    printf("[I2C] Scan iniciando...\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        i2c_start_cond();
        if (i2c_write_byte((addr << 1) | 0)) {
            printf("  Encontrado: 0x%02X\n", addr);
        }
        i2c_stop_cond();
        busy_wait_us(100);
    }
    printf("[I2C] Scan finalizado.\n");
}

#define AHT10_ADDR 0x38

int aht10_init(void) {
    i2c_start_cond();
    if (!i2c_write_byte((AHT10_ADDR << 1) | 0)) { i2c_stop_cond(); return -1; }
    if (!i2c_write_byte(0xE1)) { i2c_stop_cond(); return -1; }
    if (!i2c_write_byte(0x08)) { i2c_stop_cond(); return -1; }
    if (!i2c_write_byte(0x00)) { i2c_stop_cond(); return -1; }
    i2c_stop_cond();
    local_delay_ms(100);
    return 0;
}

bool aht10_get_data(sensor_data_T *out) {
    uint8_t raw[6];
    uint32_t rh, rt;

    i2c_start_cond();
    if (!i2c_write_byte((AHT10_ADDR << 1) | 0)) { i2c_stop_cond(); return false; }
    if (!i2c_write_byte(0xAC)) { i2c_stop_cond(); return false; }
    if (!i2c_write_byte(0x33)) { i2c_stop_cond(); return false; }
    if (!i2c_write_byte(0x00)) { i2c_stop_cond(); return false; }
    i2c_stop_cond();

    local_delay_ms(80);

    i2c_start_cond();
    if (!i2c_write_byte((AHT10_ADDR << 1) | 1)) { i2c_stop_cond(); return false; }
    for (int i = 0; i < 5; i++) raw[i] = i2c_read_byte(true);
    raw[5] = i2c_read_byte(false);
    i2c_stop_cond();

    if (raw[0] & 0x80) {
        printf("[AHT10] Err: sensor ocupado.\n");
        return false;
    }

    rh = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | (raw[3] >> 4);
    rt = (((uint32_t)raw[3] & 0x0F) << 16) | ((uint32_t)raw[4] << 8) | raw[5];

    out->umidade = (int16_t)(((uint64_t)rh * 10000) / 0x100000);
    out->temperatura = (int16_t)((((uint64_t)rt * 20000) / 0x100000) - 5000);

    return true;
}

void aht10_read(void) {
    sensor_data_T s;
    printf("[AHT10] leitura (modo debug)...\n");
    if (aht10_get_data(&s)) {
        printf("Umid: %d.%02d %%\n", s.umidade / 100, s.umidade % 100);
        printf("Temp: %d.%02d C\n", s.temperatura / 100, abs(s.temperatura) % 100);
    } else {
        printf("[AHT10] Falha leitura.\n");
    }
}