#include "recorder/dmaio.h"
#include <assert.h>

#define BUFFER_STATUS_AVAILABLE 0
#define BUFFER_STATUS_FULL      1

#define NEXTBUFFER_FLAG_DROP_CHECKED   1 /* Set if we've determined if it will be dropped or not */
#define NEXTBUFFER_FLAG_DROP           2 /* Set if this buffer is to be dropped */
#define NEXTBUFFER_FLAG_SPLIT_DMA_DONE 4 /* Only used if there are two DMAs. Set when the first one is completed. */

/* UTIL */

// Determines the next DMA buffer to use and updates the state accordingly. Returns the bank index to use next.
static inline void determine_next_dma_buffer(rec_dmaio_t* ctx) {
	//Check only once
	if (!(ctx->_next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP_CHECKED)) {
		//Peek at the next buffer and see if it's ready or not
		int after = (ctx->_current_dma_buffer + 1) % ctx->buffer_count;
		if (ctx->buffers[after].status == BUFFER_STATUS_AVAILABLE) {
			//Ideal state. We'll be good to queue up the next block
			ctx->_next_dma_buffer = after;
		} else {
			//FUCK!! We didn't complete the write in time! We'll have to overwrite the data we just got in the current buffer while the file catches up
			ctx->_next_dma_buffer_flags |= NEXTBUFFER_FLAG_DROP;
			ctx->_next_dma_buffer = ctx->_current_dma_buffer;
		}

		//Mark
		ctx->_next_dma_buffer_flags |= NEXTBUFFER_FLAG_DROP_CHECKED;
	}
}

/* CALLBACKS */

// First or second half of circular buffer is complete
static void recorder_dma_completed(DMA_HandleTypeDef *hdma) {
	//Get context
	rec_dmaio_t* ctx = (rec_dmaio_t*)hdma->Parent;

	//Sanity check
	assert(ctx->_next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP_CHECKED);

	//Check if we're either the only DMA or if the other has finished
	if (ctx->dma_handle_b == 0 || (ctx->_next_dma_buffer_flags & NEXTBUFFER_FLAG_SPLIT_DMA_DONE)) {
		//Change status of the block
		if (ctx->_next_dma_buffer_flags & NEXTBUFFER_FLAG_DROP) {
			//Count the buffer as dropped
			ctx->dropped_buffers++;
		} else {
			//We are not overriding ourselves (as we would if a buffer had to be dropped) so mark the active buffer as complete and advance
			ctx->buffers[ctx->_current_dma_buffer].status = BUFFER_STATUS_FULL;
			ctx->_current_dma_buffer = ctx->_next_dma_buffer;
			ctx->received_buffers++;
		}

		//Reset flags for next
		ctx->_next_dma_buffer_flags = 0;
	} else {
		//Mark for next
		ctx->_next_dma_buffer_flags |= NEXTBUFFER_FLAG_SPLIT_DMA_DONE;
	}
}

// First half of circular buffer is half full
static void recorder_dma_half_completed_a0(DMA_HandleTypeDef *hdma) {
	//Get context
	rec_dmaio_t* ctx = (rec_dmaio_t*)hdma->Parent;

	//Determine other DMA buffer
	determine_next_dma_buffer(ctx);

	//Swap other DMA buffer
	hdma->Instance->M1AR = (uint32_t)ctx->buffers[ctx->_next_dma_buffer].buffer_a;
}

// Second half of circular buffer is half full
static void recorder_dma_half_completed_a1(DMA_HandleTypeDef *hdma) {
	//Get context
	rec_dmaio_t* ctx = (rec_dmaio_t*)hdma->Parent;

	//Determine other DMA buffer
	determine_next_dma_buffer(ctx);

	//Swap other DMA buffer
	hdma->Instance->M0AR = (uint32_t)ctx->buffers[ctx->_next_dma_buffer].buffer_a;
}

// First half of circular buffer is half full
static void recorder_dma_half_completed_b0(DMA_HandleTypeDef *hdma) {
	//Get context
	rec_dmaio_t* ctx = (rec_dmaio_t*)hdma->Parent;

	//Determine other DMA buffer
	determine_next_dma_buffer(ctx);

	//Swap other DMA buffer
	hdma->Instance->M1AR = (uint32_t)ctx->buffers[ctx->_next_dma_buffer].buffer_b;
}

// Second half of circular buffer is half full
static void recorder_dma_half_completed_b1(DMA_HandleTypeDef *hdma) {
	//Get context
	rec_dmaio_t* ctx = (rec_dmaio_t*)hdma->Parent;

	//Determine other DMA buffer
	determine_next_dma_buffer(ctx);

	//Swap other DMA buffer
	hdma->Instance->M0AR = (uint32_t)ctx->buffers[ctx->_next_dma_buffer].buffer_b;
}

// Error in DMA
static void recorder_dma_error(DMA_HandleTypeDef *hdma) {
	int err = hdma->ErrorCode;
	abort();
}

/* API */

void rec_dmaio_init(rec_dmaio_t* ctx) {
	//Configure master DMA
	ctx->dma_handle_a->XferCpltCallback = recorder_dma_completed;
	ctx->dma_handle_a->XferM1CpltCallback = recorder_dma_completed;
	ctx->dma_handle_a->XferHalfCpltCallback = recorder_dma_half_completed_a0;
	ctx->dma_handle_a->XferM1HalfCpltCallback = recorder_dma_half_completed_a1;
	ctx->dma_handle_a->XferErrorCallback = recorder_dma_error;
	ctx->dma_handle_a->XferAbortCallback = NULL;
	ctx->dma_handle_a->Instance->NDTR = ctx->buffer_size;
	ctx->dma_handle_a->Instance->CR &= ~DMA_SxCR_CT;
	ctx->dma_handle_a->Instance->CR  |= DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_SxCR_DBM | DMA_IT_HT;
	ctx->dma_handle_a->Instance->FCR |= DMA_IT_FE;
	ctx->dma_handle_a->Parent = ctx;
	ctx->dma_handle_a->Instance->M0AR = (uint32_t)ctx->buffers[0].buffer_a;
	ctx->dma_handle_a->Instance->M1AR = (uint32_t)ctx->buffers[1].buffer_a;

	//Configure secondary DMA if it's set
	if (ctx->dma_handle_b != 0)
	{
		ctx->dma_handle_b->XferCpltCallback = recorder_dma_completed;
		ctx->dma_handle_b->XferM1CpltCallback = recorder_dma_completed;
		ctx->dma_handle_b->XferHalfCpltCallback = recorder_dma_half_completed_b0;
		ctx->dma_handle_b->XferM1HalfCpltCallback = recorder_dma_half_completed_b1;
		ctx->dma_handle_b->XferErrorCallback = recorder_dma_error;
		ctx->dma_handle_b->XferAbortCallback = NULL;
		ctx->dma_handle_b->Instance->NDTR = ctx->buffer_size;
		ctx->dma_handle_b->Instance->CR &= ~DMA_SxCR_CT;
		ctx->dma_handle_b->Instance->CR  |= DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_SxCR_DBM | DMA_IT_HT;
		ctx->dma_handle_b->Instance->FCR |= DMA_IT_FE;
		ctx->dma_handle_b->Parent = ctx;
		ctx->dma_handle_b->Instance->M0AR = (uint32_t)ctx->buffers[0].buffer_b;
		ctx->dma_handle_b->Instance->M1AR = (uint32_t)ctx->buffers[1].buffer_b;
	}

	//Reset state of buffers
	for (int i = 0; i < ctx->buffer_count; i++)
		ctx->buffers[i].status = BUFFER_STATUS_AVAILABLE;

	//Reset current state
	ctx->_current_dma_buffer = 0;
	ctx->_next_dma_buffer = 0;
	ctx->_next_dma_buffer_flags = 0;
	ctx->_current_reading_buffer = 0;

	//Enable DMA
	__HAL_DMA_ENABLE(ctx->dma_handle_a);
	if (ctx->dma_handle_b != 0)
		__HAL_DMA_ENABLE(ctx->dma_handle_b);
}

void rec_dmaio_destroy(rec_dmaio_t* ctx) {
	//Disable
	__HAL_DMA_DISABLE(ctx->dma_handle_a);
	if (ctx->dma_handle_b != 0)
		__HAL_DMA_DISABLE(ctx->dma_handle_b);
}

int rec_dmaio_pop(rec_dmaio_t* ctx, int* index) {
	//Check if it is available
	if (ctx->buffers[ctx->_current_reading_buffer].status == BUFFER_STATUS_FULL) {
		//Set output
		(*index) = ctx->_current_reading_buffer;

		//Increment
		ctx->_current_reading_buffer = (ctx->_current_reading_buffer + 1) % ctx->buffer_count;

		return 1;
	}

	return 0;
}

void rec_dmaio_free(rec_dmaio_t* ctx, int index) {
	//Mark buffer as available
	ctx->buffers[index].status = BUFFER_STATUS_AVAILABLE;
}
