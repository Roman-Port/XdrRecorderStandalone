#include "recorder.h"

#define TRANSFERS_PER_BLOCK 4
#define SAMPLES_PER_BLOCK 8 /* Depends on FIFO transfer percentage */

typedef struct {

	int16_t* output;
	int remaining_blocks;      //-1 has a special meaning. It indicates that we've advanced to the next buffer but we're waiting for it to become free
	int current_buffer_index;

} transfer_block_t;

static recorder_setup_t* iq_setup = NULL;

static transfer_block_t block_data_a;
static transfer_block_t block_data_b;

static inline void transfer_block(SAI_HandleTypeDef* hsai, transfer_block_t* block, uint8_t bufferMask, int bufferIndexOffset) {
	//Check if we've finished this buffer
	if (block->remaining_blocks == 0) {
		//Mark this buffer as complete
		iq_setup->buffers[block->current_buffer_index].state |= bufferMask;

		//Step forward to next buffer
		block->current_buffer_index = (block->current_buffer_index + 1) % iq_setup->buffer_count;

		//Set state
		block->remaining_blocks = -1;
	}

	//Check if we're waiting for the next buffer to become free and it has
	if (block->remaining_blocks == -1 && iq_setup->buffers[block->current_buffer_index].state == 0) {
		block->output = ((int16_t*)iq_setup->buffers[block->current_buffer_index].buffer) + bufferIndexOffset;
		block->remaining_blocks = RECORDER_BUFFER_SIZE / 2 / SAMPLES_PER_BLOCK; //verify
	}

	//Write samples
	if (block->remaining_blocks > 0) {
		//Transfer to the output
		int16_t temp[2];
		for (int i = 0; i < TRANSFERS_PER_BLOCK; i++) {
			*((int32_t*)temp) = hsai->Instance->DR;
			block->output[0] = temp[1];
			block->output[2] = temp[0];
			block->output += 4;
		}
		block->remaining_blocks--;
	} else {
		//Read from the data register to advance the FIFO, but just discard the result and count it towards dropped samples
		uint32_t nothingnessOfSpace;
		for (int i = 0; i < TRANSFERS_PER_BLOCK; i++) {
			nothingnessOfSpace = hsai->Instance->DR;
			iq_setup->dropped_samples++; // BUG: This'll advance twice as fast as it should
		}
	}
}

static void transfer_block_a(SAI_HandleTypeDef* hsai) {
	transfer_block(hsai, &block_data_a, 0x0F, 0);
}

static void transfer_block_b(SAI_HandleTypeDef* hsai) {
	transfer_block(hsai, &block_data_b, 0xF0, 1);
}

typedef void (*block_transfer_cb)(SAI_HandleTypeDef* hsai);
static void prepare_transfer_block(SAI_HandleTypeDef* hsai, transfer_block_t* block, block_transfer_cb transferCb) {
	//Reset the block data
	block->current_buffer_index = 0;
	block->output = 0;
	block->remaining_blocks = -1;

	//Configure the SAI
	__HAL_SAI_ENABLE_IT(hsai, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET | SAI_IT_FREQ);
	hsai->InterruptServiceRoutine = transferCb;
	hsai->Instance->CR2 |= SAI_xCR2_FFLUSH;
}

static void prepare_transfers(recorder_setup_t* setup) {
	iq_setup = setup;
	prepare_transfer_block(&hsai_BlockA1, &block_data_a, transfer_block_a);
	prepare_transfer_block(&hsai_BlockB1, &block_data_b, transfer_block_b);
}

static void begin_transfers() {
	//Start SAIs (starting with B, since it depends on A's clock)
	__HAL_SAI_ENABLE(&hsai_BlockB1);
	__HAL_SAI_ENABLE(&hsai_BlockA1);
}

static void stop_transfer_block(SAI_HandleTypeDef* hsai) {
	__HAL_SAI_DISABLE_IT(hsai, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET | SAI_IT_FREQ);
	__HAL_SAI_CLEAR_FLAG(hsai, SAI_FLAG_OVRUDR);
	__HAL_SAI_DISABLE(hsai);
}

static void stop_transfers() {
	//Stop SAIs (starting with A, as B depends on it's clock)
	stop_transfer_block(&hsai_BlockA1);
	stop_transfer_block(&hsai_BlockB1);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
	abort();
}

// Class for recorder

const recorder_class_t recorder_class_iq = {
		.name = "Baseband",
		.input_bytes_per_sample = 4,
		.output_channels = 2,
		.output_bits_per_sample = 16,
		.output_sample_rate = 650026,
		.init_cb = prepare_transfers,
		.start_cb = begin_transfers,
		.stop_cb = stop_transfers
};
