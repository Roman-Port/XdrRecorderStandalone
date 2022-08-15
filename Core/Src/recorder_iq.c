#include "recorder.h"
#include "recorder/interlace.h"

static uint32_t iq_buffer[RECORDER_BUFFER_SIZE]; // Buffer we interlace into to write to the SD card
static int latest_file_buffer = 0; // Latest buffer index we got from the IO. This is also what is actively being interlaced.

static int iq_tick(FIL* file, rec_dmaio_t* io, uint64_t* samplesWritten) {
	//Check if the interlacing is completed
	int writeBuffer = 0;
	if (interlace_end()) {
		//Interlacing is done; mark the buffer as free
		rec_dmaio_free(io, latest_file_buffer);

		//Write the buffer to the card after other tick steps
		writeBuffer = 1;
	}

	//Attempt to pop a buffer from the DMA queue
	if (!interlace_busy() && rec_dmaio_pop(io, &latest_file_buffer)) {
		//Begin interlacing
		interlace_begin(
				(uint16_t*)io->buffers[latest_file_buffer].buffer_a,
				(uint16_t*)io->buffers[latest_file_buffer].buffer_b,
				iq_buffer,
				RECORDER_BUFFER_SIZE
		);
	}

	//Write the IQ buffer if desired
	UINT written;
	if (writeBuffer) {
		if (f_write(file, iq_buffer, sizeof(iq_buffer), &written) == FR_OK) {
			(*samplesWritten) += RECORDER_BUFFER_SIZE;
		} else {
			return RECORDER_TICK_STATUS_IO_ERR;
		}
	}

	return RECORDER_TICK_STATUS_OK;
}

static void iq_init() {
	//Enable SAI (starting the slave first)
	__HAL_SAI_ENABLE(&hsai_BlockB1);
	__HAL_SAI_ENABLE(&hsai_BlockA1);
}

void recorder_class_init_iq(recorder_class_t* cls) {

	//Configure DMA
	hsai_BlockA1.hdmarx->Instance->PAR = (uint32_t)&hsai_BlockA1.Instance->DR;
	hsai_BlockB1.hdmarx->Instance->PAR = (uint32_t)&hsai_BlockB1.Instance->DR;

	//Configure A
	__HAL_SAI_ENABLE_IT(&hsai_BlockB1, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET);
	hsai_BlockB1.Instance->CR1 |= SAI_xCR1_DMAEN;
	hsai_BlockB1.Instance->CR2 |= SAI_xCR2_FFLUSH;

	//Configure B
	__HAL_SAI_ENABLE_IT(&hsai_BlockA1, SAI_IT_OVRUDR | SAI_IT_AFSDET | SAI_IT_LFSDET);
	hsai_BlockA1.Instance->CR1 |= SAI_xCR1_DMAEN;
	hsai_BlockA1.Instance->CR2 |= SAI_xCR2_FFLUSH;

	//Setup
	cls->name = "Baseband";
	cls->dma_handle_a = hsai_BlockA1.hdmarx;
	cls->dma_handle_b = hsai_BlockB1.hdmarx;
	cls->input_bytes_per_sample = sizeof(uint16_t);
	cls->input_requires_slave = 1;
	cls->output_channels = 2;
	cls->output_bits_per_sample = 16;
	cls->output_sample_rate = 650026;
	cls->tick_cb = iq_tick;
	cls->init_cb = iq_init;
}
