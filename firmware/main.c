#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <generated/soc.h>
#include <system.h>

#include "aht10.h"
#include "lora_RFM95.h"

#define FIRMWARE_VERSION "Tarefa05_v1.1"
#define LORA_FREQUENCY 915.0f
#define SEND_INTERVAL_MS 10000

/* helpers de tempo */
static void sleep_ms(unsigned int ms)
{
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for (volatile int j = 0; j < 2000; j++) ;
#endif
    }
}

/* monta payload textual: "T:23.45,H:56.78\n" */
static void format_sensor_payload(char *buf, size_t buflen, sensor_data_T *s)
{
    // temperatura e umidade estão em centésimos
    int temp_int = s->temperatura / 100;
    int temp_dec = abs(s->temperatura) % 100;
    int hum_int = s->umidade / 100;
    int hum_dec = abs(s->umidade) % 100;

    // formata com dois decimais cada
    snprintf(buf, buflen, "Temperatura:%d.%02d C, Umidade:%d.%02d UR\n",
             temp_int, temp_dec, hum_int, hum_dec);
}

/* lê sensor e envia via LoRa em formato textual */
static void send_sensor_reading(void)
{
    sensor_data_T sd;
    char payload[64];

    printf("[FW %s] Iniciando leitura do sensor AHT10...\n", FIRMWARE_VERSION);

    if (!aht10_get_data(&sd)) {
        printf("[FW] ERRO: leitura AHT10 falhou. Abortando envio.\n");
        return;
    }

    format_sensor_payload(payload, sizeof(payload), &sd);
    printf("[FW] Payload formado: %s", payload);

    if (!lora_send_text(payload, strlen(payload))) {
        printf("[FW] ERRO: envio LoRa falhou.\n");
    } else {
        printf("[FW] Enviado com sucesso.\n");
    }
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

#ifdef CSR_TIMER0_BASE
    timer0_en_write(0);
    timer0_reload_write(0);
    timer0_load_write(CONFIG_CLOCK_FREQUENCY / 1000000);
    timer0_en_write(1);
    timer0_update_value_write(1);
#endif

    sleep_ms(500);

    printf("\n------ LiteX BIOS (Tarefa05) ------\n");
    printf("Firmware: %s\n", FIRMWARE_VERSION);

    i2c_init();
    if (aht10_init() != 0) {
        printf("[FW] Aviso: inicialização AHT10 reportou erro (continuando para debug)...\n");
    }

    if (!lora_init()) {
        printf("[FW] ERRO CRÍTICO: LoRa não inicializado. Parando.\n");
        while (1) sleep_ms(1000);
    }

    while (1) {
        send_sensor_reading();
        sleep_ms(SEND_INTERVAL_MS);
    }

    return 0;
}