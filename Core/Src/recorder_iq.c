#include "recorder.h"
#include "assert.h"

static recorder_setup_t* iq_setup = NULL;

static int transfer_buffer_index = 0;
static int transfer_buffer_flags = 0;

static inline void begin_dma_transfer() {
	//Clear flags
	transfer_buffer_flags = 0;

	//Determine next buffer address
	int16_t* addr = (int16_t*)iq_setup->buffers[transfer_buffer_index].buffer;

	//Begin DMAs transfer to new buffer
	HAL_DMA_Start_IT(hsai_BlockA1.hdmarx, (uint32_t)&hsai_BlockA1.Instance->DR, (uint32_t)addr, RECORDER_BUFFER_SIZE/2);
	HAL_DMA_Start_IT(hsai_BlockB1.hdmarx, (uint32_t)&hsai_BlockB1.Instance->DR, (uint32_t)&addr[RECORDER_BUFFER_SIZE], RECORDER_BUFFER_SIZE/2);
}

static inline void dma_transfer_completed(int flagsMask) {
	//Mark in flags
	transfer_buffer_flags |= flagsMask;

	//Check if both are done
	if ((transfer_buffer_flags & 0b11) == 0b11) {
		//Mark this buffer as complete
		iq_setup->buffers[transfer_buffer_index].state = 0xFF;

		//Step cursor forward to next buffer
		transfer_buffer_index = (transfer_buffer_index + 1) % iq_setup->buffer_count;

		//Make sure it is available
		if (iq_setup->buffers[transfer_buffer_index].state == 0) {
			//Start new DMA transfer
			begin_dma_transfer();
		} else {
			//TODO
			abort();
		}
	}
}

static void transfer_block_a(DMA_HandleTypeDef* dma) {
	dma_transfer_completed(0b01);
}

static void transfer_block_b(DMA_HandleTypeDef* dma) {
	dma_transfer_completed(0b10);
}

typedef void (*block_transfer_cb)(DMA_HandleTypeDef* hsai);
static void prepare_transfer_block(SAI_HandleTypeDef* hsai, block_transfer_cb transferCb) {
	//Configure the SAI
	__HAL_SAI_ENABLE_IT(hsai, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET);
	hsai->Instance->CR1 |= SAI_xCR1_DMAEN;
	hsai->Instance->CR2 |= SAI_xCR2_FFLUSH;

	//Configure DMA
	hsai->hdmarx->XferCpltCallback = transferCb;
}

static void prepare_transfers(recorder_setup_t* setup) {
	//Set
	iq_setup = setup;

	//Configure
	prepare_transfer_block(&hsai_BlockA1, transfer_block_a);
	prepare_transfer_block(&hsai_BlockB1, transfer_block_b);

	//Begin DMA transfers
	begin_dma_transfer();
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
		.output_channels = 3, //test
		.output_bits_per_sample = 16,
		.output_sample_rate = 650026,
		.init_cb = prepare_transfers,
		.start_cb = begin_transfers,
		.stop_cb = stop_transfers
};
