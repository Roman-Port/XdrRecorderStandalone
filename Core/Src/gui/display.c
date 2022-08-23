#include "gui/display.h"
#include "main.h"
#include <string.h>

static uint8_t tx_display_framebuffer[1 + sizeof(display_framebuffer)];

/* DISPLAY DEFINES */

#define SSD1306_I2C_ADDRESS 0x3D
#define SSD1306_TIMEOUT     1000
#define SSD1306_CMD_START   0x00    // indicates following bytes are commands
#define SSD1306_DATA_START  0x40    // indicates following bytes are data

/* INTERNAL DISPLAY COMMANDS */

#define SEND_CMD_BEGIN(opcode, size) uint8_t buf[size]; buf[0] = SSD1306_CMD_START; buf[1] = opcode;
#define SEND_CMD_END return HAL_I2C_Master_Transmit(&hi2c2, SSD1306_I2C_ADDRESS << 1 , buf, sizeof(buf), SSD1306_TIMEOUT);

#define WRAP_CMD_0(name, opcode) HAL_StatusTypeDef name() { SEND_CMD_BEGIN(opcode, 2); SEND_CMD_END;  }
#define WRAP_CMD_1(name, opcode) HAL_StatusTypeDef name(uint8_t arg1) { SEND_CMD_BEGIN(opcode, 3); buf[2] = arg1; SEND_CMD_END; }
#define WRAP_CMD_2(name, opcode) HAL_StatusTypeDef name(uint8_t arg1, uint8_t arg2) { SEND_CMD_BEGIN(opcode, 4); buf[2] = arg1; buf[3] = arg2; SEND_CMD_END; }
#define WRAP_CMD_3(name, opcode) HAL_StatusTypeDef name(uint8_t arg1, uint8_t arg2, uint8_t arg3) { SEND_CMD_BEGIN(opcode, 4); buf[2] = arg1; buf[3] = arg2; buf[4] = arg3; SEND_CMD_END; }

WRAP_CMD_0(ssd1306_set_display_off, 0xAE)
WRAP_CMD_0(ssd1306_set_display_on, 0xAF)
WRAP_CMD_1(ssd1306_set_display_clock_div, 0xD5)
WRAP_CMD_1(ssd1306_set_multiplex, 0xA8)
WRAP_CMD_1(ssd1306_set_vertical_offset, 0xD3)
WRAP_CMD_0(ssd1306_set_start_line, 0x40)
WRAP_CMD_1(ssd1306_set_charge_pump, 0x8D)
WRAP_CMD_1(ssd1306_set_address_mode, 0x20)
WRAP_CMD_0(ssd1306_set_colscan_ascending, 0xA0)
WRAP_CMD_0(ssd1306_set_colscan_descending, 0xA1)
WRAP_CMD_0(ssd1306_set_comscan_ascending, 0xC0)
WRAP_CMD_0(ssd1306_set_comscan_descending, 0xC8)
WRAP_CMD_1(ssd1306_set_compins, 0xD1)
WRAP_CMD_1(ssd1306_set_contrast, 0x81)
WRAP_CMD_1(ssd1306_set_precharge, 0xD9)
WRAP_CMD_1(ssd1306_set_vcomlevel, 0xDB)
WRAP_CMD_0(ssd1306_set_display_entirely_on, 0xA5)
WRAP_CMD_0(ssd1306_set_display_entirely_off, 0xA4)
WRAP_CMD_0(ssd1306_set_invert_on, 0xA7)
WRAP_CMD_0(ssd1306_set_invert_off, 0xA6)
WRAP_CMD_0(ssd1306_set_scroll_on, 0x2F)
WRAP_CMD_0(ssd1306_set_scroll_off, 0x2E)
WRAP_CMD_2(ssd1306_set_page_address, 0x22)
WRAP_CMD_2(ssd1306_set_col_range, 0x21)

/* DISPLAY API */

void display_init() {
	//Initialize the device
	ssd1306_set_display_off();
	ssd1306_set_display_clock_div(DISPLAY_HEIGHT);
	ssd1306_set_multiplex(DISPLAY_WIDTH - 1);
	ssd1306_set_vertical_offset(0);
	ssd1306_set_start_line();
	ssd1306_set_charge_pump(0x14);
	ssd1306_set_address_mode(1);
	ssd1306_set_colscan_descending();
	ssd1306_set_comscan_descending();
	ssd1306_set_compins(0x02); //sequential
	ssd1306_set_contrast(0x00);
	ssd1306_set_precharge(0xF1);
	ssd1306_set_vcomlevel(0x40);
	ssd1306_set_display_entirely_off();
	ssd1306_set_invert_off();
	ssd1306_set_scroll_off();
	ssd1306_set_display_on();
	ssd1306_set_col_range(0, DISPLAY_WIDTH - 1);
	ssd1306_set_page_address(0, DISPLAY_HEIGHT - 1);
}

void display_update() {
	//Copy into the TX buffer
	tx_display_framebuffer[0] = SSD1306_DATA_START;
	memcpy(&tx_display_framebuffer[1], display_framebuffer, sizeof(display_framebuffer));

	//Begin transmission
	HAL_I2C_Master_Transmit(&hi2c2, SSD1306_I2C_ADDRESS << 1, tx_display_framebuffer, sizeof(tx_display_framebuffer), SSD1306_TIMEOUT);
}

void display_update_async() {
	//Copy into the TX buffer
	tx_display_framebuffer[0] = SSD1306_DATA_START;
	memcpy(&tx_display_framebuffer[1], display_framebuffer, sizeof(display_framebuffer));

	//Begin transmission
	HAL_I2C_Master_Transmit_IT(&hi2c2, SSD1306_I2C_ADDRESS << 1, tx_display_framebuffer, sizeof(tx_display_framebuffer));
}
