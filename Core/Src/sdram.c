#include "sdram.h"

// Good chunk of this file is from https://chowdera.com/2020/12/20201205093909390t.html

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

static int SDRAM_SendCommand(SDRAM_HandleTypeDef* sdram, uint32_t CommandMode, uint32_t Bank, uint32_t RefreshNum, uint32_t RegVal)
{

    uint32_t CommandTarget;
    FMC_SDRAM_CommandTypeDef Command;

    if (Bank == 1) {

        CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    } else if (Bank == 2) {

        CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    }

    Command.CommandMode = CommandMode;
    Command.CommandTarget = CommandTarget;
    Command.AutoRefreshNumber = RefreshNum;
    Command.ModeRegisterDefinition = RegVal;

    if (HAL_SDRAM_SendCommand(sdram, &Command, 0x1000) != HAL_OK) {

        return -1;
    }

    return 0;
}

void W9825G6KH_init(SDRAM_HandleTypeDef* sdram) {
	/* 1.  Clock enable command  */
	SDRAM_SendCommand(sdram, FMC_SDRAM_CMD_CLK_ENABLE, 1, 1, 0);

	/* 2.  Time delay , At least 100us */
	HAL_Delay(1);

	/* 3. SDRAM All precharge commands  */
	SDRAM_SendCommand(sdram, FMC_SDRAM_CMD_PALL, 1, 1, 0);

	/* 4.  Auto refresh command  */
	SDRAM_SendCommand(sdram, FMC_SDRAM_CMD_AUTOREFRESH_MODE, 1, 8, 0);

	/* 5.  To configure SDRAM Mode register  */
	uint32_t temp = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1            |          // Set burst length ：1
			   SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL     |          // Set burst type ： continuity
			   SDRAM_MODEREG_CAS_LATENCY_3             |          // Set up CL value ：3
			   SDRAM_MODEREG_OPERATING_MODE_STANDARD   |          // Set the operation mode ： standard
			   SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;              // Set burst write mode ： Single point access
	SDRAM_SendCommand(sdram, FMC_SDRAM_CMD_LOAD_MODE, 1, 1, temp);

	/* 6.  Set the self refresh rate  */
	/*
	(<SDRAM refresh period> / <Number of rows>） * (<system clock speed in MHz> / 2) – 20
	= (64000(64 ms) / 8192) * (96 / 2) - 20
	= 355
	*/
	HAL_SDRAM_ProgramRefreshRate(sdram, 355);
}

int run_ram_check(__IO uint32_t* start, uint32_t len, uint32_t** failingAddress, uint32_t* failingPattern, uint32_t* failingPatternOut) {
	//Test changing each bit
	for (int i = 0; i < 32; i++) {
		//Create pattern
		uint32_t pattern = 0xFFFFFFFF ^ (1U << i);

		//Write pattern to all bytes
		for (uint32_t addr = 0; addr < len; addr++)
			start[addr] = pattern;

		//Verify
		uint32_t value;
		for (uint32_t addr = 0; addr < len; addr++) {
			//Read back
			value = start[addr];

			//Check
			if (value != pattern) {
				//Error! Set results
				(*failingAddress) = (uint32_t**)&start[addr];
				(*failingPattern) = pattern;
				(*failingPatternOut) = value;
				return -1;
			}
		}
	}
	return 0;
}
