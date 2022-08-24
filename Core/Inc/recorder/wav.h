#ifndef INC_RECORDER_WAV_H_
#define INC_RECORDER_WAV_H_

#include <stdint.h>

typedef struct {
	char marker[4];
	int32_t len;
} wav_file_segment_t;

typedef struct {
	wav_file_segment_t riff;
	char file_type[4];
	wav_file_segment_t fmt;
	uint16_t format;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t bytes_per_sec;
	uint16_t bytes_per_sample_pair; // (bits_per_sample * channels) / 8
	uint16_t bits_per_sample;
	wav_file_segment_t data;
} wav_file_header_t;

// Fills in WAV header with all required values.
void wav_init_header(wav_file_header_t* header, uint16_t channels, uint16_t bits_per_sample, uint32_t sample_rate);

// Calculates and applies file length. samples_written is the number of total samples (or for stereo, sample pairs) written
void wav_calculate_length(wav_file_header_t* header, uint64_t samples_written);


#endif /* INC_RECORDER_WAV_H_ */
