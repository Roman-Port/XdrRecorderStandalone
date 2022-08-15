#ifndef INC_RECORDER_H_
#define INC_RECORDER_H_

#include <stdint.h>
#include "fatfs.h"
#include "recorder/dmaio.h"
#include "recorder/wav.h"

#define RECORDER_BUFFER_SIZE 32768

#define RECORDER_TICK_STATUS_OK 0
#define RECORDER_TICK_STATUS_IO_ERR -1

// Processed in a loop while the recorder is active. Should return RECORDER_TICK_STATUS_*
typedef int (*recorder_class_tick_cb)(FIL* file, rec_dmaio_t* io, uint64_t* samplesWritten);

// Processed once after initialization
typedef void (*recorder_class_init_cb)();

// Declares all of the options for a recorder
typedef struct {

	const char* name;
	DMA_HandleTypeDef* dma_handle_a; // Required
	DMA_HandleTypeDef* dma_handle_b; // Required only if input_requires_slave is set

	uint32_t input_bytes_per_sample; // Bytes per sample for the DMA input
	uint8_t input_requires_slave;    // Set to 1 if both the A and B DMA queue buffers are used

	uint16_t output_channels;
	uint16_t output_bits_per_sample;
	uint32_t output_sample_rate;

	recorder_class_tick_cb tick_cb;
	recorder_class_init_cb init_cb;

} recorder_class_t;

// Initializes a recorder_class_t object for IQ
void recorder_class_init_iq(recorder_class_t* cls);

// Initializes the recorder.
void recorder_init();

// Requests that a recorder at index begins recording. Interrupt safe.
void recorder_request_start(int index);

// Should be called in processing loop. Handles events.
void recorder_tick();

/* USER CODE */

// USER IMPLIMENTED - Called when a recorder starts. Output should be opened. Returns 1 on success, otherwise 0
int recorder_handler_begin(int index, FIL* output);

// USER IMPLIMENTED - Called when a recorder stops (either normally or with an error)
void recorder_handler_stop(int index, int code);

#endif /* INC_RECORDER_H_ */
