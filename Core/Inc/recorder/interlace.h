#ifndef INC_RECORDER_INTERLACE_H_
#define INC_RECORDER_INTERLACE_H_

#include <stdint.h>

// Starts the interlace process. Interlaces A and B into output. Returns 1 if it was successfully started, otherwise 0.
int interlace_begin(uint16_t* a, uint16_t* b, uint32_t* output, uint16_t len);

// Checks if the interlacing hardware is busy. If it is, 1 is returned. If it's free, 0 is returned.
int interlace_busy();

// Checks the status of the interlacing process. If it's done, returns 1. If it's still working, returns 0.
int interlace_end();

#endif
