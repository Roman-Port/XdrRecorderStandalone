#include "recorder.h"
#include "assert.h"
#include "recorder/dma2d_pipeline.h"

typedef struct {

	uint16_t buffer[RECORDER_BUFFER_SIZE];
	int dma_buffer_index;

} iq_working_buffer_t;

#define NEXTBUFFER_FLAG_DROP_CHECKED   1 /* Set if we've determined if it will be dropped or not */
#define NEXTBUFFER_FLAG_DROP           2 /* Set if this buffer is to be dropped */
#define NEXTBUFFER_FLAG_SPLIT_DMA_DONE 4 /* Set when the first one is completed. */

static recorder_setup_t* iq_setup = NULL;

static iq_working_buffer_t working_buffers[2];
static int current_working_buffer_index;

static int next_dma_buffer;
static int next_dma_buffer_flags;
static int current_dma_buffer;    // Buffer currently being transferred into

static void dma2d_completed(DMA2D_HandleTypeDef *hdma2d) {
	//Get index of the buffer
	int currentWorkingBuffer = !current_working_buffer_index;

	//Mark the buffer as complete
	iq_setup->buffers[working_buffers[currentWorkingBuffer].dma_buffer_index].state = 0xFF;
}

static void dma2d_error(DMA2D_HandleTypeDef *hdma2d) {
	abort();
}

// First or second half of circular buffer is complete
static void recorder_dma_completed(DMA_HandleTypeDef *hdma) {
	//Sanity check
	assert(next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP_CHECKED);

	//Check if both DMAs have finished
	if (next_dma_buffer_flags & NEXTBUFFER_FLAG_SPLIT_DMA_DONE) {
		//Change status of the block
		if (next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP) {
			//Count the buffer as dropped
			iq_setup->dropped_samples += RECORDER_BUFFER_SIZE;
		} else {
			//Make sure DMA2D isn't already busy
			if (hdma2d.Instance->CR & DMA2D_CR_START)
				abort();

			//Setup DMA2D to interlace the channels
			working_buffers[current_working_buffer_index].dma_buffer_index = current_dma_buffer;
			MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (RECORDER_BUFFER_SIZE | (1 << DMA2D_NLR_PL_Pos))); // Size
			WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)&((int16_t*)iq_setup->buffers[current_dma_buffer].buffer)[1]); // Destination
			WRITE_REG(hdma2d.Instance->OOR, (uint32_t)1); // Destination offset
			WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)working_buffers[current_working_buffer_index].buffer); // Source
			WRITE_REG(hdma2d.Instance->FGOR, (uint32_t)0); // Source offset

			//Enable interrupts
			__HAL_DMA2D_ENABLE_IT(&hdma2d, DMA2D_IT_TC | DMA2D_IT_TE | DMA2D_IT_CE);

			//Set callbacks
			hdma2d.XferCpltCallback = dma2d_completed;
			hdma2d.XferErrorCallback = dma2d_error;

			//Advance cursors
			current_dma_buffer = next_dma_buffer;
			current_working_buffer_index = !current_working_buffer_index;

			//Enable
			__HAL_DMA2D_ENABLE(&hdma2d);
		}

		//Reset flags for next
		next_dma_buffer_flags = 0;
	} else {
		//Mark for next
		next_dma_buffer_flags |= NEXTBUFFER_FLAG_SPLIT_DMA_DONE;
	}
}

// Determines the next DMA buffer to use and updates the state accordingly.
static inline void determine_next_dma_buffer() {
	//Check only once
	if (!(next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP_CHECKED)) {
		//Peek at the next buffer and see if it's ready or not
		int after = (current_dma_buffer + 1) % iq_setup->buffer_count;
		if (iq_setup->buffers[after].state == 0) {
			//Ideal state. We'll be good to queue up the next block
			next_dma_buffer = after;
		} else {
			//FUCK!! We didn't complete the write in time! We'll have to overwrite the data we just got in the current buffer while the file catches up
			next_dma_buffer_flags |= NEXTBUFFER_FLAG_DROP;
			next_dma_buffer = current_dma_buffer;
		}

		//Mark
		next_dma_buffer_flags |= NEXTBUFFER_FLAG_DROP_CHECKED;
	}
}

// First half of circular buffer is half full
static void recorder_dma_half_completed_a0(DMA_HandleTypeDef *hdma) {
	determine_next_dma_buffer();
	hdma->Instance->M1AR = (uint32_t)iq_setup->buffers[next_dma_buffer].buffer;
}

// Second half of circular buffer is half full
static void recorder_dma_half_completed_a1(DMA_HandleTypeDef *hdma) {
	determine_next_dma_buffer();
	hdma->Instance->M0AR = (uint32_t)iq_setup->buffers[next_dma_buffer].buffer;
}

// First half of circular buffer is half full
static void recorder_dma_half_completed_b0(DMA_HandleTypeDef *hdma) {
	determine_next_dma_buffer();
}

// Second half of circular buffer is half full
static void recorder_dma_half_completed_b1(DMA_HandleTypeDef *hdma) {
	determine_next_dma_buffer();
}

// Error in DMA
static void recorder_dma_error(DMA_HandleTypeDef *hdma) {
	int err = hdma->ErrorCode;
	abort();
}

static void setup_dma() {
	//Configure master DMA
	hsai_BlockA1.hdmarx->XferCpltCallback = recorder_dma_completed;
	hsai_BlockA1.hdmarx->XferM1CpltCallback = recorder_dma_completed;
	hsai_BlockA1.hdmarx->XferHalfCpltCallback = recorder_dma_half_completed_a0;
	hsai_BlockA1.hdmarx->XferM1HalfCpltCallback = recorder_dma_half_completed_a1;
	hsai_BlockA1.hdmarx->XferErrorCallback = recorder_dma_error;
	hsai_BlockA1.hdmarx->XferAbortCallback = NULL;
	hsai_BlockA1.hdmarx->Instance->NDTR = RECORDER_BUFFER_SIZE;
	hsai_BlockA1.hdmarx->Instance->CR &= ~DMA_SxCR_CT;
	hsai_BlockA1.hdmarx->Instance->CR  |= DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_SxCR_DBM | DMA_IT_HT;
	hsai_BlockA1.hdmarx->Instance->FCR |= DMA_IT_FE;
	hsai_BlockA1.hdmarx->Instance->PAR = (uint32_t)&hsai_BlockA1.Instance->DR;
	hsai_BlockA1.hdmarx->Instance->M0AR = (uint32_t)iq_setup->buffers[0].buffer;
	hsai_BlockA1.hdmarx->Instance->M1AR = (uint32_t)iq_setup->buffers[1].buffer;

	//Configure slave DMA
	hsai_BlockB1.hdmarx->XferCpltCallback = recorder_dma_completed;
	hsai_BlockB1.hdmarx->XferM1CpltCallback = recorder_dma_completed;
	hsai_BlockB1.hdmarx->XferHalfCpltCallback = recorder_dma_half_completed_b0;
	hsai_BlockB1.hdmarx->XferM1HalfCpltCallback = recorder_dma_half_completed_b1;
	hsai_BlockB1.hdmarx->XferErrorCallback = recorder_dma_error;
	hsai_BlockB1.hdmarx->XferAbortCallback = NULL;
	hsai_BlockB1.hdmarx->Instance->NDTR = RECORDER_BUFFER_SIZE;
	hsai_BlockB1.hdmarx->Instance->CR &= ~DMA_SxCR_CT;
	hsai_BlockB1.hdmarx->Instance->CR  |= DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_SxCR_DBM | DMA_IT_HT;
	hsai_BlockB1.hdmarx->Instance->FCR |= DMA_IT_FE;
	hsai_BlockB1.hdmarx->Instance->PAR = (uint32_t)&hsai_BlockB1.Instance->DR;
	hsai_BlockB1.hdmarx->Instance->M0AR = (uint32_t)working_buffers[0].buffer;
	hsai_BlockB1.hdmarx->Instance->M1AR = (uint32_t)working_buffers[1].buffer;

	//Reset current state
	current_dma_buffer = 0;
	next_dma_buffer = 0;
	next_dma_buffer_flags = 0;

	//Enable DMA
	__HAL_DMA_ENABLE(hsai_BlockA1.hdmarx);
	__HAL_DMA_ENABLE(hsai_BlockB1.hdmarx);
}

static void configure_sai(SAI_HandleTypeDef* hsai) {
	__HAL_SAI_ENABLE_IT(hsai, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET);
	hsai->Instance->CR1 |= SAI_xCR1_DMAEN;
	hsai->Instance->CR2 |= SAI_xCR2_FFLUSH;
}

static void prepare_transfers(recorder_setup_t* setup) {
	//Set
	iq_setup = setup;

	//Configure SAIs
	configure_sai(&hsai_BlockA1);
	configure_sai(&hsai_BlockB1);

	//Setup and begin DMA
	setup_dma();
}

static void begin_transfers() {
	//Enable SAIs
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
