#ifndef INC_SDMAN_H_
#define INC_SDMAN_H_

#include "fatfs.h"

#define SDMAN_STATE_REMOVED      0
#define SDMAN_STATE_MOUNTING     1
#define SDMAN_STATE_MOUNT_ERROR  2
#define SDMAN_STATE_READY        3
#define SDMAN_STATE_IO_ERROR     4

extern FATFS sdman_fs;
extern int sdman_state;

void sdman_tick();

void sdman_report_io_error();

#endif /* INC_SDMAN_H_ */
