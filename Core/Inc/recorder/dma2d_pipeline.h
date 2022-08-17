#ifndef INC_RECORDER_DMA2D_PIPELINE_H_
#define INC_RECORDER_DMA2D_PIPELINE_H_

typedef void (*dma2d_pipeline_completed_cb)();

typedef struct {

	void* src;
	void* dst;

	int transfers;

	int src_skip;
	int dst_skip;

} dma2d_pipeline_step_t;

int dma2d_pipeline_begin(const dma2d_pipeline_step_t* steps, int stepCount, dma2d_pipeline_completed_cb completedCb);

#endif
