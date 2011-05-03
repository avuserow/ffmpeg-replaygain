#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "gain_analysis.h"
#include <stdio.h>
#include <math.h>

int process_file(char* filename) {
	AVFormatContext *pFormatCtx;
	if (av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL) != 0) {
		fprintf(stderr, "could not open file %s\n", filename);
		return 1;
	}

	if (av_find_stream_info(pFormatCtx) < 0) {
		fprintf(stderr, "Couldn't find stream information for %s\n",
				filename);
		return 1;
	}
	//dump_format(pFormatCtx, 0, filename, 0);


	// Find the first video stream
	int audio_stream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			audio_stream = i;
			break;
		}
	}
	if (audio_stream == -1) {
		fprintf(stderr, "could not find audio stream in %s\n", filename);
		return 1;
	}

	// Get a pointer to the codec context for the audio stream
	AVCodecContext *pCodecCtx = pFormatCtx->streams[audio_stream]->codec;

	// Find the decoder for the stream
	AVCodec* pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec for file %s!\n", filename);
		return 1; // Codec not found
	}
	// Open codec
	if (avcodec_open(pCodecCtx, pCodec) < 0) {
		fprintf(stderr, "could not open codec on file %s\n", filename);
		return 1; // Could not open codec
	}

	fprintf(stderr, "note: %d audio channels; %d sample_rate \n",
			pCodecCtx->channels, pCodecCtx->sample_rate);
	InitGainAnalysis(pCodecCtx->sample_rate);

	AVPacket packet;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		if (packet.stream_index == audio_stream) {
			int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
			uint8_t *samples = malloc(out_size);
			Float_t adjustedSamples[out_size];
			avcodec_decode_audio3(pCodecCtx, (int8_t*)samples, &out_size,
					&packet);

			for (int i = 0; i < out_size; i++) {
				adjustedSamples[i] = samples[i];
			}
			for (int i = 0; i < out_size; i++) {
				//fprintf(stderr, "%d, %f\n", i, samples[i]);
			}
			//exit(0);

			if (AnalyzeSamples(adjustedSamples, adjustedSamples, out_size, pCodecCtx->channels)
					!= GAIN_ANALYSIS_OK) {
				printf("!!! AnalyzeSamples: Not OK!\n");
			}

			free(samples);
		}
		av_free_packet(&packet);
	}

	// TODO: clipping
	
	float otherGain = GetTitleGain();
	printf("titleGain: %+6.2f, file: %s\n", otherGain, filename);

	return 0;
}

int main(int argc, char* argv[]) {
	av_register_all();

	int bad_files = 0;
	for (int i = 1; i < argc; i++) {
		bad_files += process_file(argv[i]);
	}

	return bad_files;
}
