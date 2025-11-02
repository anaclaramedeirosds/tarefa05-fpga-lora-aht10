#ifndef LORA_RFM95_H_
#define LORA_RFM95_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool lora_init(void);
bool lora_send_bytes(const uint8_t *data, size_t len);
bool lora_send_text(const char *s, size_t len);
void lora_set_mode(uint8_t mode);
uint8_t lora_read_reg(uint8_t reg);
void lora_write_reg(uint8_t reg, uint8_t value);

#endif // LORA_RFM95_H_
