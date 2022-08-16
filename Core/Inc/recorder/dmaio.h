#ifndef INC_RECORDER_DMAIO_H_
#define INC_RECORDER_DMAIO_H_

#include "stm32f4xx_hal.h"

#define REC_MAX_BUFFERS 256

typedef struct {

	void* buffer_a; // Required. Output buffer.
	void* buffer_b; // Required only if the slave DMA is set. Output buffer.

	// PRIVATE

	uint8_t status;

} rec_dmaio_buffer_t;

typedef struct {

	// USER SET VARS

	DMA_HandleTypeDef* dma_handle_a; // Required
	DMA_HandleTypeDef* dma_handle_b; // Optional

	uint16_t buffer_count; // Number of available buffers
	uint16_t buffer_size;  // Size of each buffer, in elements
	rec_dmaio_buffer_t buffers[REC_MAX_BUFFERS];

	uint64_t dropped_buffers;
	uint64_t received_buffers;

	// PRIVATE

	uint8_t _current_dma_buffer;     // Buffer currently being written to via DMA
	uint8_t _next_dma_buffer;        // Buffer that will be written to next
	uint8_t _next_dma_buffer_flags;  // Status of the next DMA buffer
	uint8_t _current_reading_buffer; // Buffer index that is next to be read with rec_dmaio_pop

} rec_dmaio_t;

// Initializes the struct and prepares for reading. Begins DMA transfers
void rec_dmaio_init(rec_dmaio_t* ctx);

// Dequeues the latest buffer. Returns 1 if successful, otherwise 0. Sets buffer to the address of the buffer and index to the index of it.
int rec_dmaio_pop(rec_dmaio_t* ctx, int* index);

// Marks the specified buffer index as available. Should be called after the buffers used by rec_dmaio_pop are consumed.
void rec_dmaio_free(rec_dmaio_t* ctx, int index);

// Stops DMA transfers
void rec_dmaio_destroy(rec_dmaio_t* ctx);

#endif /* INC_RECORDER_DMAIO_H_ */
