#include "recorder/wav.h"

static void set_marker_chars(char output[4], const char* name) {
	for (int i = 0; i < 4; i++)
		output[i] = name[i];
}

// Fills in WAV header with all required values.
void wav_init_header(wav_file_header_t* header, uint16_t channels, uint16_t bits_per_sample, uint32_t sample_rate) {
	//Write
	set_marker_chars(header->riff.marker, "RIFF");
	header->riff.len = 0;
	set_marker_chars(header->file_type, "WAVE");
	set_marker_chars(header->fmt.marker, "fmt ");
	header->fmt.len = 16;
	header->format = 1; //PCM
	header->channels = channels;
	header->sample_rate = sample_rate;
	header->bytes_per_sec = (sample_rate * bits_per_sample * channels) / 8;
	header->bytes_per_sample_pair = (bits_per_sample * channels) / 8;
	header->bits_per_sample = bits_per_sample;
	set_marker_chars(header->data.marker, "data");
	header->data.len = 0;

	//Calculate lengths
	wav_calculate_length(header, 0);
}

// Used for writing to the length in a WAV header. If the length is under the maximum value an int32 can store, it's written unchanged. If it's greater, however, -1 is always returned.
static int32_t pack_length(uint64_t len) {
	if (len <= 2147483647)
		return len;
	return -1;
}

// Calculates and applies file length. samples_written is the number of total samples (or for stereo, sample pairs) written
void wav_calculate_length(wav_file_header_t* header, uint64_t samples_written) {
	//Calculate data length
	uint64_t dataLen = samples_written * header->bytes_per_sample_pair;

	//Set
	header->data.len = pack_length(dataLen);
	header->riff.len = pack_length(dataLen + sizeof(wav_file_header_t) - 8);
}
