#ifndef AHT10_H_
#define AHT10_H_

#include <stdint.h>
#include <stdbool.h>

// Estrutura para armazenar leitura do sensor.
// Valores em centésimos (×100) para evitar ponto flutuante.
typedef struct {
    int16_t temperatura; // temperatura * 100
    int16_t umidade;     // umidade * 100
} sensor_data_T;

/* Inicializa o driver I2C (bitbang). Deve ser chamada antes de usar o AHT10. */
void i2c_init(void);

/* Varre barramento I2C e imprime endereços encontrados (debug). */
void i2c_scan(void);

/* Inicializa o AHT10. Retorna 0 em sucesso, -1 em erro. */
int aht10_init(void);

/* Lê e imprime valores do sensor (modo debug). */
void aht10_read(void);

/* Lê dados do AHT10 e preenche a struct apontada por 'd'. Retorna true em sucesso. */
bool aht10_get_data(sensor_data_T *d);

#endif // AHT10_H_