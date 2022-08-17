#include "recorder/dma2d_pipeline.h"
#include "main.h"

#define STATUS_IDLE 0
#define STATUS_BUSY 1

static int step_count;
static const dma2d_pipeline_step_t* step_data;
static dma2d_pipeline_completed_cb completed_cb;

static int step_index;
static int status;

static void start_dma(const dma2d_pipeline_step_t* info);

static void dma_completed(DMA2D_HandleTypeDef *hdma2d) {
	//Increment the index
	step_index++;

	//Check if we've reached the end
	if (step_index >= step_count) {
		//Finished!
		status = STATUS_IDLE;
		completed_cb();
	} else {
		//Begin this step
		start_dma(&step_data[step_index]);
	}
}

static void dma_error(DMA2D_HandleTypeDef *hdma2d) {
	abort();
}

static void start_dma(const dma2d_pipeline_step_t* info) {
	//Configure
	MODIFY_REG(hdma2d.Instance->NLR, (DMA2D_NLR_NL | DMA2D_NLR_PL), (info->transfers | (1 << DMA2D_NLR_PL_Pos))); // Size
	WRITE_REG(hdma2d.Instance->OMAR, (uint32_t)info->dst); // Destination
	WRITE_REG(hdma2d.Instance->OOR, (uint32_t)info->dst_skip); // Destination offset
	WRITE_REG(hdma2d.Instance->FGMAR, (uint32_t)info->src); // Source
	WRITE_REG(hdma2d.Instance->FGOR, (uint32_t)info->src_skip); // Source offset

	//Enable interrupts
	__HAL_DMA2D_ENABLE_IT(&hdma2d, DMA2D_IT_TC | DMA2D_IT_TE | DMA2D_IT_CE);

	//Set callbacks
	hdma2d.XferCpltCallback = dma_completed;
	hdma2d.XferErrorCallback = dma_error;

	//Enable
	__HAL_DMA2D_ENABLE(&hdma2d);
}

// Starts the interlace process. Interlaces A and B into output. Returns 1 if it was successfully started, otherwise 0.
int dma2d_pipeline_begin(const dma2d_pipeline_step_t* steps, int stepCount, dma2d_pipeline_completed_cb completedCb) {
	//Make sure we're not busy
	if (status != STATUS_IDLE)
		return 0;

	//Set state
	step_count = stepCount;
	step_data = steps;
	completed_cb = completedCb;
	step_index = 0;
	status = STATUS_BUSY;

	//Begin first DMA
	start_dma(&step_data[0]);

	return 1;
}
