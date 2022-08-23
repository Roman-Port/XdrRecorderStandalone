#include <stdint.h>
#include <string.h>
#include "sdman.h"
#include "main.h"
#include "fatfs_platform.h"

FATFS sdman_fs;
int sdman_state = SDMAN_STATE_REMOVED;

static uint32_t next_mount_attempt = 0;
static uint32_t last_insertion_status = 0xFF;

int sdman_is_inserted() {
	return BSP_PlatformIsDetected();
}

void sdman_tick() {
	//Check for removal or insertion
	int inserted = sdman_is_inserted();
	if (inserted != last_insertion_status) {
		if (inserted) {
			//Card was just inserted; Wait a moment for it to finish being inserted into the slot and mount it
			sdman_state = SDMAN_STATE_MOUNTING;
			next_mount_attempt = HAL_GetTick() + 250;
		} else {
			//Card was just removed; Zero out structures
			memset(&sdman_fs, 0, sizeof(sdman_fs));

			//Unlink FatFS driver
			FATFS_UnLinkDriver(SDPath);

			//Reinitialize FatFS for next insertion
			MX_FATFS_Init();

			//Update state
			sdman_state = SDMAN_STATE_REMOVED;
		}
		last_insertion_status = inserted;
	}

	//Check if we should attempt to mount the media
	if (sdman_state == SDMAN_STATE_MOUNTING && inserted && HAL_GetTick() >= next_mount_attempt) {
		//Mount the card
		int code = f_mount(&sdman_fs, SDPath, 1);
		if (code == FR_OK)
			sdman_state = SDMAN_STATE_READY;
		else
			sdman_state = SDMAN_STATE_MOUNT_ERROR;
	}
}

void sdman_report_io_error() {
	sdman_state = SDMAN_STATE_IO_ERROR;
}
