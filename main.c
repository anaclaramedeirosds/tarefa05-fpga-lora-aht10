#include <stdio.h>
#include <math.h> 
#include <stdlib.h>
#include <string.h>
#include "inc/ssd1306.h"
#include "inc/ssd1306_fonts.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/lora_RFM95.h"

// DEFINIÇÕES DE HARDWARE
// Configuração dos pinos para comunicação SPI com módulo LoRa
#define PINO_MISO  16
#define PINO_CS    17  
#define PINO_SCK   18
#define PINO_MOSI  19
#define PINO_RST   20
#define PINO_DIO0  8   // Pino para detecção de interrupção

// Configuração de frequência do rádio LoRa
#define FREQ_LORA  915E6  // Frequência em Hz (região das Américas)

// Estrutura para armazenamento dos dados recebidos
typedef struct {
    int16_t temperatura;
    int16_t umidade;
} dados_sensor_t;

struct repeating_timer timer_animacao;
int contador_animacao = 0;

// FUNÇÕES DE VISUALIZAÇÃO
// Função para exibir dados no display OLED
void exibir_dados(float temp, float umid) {
    // Limpa toda a tela
    ssd1306_Fill(Black);
    
    // Formata os valores com uma casa decimal
    char texto_temp[16], texto_umid[16];
    snprintf(texto_temp, sizeof(texto_temp), "%.1fC", temp);
    snprintf(texto_umid, sizeof(texto_umid), "%.1f%%", umid);
    
    // Cabeçalho simples
    ssd1306_SetCursor(0, 2);
    ssd1306_WriteString("TEMPERATURA E ", Font_6x8, White);
    
    ssd1306_SetCursor(82, 2);
    ssd1306_WriteString("UMIDADE", Font_6x8, White);
    
    // Linha divisória 
    ssd1306_FillRectangle(0, 15, 127, 16, White);
    
    // Valores principais - fonte maior e centralizada
    ssd1306_SetCursor(15, 25);
    ssd1306_WriteString(texto_temp, Font_11x18, White);
    
    ssd1306_SetCursor(75, 25);
    ssd1306_WriteString(texto_umid, Font_11x18, White);
    
    // Rodapé informativo
    ssd1306_SetCursor(25, 55);
    ssd1306_WriteString("Dados LoRa", Font_6x8, White);
    
    ssd1306_UpdateScreen();
}

// Bitmap personalizado para fundo do display
void desenhar_fundo() {
    const uint8_t bitmap_fundo[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // ... (conteúdo do bitmap reduzido para exemplo)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    ssd1306_DrawBitmap(0, 0, bitmap_fundo, 128, 64, White);
    ssd1306_UpdateScreen();
}

// Animação de espera por dados
bool animacao_aguarde(struct repeating_timer *t) {
    if(contador_animacao < 3){
        ssd1306_SetCursor((contador_animacao + 4) * 15, 25);
        ssd1306_WriteString(".", Font_16x15, White);
    }
    if(contador_animacao > 2 && contador_animacao < 6){
        ssd1306_SetCursor((contador_animacao + 1) * 15, 25);
        ssd1306_WriteString(" ", Font_16x15, White);
    }
    if(contador_animacao > 5){
        contador_animacao = -1;
    }
    contador_animacao++;
    ssd1306_UpdateScreen();
    return true;
}

// Tela de espera por dados
void tela_aguardando_dados() {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(12, 10);
    ssd1306_WriteString("Aguardando", Font_16x15, White);
    ssd1306_SetCursor(25, 30);
    ssd1306_WriteString("dados", Font_16x15, White);
    ssd1306_UpdateScreen();

   // add_repeating_timer_ms(200, animacao_aguarde, NULL, &timer_animacao);
    sleep_ms(10000);
}

// INICIALIZAÇÃO DO SISTEMA

void inicializar_sistema() {
    stdio_init_all();
    ssd1306_Init();

    ssd1306_Fill(Black);
    
    // Configuração do módulo LoRa
    lora_config_t config_lora = {
        .spi_instance = spi0,
        .pin_miso = PINO_MISO,
        .pin_cs = PINO_CS,
        .pin_sck = PINO_SCK,
        .pin_mosi = PINO_MOSI,
        .pin_rst = PINO_RST,
        .pin_dio0 = PINO_DIO0,
        .frequency = FREQ_LORA
    };

    // Inicialização do módulo LoRa
    printf("Iniciando comunicação LoRa em %.0f Hz...\n", (float)FREQ_LORA);
    
    ssd1306_SetCursor(5, 10);
    ssd1306_WriteString("Iniciando LoRa:", Font_6x8, White);
    ssd1306_SetCursor(5, 20);
    
    char info_freq[8];
    snprintf(info_freq, sizeof(info_freq), "%.1fMHz", (float)FREQ_LORA/1000000);
    ssd1306_WriteString(info_freq, Font_6x8, White);
    ssd1306_UpdateScreen();
    sleep_ms(800);

    ssd1306_SetCursor(5, 35);
    
    if (!lora_init(config_lora)) {
        printf("ERRO: Falha na inicializacao do LoRa\n");
        ssd1306_WriteString("Falha no LoRa", Font_6x8, White);
        ssd1306_UpdateScreen();
        while (1); // Loop infinito em caso de falha
    }
    
    printf("SUCESSO: LoRa inicializado\n");
    ssd1306_WriteString("LoRa OK", Font_6x8, White);
    ssd1306_UpdateScreen();
    sleep_ms(1200);

    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    // Inicia modo de recepção contínua
    lora_start_rx_continuous();
}

// PROGRAMA PRINCIPAL

int main() {
    dados_sensor_t dados_recebidos;
    bool primeira_recepcao = true;

    inicializar_sistema();
    tela_aguardando_dados();

    while (1) {
        int bytes_recebidos = lora_receive_bytes((uint8_t*)&dados_recebidos, 
                                               sizeof(dados_recebidos));

        if (bytes_recebidos == sizeof(dados_recebidos)) {
            if (primeira_recepcao) {
                cancel_repeating_timer(&timer_animacao);
                ssd1306_Fill(Black);
                primeira_recepcao = false;
            }
            
            exibir_dados((float)dados_recebidos.temperatura/100, 
                        (float)dados_recebidos.umidade/100);
        }
    }    
    
    return 0;
}