#ifndef INC_SDRAM_H_
#define INC_SDRAM_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SDRAM_ADDR ((__IO uint8_t*)0xC0000000)
#define SDRAM_SIZE 33554432 /* in bytes */

void W9825G6KH_init(SDRAM_HandleTypeDef* sdram);
int run_ram_check(__IO uint32_t* start, uint32_t len, uint32_t** failingAddress, uint32_t* failingPattern, uint32_t* failingPatternOut);

#endif
