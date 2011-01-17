#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "gain_analysis.h"
#include <stdio.h>
#include <math.h>

/* 
An ANSI C implementation of MATLAB FILTER.M (built-in)
Written by Chen Yangquan <elecyq@nus.edu.sg>
1998-11-11
*/

void filter(int ord, float *a, float *b, int np, float *x, float *y)
{
	int i,j;
	y[0]=b[0]*x[0];
	for (i=1;i<ord+1;i++)
	{
		y[i]=0.0;
		for (j=0;j<i+1;j++)
			y[i]=y[i]+b[j]*x[i-j];
		for (j=0;j<i;j++)
			y[i]=y[i]-a[j+1]*y[i-j-1];
	}
	/* end of initial part */
	for (i=ord+1;i<np+1;i++)
	{
		y[i]=0.0;
		for (j=0;j<ord+1;j++)
			y[i]=y[i]+b[j]*x[i-j];
		for (j=0;j<ord;j++)
			y[i]=y[i]-a[j+1]*y[i-j-1];
	}
} /* end of filter */

// from http://replaygain.hydrogenaudio.org/equal_loud_coef.txt
int yulewalk_filter_count = 11;
float yulewalk_filter_a[] = {
	1.00000000000000,
	-3.47845948550071,
	6.36317777566148,
	-8.54751527471874,
	9.47693607801280,
	-8.81498681370155,
	6.85401540936998,
	-4.39470996079559,
	2.19611684890774,
	-0.75104302451432,
	0.13149317958808
};

float yulewalk_filter_b[] = {
	0.05418656406430,
	-0.02911007808948,
	-0.00848709379851,
	-0.00851165645469,
	-0.00834990904936,
	0.02245293253339,
	-0.02596338512915,
	0.01624864962975,
	-0.00240879051584,
	0.00674613682247,
	-0.00187763777362
};

int butterworth_filter_count = 3;
float butterworth_filter_a[] = {
	1.00000000000000,
	-1.96977855582618,
	0.97022847566350
};

float butterworth_filter_b[] = {
	0.98500175787242,
	-1.97000351574484,
	0.98500175787242
};

float calculate_rms(unsigned int item_count, short* items) {
	if (item_count == 0) return 0.0;

	float* in = malloc(sizeof(float) * item_count);
	memset(in, 0, sizeof(float) * item_count);
	for (int i = 0; i < item_count; i++) {
		in[i] = items[i];
	}
	float* out = malloc(sizeof(float) * item_count);
	memset(out, 0, sizeof(float) * item_count);

	filter(yulewalk_filter_count, yulewalk_filter_a, yulewalk_filter_b,
			item_count-1, in, out);

	free(in);
	in = out;
	out = malloc(sizeof(float) * item_count);
	memset(out, 0, sizeof(float) * item_count);

	filter(butterworth_filter_count, butterworth_filter_a,
			butterworth_filter_b, item_count-1, in, out);
	free(in);

	unsigned long int sum = 0;
	for (int i = 0; i < item_count; i++) {
		sum += out[i] * out[i];
	}
	free(out);

	float rms = (float) sum / item_count;
	return rms;
}

int float_qsort_cmp(const void* pa, const void* pb) {
	float a = *(float*) pa;
	float b = *(float*) pb;
	if (a == b) return 0;
	return a > b ? 1 : -1;
}

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

	unsigned int rms_data_count = 10;
	unsigned int rms_data_used = 0;
	float* rms_data = malloc(sizeof(float) * rms_data_count);
	memset(rms_data, 0, sizeof(float) * rms_data_count);

	InitGainAnalysis(pCodecCtx->sample_rate);

//	printf("sample rate: %d\n", pCodecCtx->sample_rate);
	unsigned int sample_block_count = pCodecCtx->sample_rate / (1 / 0.050);
//	printf("samples per 50ms: %d\n", sample_block_count);
	short sample_block[sample_block_count];
	unsigned int sample_block_used = 0;
	AVPacket packet;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		if (packet.stream_index == audio_stream) {
			int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
			uint8_t *samples = malloc(out_size);
			int res = avcodec_decode_audio3(pCodecCtx, (short*)samples, &out_size,
					&packet);

			for (int i = 0; i < res; i++) {
				sample_block[sample_block_used++] = samples[i];

				if (sample_block_used == sample_block_count) {
					rms_data[rms_data_used++] =
						calculate_rms(sample_block_used, sample_block);
					if (rms_data_used == rms_data_count) {
						rms_data_count *= 2;
						rms_data = realloc(rms_data, sizeof(float) * rms_data_count);
					}

					memset(&sample_block, 0,
						sizeof(short) * sample_block_count);
					sample_block_used = 0;
				}
			}

			AnalyzeSamples(samples, NULL, out_size, 1);

			free(samples);
		} else {
			av_free_packet(&packet);
		}
	}

	qsort(rms_data, rms_data_used, sizeof(float), float_qsort_cmp);

	float gain = rms_data[(int) (0.95 * rms_data_used - 1)];
	float dbgain = 10 * log(gain)/log(10);
	printf("%s got a gain of %f\n", filename + strlen(filename) - 20, dbgain);
	free(rms_data);

	float otherGain = GetTitleGain();
	printf("got titleGain: %+6.2f\n", otherGain);

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
