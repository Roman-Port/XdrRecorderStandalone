#include "recorder.h"
#include "sdram.h"
#include <string.h>

recorder_instance_t recorders[RECORDER_INSTANCES_COUNT];
static uint16_t recorder_start_flags = 0; // Each bit represents an index that we want to start capture from

static void setup_recorder_buffers() {
	//We'll now need to figure out how to divide up our available memory. We want to maximize the amount of time each
	//recorder will get to store. Ideally, all recorders will an equal amount of time, so we'll need to calculate this.

	//Determine total bytes/sec recorders will consume
	uint32_t totalBytesPerSec = 0;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++)
		totalBytesPerSec += recorders[i].info->input_bytes_per_sample * recorders[i].info->output_sample_rate;

	//Calculate the number of seconds we can buffer for each recorder
	uint32_t bufferSecondsPerRecorder = SDRAM_SIZE / totalBytesPerSec;

	//Finally, we can set up memory for each buffer
	uint8_t* addr = (uint8_t*)SDRAM_ADDR;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		//Calculate the number of samples we'll be able to record
		uint32_t bufferSamples = bufferSecondsPerRecorder * recorders[i].info->output_sample_rate;

		//Calculate the number of buffers this is equivalent to
		uint32_t bufferCount = bufferSamples / RECORDER_BUFFER_SIZE;

		//Calculate increment
		uint32_t increment = recorders[i].info->input_bytes_per_sample * RECORDER_BUFFER_SIZE;

		//Make sure we aren't overflowing
		assert(bufferCount <= RECORDER_MAX_BUFFERS);

		//Set
		recorders[i].setup.buffer_count = bufferCount;

		//Setup each buffer
		for (int b = 0; b < bufferCount; b++) {
			//Configure
			recorders[i].setup.buffers[b].state = 0;
			recorders[i].setup.buffers[b].buffer = addr;

			//Zero it out. This helps with debugging and also serves as a simple check to make sure the RAM is even accessible
			memset(addr, 0, increment);

			//Increment the offset
			addr += increment;
		}
	}

	//Sanity check that we haven't overflowed available RAM
	uint8_t* ramEnd = (uint8_t*)SDRAM_ADDR + SDRAM_SIZE;
	assert(addr < ramEnd);
}

void recorder_init() {
	//First, gather all classes
	recorders[0].info = &recorder_class_iq;

	//Setup buffer memory
	setup_recorder_buffers();

	//Setup all recorders
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		//Clear state
		recorders[i].state = RECORDER_STATE_IDLE;
		recorders[i].received_samples = 0;

		//Prepare class
		recorders[i].info->init_cb(&recorders[i].setup);
	}

	//Finally, start capture on all
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++)
		recorders[i].info->start_cb();
}

// Requests that a recorder at index begins recording. Interrupt safe.
void recorder_request_start(int index) {
	recorder_start_flags |= 1U << index;
}

// Immediately starts a recorder. Should be done in worker.
static void recorder_start(int i) {
	//Check if this recorder is already active
	if (recorders[i].state != RECORDER_STATE_IDLE)
		return;

	//Attempt to open a file for this
	if (!recorder_handler_begin(i, &recorders[i].file))
		return;

	//Set state
	recorders[i].state = RECORDER_STATE_RECORDING;
	recorders[i].received_samples = 0;
	recorders[i].setup.dropped_samples = 0;

	//TODO: Do something to mark samples already in the DMA buffer queue
}

// Immediately stops a recorder. Should be done in worker.
static void recorder_stop(int index, int code) {
	//Set state
	recorders[index].state = RECORDER_STATE_STOPPING;

	//Send user notification
	recorder_handler_stop(index, &recorders[index].file, code);

	//Set state
	recorders[index].state = RECORDER_STATE_IDLE;
}

// Should be called in processing loop. Handles events.
void recorder_tick() {
	//Check start flags to begin recording
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		if (recorder_start_flags & (1U << i)) {
			recorder_start_flags &= ~(1U << i);
			recorder_start(i);
		}
	}

	//Tick all active recorders
	int code = RECORDER_TICK_STATUS_OK;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		if (recorders[i].state == RECORDER_STATE_RECORDING) {
			//Check if the current buffer is available to be written to disk
			if (recorders[i].setup.buffers[recorders[i].output_buffer_index].state == 0xFF) {
				//Write
				UINT written;
				if (f_write(&recorders[i].file, recorders[i].setup.buffers[recorders[i].output_buffer_index].buffer, recorders[i].info->input_bytes_per_sample * RECORDER_BUFFER_SIZE, &written) != FR_OK)
					code = RECORDER_TICK_STATUS_IO_ERR;

				//Update statistics
				recorders[i].received_samples += RECORDER_BUFFER_SIZE;

				//Mark as free and advance cursor
				recorders[i].setup.buffers[recorders[i].output_buffer_index].state = 0;
				recorders[i].output_buffer_index = (recorders[i].output_buffer_index + 1) % recorders[i].setup.buffer_count;
			}

			//Check for errors
			if (code != RECORDER_TICK_STATUS_OK)
				recorder_stop(i, code);

			//TEST
			if (recorders[i].received_samples >= 650026 * 60 * 3) {
				recorder_stop(i, 2);
			}
		}
	}
}

// Query info about an instance by index
void recorder_query_instance_info(int index, recorder_class_t* info, uint8_t* state, uint64_t* received_samples, uint64_t* dropped_samples) {
	(*info) = *recorders[index].info;
	(*state) = recorders[index].state;
	(*received_samples) = recorders[index].received_samples;
	(*dropped_samples) = recorders[index].setup.dropped_samples;
}

/* USER STUBS */

// USER IMPLIMENTED - Called when a recorder starts. Output should be opened. Returns 1 on success, otherwise 0
__weak int recorder_handler_begin(int index, FIL* output) {
	UNUSED(index);
	UNUSED(output);
	return 0;
}

// USER IMPLIMENTED - Called when a recorder stops (either normally or with an error)
__weak void recorder_handler_stop(int index, FIL* output, int code) {
	UNUSED(index);
	UNUSED(output);
	UNUSED(code);
}
