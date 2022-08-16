#include "recorder.h"
#include "sdram.h"
#include <string.h>

typedef struct {

	recorder_class_t info;

	uint8_t state;
	uint64_t received_samples;

	FIL file;
	rec_dmaio_t io;

} recorder_instance_t;

static recorder_instance_t recorders[RECORDER_INSTANCES_COUNT];
static uint16_t recorder_start_flags = 0; // Each bit represents an index that we want to start capture from

static void setup_recorder_buffers() {
	//We'll now need to figure out how to divide up our available memory. We want to maximize the amount of time each
	//recorder will get to store. Ideally, all recorders will an equal amount of time, so we'll need to calculate this.

	//Determine total bytes/sec recorders will consume
	uint32_t totalBytesPerSec = 0;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++)
		totalBytesPerSec += recorders[i].info.input_bytes_per_sample * (recorders[i].info.input_requires_slave + 1) * recorders[i].info.output_sample_rate;

	//Calculate the number of seconds we can buffer for each recorder
	uint32_t bufferSecondsPerRecorder = SDRAM_SIZE / totalBytesPerSec;

	//Finally, we can set up memory for each buffer
	uint8_t* addr = (uint8_t*)SDRAM_ADDR;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		//Calculate the number of samples we'll be able to record
		uint32_t bufferSamples = bufferSecondsPerRecorder * recorders[i].info.output_sample_rate;

		//Calculate the number of buffers this is equivalent to
		uint32_t bufferCount = bufferSamples / RECORDER_BUFFER_SIZE / (recorders[i].info.input_requires_slave + 1);

		//Calculate increment
		uint32_t increment = recorders[i].info.input_bytes_per_sample * (recorders[i].info.input_requires_slave + 1) * RECORDER_BUFFER_SIZE;

		//Make sure we aren't overflowing
		assert(bufferCount <= REC_MAX_BUFFERS);

		//Set
		recorders[i].io.buffer_count = bufferCount;

		//Setup each buffer
		for (int b = 0; b < bufferCount; b++) {
			//Set A
			recorders[i].io.buffers[b].buffer_a = addr;
			memset(addr, 0, increment);
			addr += increment;

			//Set B (if applicable)
			if (recorders[i].info.input_requires_slave) {
				recorders[i].io.buffers[b].buffer_b = addr;
				memset(addr, 0, increment);
				addr += increment;
			}
		}
	}

	//Sanity check that we haven't overflowed available RAM
	uint8_t* ramEnd = (uint8_t*)SDRAM_ADDR + SDRAM_SIZE;
	assert(addr < ramEnd);
}

void recorder_init() {
	//First, set up all recorder classes
	recorder_class_init_iq(&recorders[0].info);

	//Setup buffer memory
	setup_recorder_buffers();

	//Setup all recorders
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		//Clear state
		recorders[i].state = RECORDER_STATE_IDLE;
		recorders[i].received_samples = 0;

		//Get transfer width of the DMA. If applicable, make sure both match
		int msize = (recorders[i].info.dma_handle_a->Instance->CR >> 13) & 0b11; //pg 329 of reference manual
		if (recorders[i].info.dma_handle_b != 0)
			assert(msize == ((recorders[i].info.dma_handle_b->Instance->CR >> 13) & 0b11));

		//Calculate bytes/transfer
		int transferBytes = 0;
		switch (msize) {
		case 0b00: transferBytes = 1; break;
		case 0b01: transferBytes = 2; break;
		case 0b10: transferBytes = 4; break;
		}

		//Calculate the number of DMA transfers needed to consume the same amount of bytes as we're buffering
		int bufferedBytes = recorders[i].info.input_bytes_per_sample * RECORDER_BUFFER_SIZE;
		assert((bufferedBytes % transferBytes) == 0);
		int transferSize = bufferedBytes / transferBytes;
		assert(transferSize < 65536); // 65535 is the maximum number of tranfers a DMA can do

		//Setup IO and begin capturing to buffers
		recorders[i].io.dma_handle_a = recorders[i].info.dma_handle_a;
		recorders[i].io.dma_handle_b = recorders[i].info.dma_handle_b;
		recorders[i].io.dropped_buffers = 0;
		recorders[i].io.received_buffers = 0;
		recorders[i].io.buffer_size = transferSize;
		rec_dmaio_init(&recorders[i].io);

		//Initialize hardware
		recorders[i].info.init_cb();
	}
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
	recorders[i].io.dropped_buffers = 0;

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
	int code;
	for (int i = 0; i < RECORDER_INSTANCES_COUNT; i++) {
		if (recorders[i].state == RECORDER_STATE_RECORDING) {
			//Tick
			code = recorders[i].info.tick_cb(
					&recorders[i].file,
					&recorders[i].io,
					&recorders[i].received_samples
			);

			//Check for errors
			if (code != RECORDER_TICK_STATUS_OK)
				recorder_stop(i, code);

			//TEST
			if (recorders[i].received_samples >= 650026 * 60 * 5) {
				recorder_stop(i, 2);
			}
		}
	}
}

// Query info about an instance by index
void recorder_query_instance_info(int index, recorder_class_t* info, uint8_t* state, uint64_t* received_samples, uint64_t* dropped_buffers) {
	(*info) = recorders[index].info;
	(*state) = recorders[index].state;
	(*received_samples) = recorders[index].received_samples;
	(*dropped_buffers) = recorders[index].io.dropped_buffers;
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
