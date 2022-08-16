#ifndef INC_RECORDER_H_
#define INC_RECORDER_H_

#include <stdint.h>
#include "fatfs.h"
#include "recorder/wav.h"

#define RECORDER_MAX_BUFFERS 512
#define RECORDER_BUFFER_SIZE 65536 // in samples (per channel)
#define RECORDER_INSTANCES_COUNT 1

#define RECORDER_STATE_IDLE 0
#define RECORDER_STATE_RECORDING 1
#define RECORDER_STATE_STOPPING 2

#define RECORDER_TICK_STATUS_OK 0
#define RECORDER_TICK_STATUS_IO_ERR -1

typedef struct {

	void* buffer;
	uint8_t state; // All 8 bits must be set for this to be considered full

} recorder_setup_buffer_t;

typedef struct {

	recorder_setup_buffer_t buffers[RECORDER_MAX_BUFFERS];
	int buffer_count;

	uint64_t dropped_samples;

} recorder_setup_t;

// Called to setup
typedef void (*recorder_class_init_cb)(recorder_setup_t* setup);

// Called to begin recieving
typedef void (*recorder_class_start_cb)();

// Called to stop recieving
typedef void (*recorder_class_stop_cb)();

// Declares all of the options for a recorder
typedef struct {

	const char* name;

	uint32_t input_bytes_per_sample; //across all channels

	uint16_t output_channels;
	uint16_t output_bits_per_sample;
	uint32_t output_sample_rate;

	recorder_class_init_cb init_cb;
	recorder_class_start_cb start_cb;
	recorder_class_stop_cb stop_cb;

} recorder_class_t;

/* CLASSES */

extern const recorder_class_t recorder_class_iq;

/* MAIN */

// Initializes the recorder.
void recorder_init();

// Query info about an instance by index
void recorder_query_instance_info(int index, recorder_class_t* info, uint8_t* state, uint64_t* received_samples, uint64_t* dropped_samples);

// Requests that a recorder at index begins recording. Interrupt safe.
void recorder_request_start(int index);

// Should be called in processing loop. Handles events.
void recorder_tick();

/* USER CODE */

// USER IMPLIMENTED - Called when a recorder starts. Output should be opened. Returns 1 on success, otherwise 0
int recorder_handler_begin(int index, FIL* output);

// USER IMPLIMENTED - Called when a recorder stops (either normally or with an error). File should be closed by this function.
void recorder_handler_stop(int index, FIL* output, int code);

#endif /* INC_RECORDER_H_ */
