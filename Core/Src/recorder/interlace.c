#include "recorder/interlace.h"
#include "main.h"

#define STATUS_IDLE   0 // Ready for next transfer
#define STATUS_STAGE1 1 // Transferring buffer 1
#define STATUS_STAGE2 2 // Transferring buffer 2
#define STATUS_DONE   3 // Buffer is completed. Automatically cleared by interlace_end

static uint8_t status = STATUS_IDLE;
static uint16_t* stage2_buffer;

static void interlace_dma_error(DMA2D_HandleTypeDef *hdma2d) {
	abort();
}

static void interlace_dma_completed_stage2(DMA2D_HandleTypeDef *hdma2d) {
	//Finished!
	status = STATUS_DONE;
}

static void interlace_dma_completed_stage1(DMA2D_HandleTypeDef *hdma2d) {
	//First buffer is completed. Setup state
	status = STATUS_STAGE2;

	//Set source adddress
	WRITE_REG(hdma2d->Instance->FGMAR, (uint32_t)stage2_buffer);

	//Increment destination address by two bytes to write the other "column"
	hdma2d->Instance->OMAR += sizeof(uint16_t);

	//Enable interrupts
	__HAL_DMA2D_ENABLE_IT(hdma2d, DMA2D_IT_TC | DMA2D_IT_TE | DMA2D_IT_CE);

	//Enable
	hdma2d->XferCpltCallback = interlace_dma_completed_stage2;
	hdma2d->XferErrorCallback = interlace_dma_error;
	__HAL_DMA2D_ENABLE(hdma2d);
}

// Starts the interlace process. Interlaces A and B into output. Returns 1 if it was successfully started, otherwise 0.
int interlace_begin(uint16_t* a, uint16_t* b, uint32_t* output, uint16_t len) {
	//Make sure we're not busy
	if (status != STATUS_IDLE)
		return 0;

	//Set state
	status = STATUS_STAGE1;
	stage2_buffer = b;

	//We use the 2D DMA hardware for this. As far as it's concerned, it's rendering two "images" of size 1x<len> to a canvas of 2x<len>. Janky but it works.
	//We need to do this in two passes. First, set up the "canvas" it's rendering to
	MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (len | (1 << DMA2D_NLR_PL_Pos))); // Size
	WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)output); // Destination

	//Set source adddress
	WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)a);

	//Enable interrupts
	__HAL_DMA2D_ENABLE_IT(&hdma2d, DMA2D_IT_TC | DMA2D_IT_TE | DMA2D_IT_CE);

	//Enable
	hdma2d.XferCpltCallback = interlace_dma_completed_stage1;
	hdma2d.XferErrorCallback = interlace_dma_error;
	__HAL_DMA2D_ENABLE(&hdma2d);

	return 1;
}

// Checks if the interlacing hardware is busy. If it is, 1 is returned. If it's free, 0 is returned.
int interlace_busy() {
	return status != STATUS_IDLE;
}

// Checks the status of the interlacing process. If it's done, returns 1. If it's still working, returns 0.
int interlace_end() {
	if (status == STATUS_DONE) {
		status = STATUS_IDLE;
		return 1;
	}
	return 0;
}
