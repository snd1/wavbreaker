#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>

#include "wavbreaker.h"
#include "linuxaudio.h"
#include "sample.h"
#include "wav.h"
#include "cdda.h"

#define BUF_SIZE 4096
#define BLOCK_SIZE 2352

#define CDDA 1
#define WAV  2 

static SampleInfo sampleInfo;
static unsigned long sample_start = 0;
static int playing = 0;
static int audio_type;
static int audio_fd;

static char *audio_dev = "/dev/dsp";
static char *sample_file = NULL;

static pthread_t thread;
static pthread_attr_t thread_attr;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void sample_max_min(GraphData *graphData, double *pct);

void sample_set_audio_dev(char *str)
{
	audio_dev = strdup(str);
}

char * sample_get_audio_dev()
{
	return strdup(audio_dev);
}

char * sample_get_sample_file()
{
	return strdup(sample_file);
}

static void *
play_thread(void *thread_data)
{
	int ret, i;
	unsigned char devbuf[BUF_SIZE];

	if ((audio_fd = open_device(audio_dev, &sampleInfo)) < 0) {
		playing = 0;
		return NULL;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_file, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_file, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	}

	while (ret > 0 && ret <= BUF_SIZE) {
	        write(audio_fd, devbuf, ret);

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_file, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_file, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		}
	}

	pthread_mutex_lock(&mutex);

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);

	return NULL;
}

int play_sample(unsigned long startpos)
{       
	int ret;

	if (playing) {
		return 2;
	}

	if (sample_file == NULL) {
		return 3;
	}

	playing = 1;
	sample_start = startpos * BLOCK_SIZE;

	/* setup thread */

	if ((ret = pthread_mutex_init(&mutex, NULL)) != 0) {
		perror("Return from pthread_mutex_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_init(&thread_attr)) != 0) {
		perror("Return from pthread_attr_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_setdetachstate(&thread_attr,
		PTHREAD_CREATE_DETACHED)) != 0) {

		perror("Return from pthread_attr_setdetachstate");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_create(&thread, &thread_attr, play_thread,
			NULL)) != 0) {

		perror("Return from pthread_create");
		printf("Error #%d\n", ret);
		return 1;
	}

	return 0;
}               

void stop_sample()
{       
	if (pthread_mutex_trylock(&mutex)) {
		return;
	}

	if (!playing) {
		pthread_mutex_unlock(&mutex);
		return;
	}

	if (pthread_cancel(thread)) {
		perror("Return from pthread_cancel");
	        printf("trouble cancelling the thread\n");
		pthread_mutex_unlock(&mutex);
		return;
	}

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);
}

void sample_open_file(const char *filename, GraphData *graphData, double *pct)
{
	sample_file = strdup(filename);

	if (strstr(sample_file, ".wav")) {
		wav_read_header(sample_file, &sampleInfo);
		audio_type = WAV;
	printf("opening wav\n");
	} else {
		cdda_read_header(sample_file, &sampleInfo);
		audio_type = CDDA;
	printf("opening cdda\n");
	}

	sample_max_min(graphData, pct);
}

void sample_max_min(GraphData *graphData, double *pct)
{
	int tmp, min, max;
	int i, k, ret;
	int numSampleBlocks;
	unsigned char devbuf[BLOCK_SIZE];
	Points *graph_data;

	numSampleBlocks = 
		(((sampleInfo.numBytes / (sampleInfo.bitsPerSample / 8))
		/ BLOCK_SIZE) + 1) * 2;

	graph_data = (Points *)malloc(numSampleBlocks * sizeof(Points));

	if (graph_data == NULL) {
		printf("NULL returned from malloc of graph_data\n");
		return;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_file, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_file, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	}

	printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);

	while (ret > 0 && ret <= BLOCK_SIZE) {
		min = max = 0;
		i++;
		for (k = 0; k < ret; k++) {
			if (sampleInfo.bitsPerSample == 8) {
				tmp = devbuf[k];
			} else if (sampleInfo.bitsPerSample == 16) {
				tmp = (char)devbuf[k+1] << 8 | (char)devbuf[k];
				k++;
			}

			if (tmp > max) {
				max = tmp;
			} else if (tmp < min) {
				min = tmp;
			}
		}

		graph_data[i].min = min;
		graph_data[i].max = max;

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_file, devbuf, BLOCK_SIZE,
				BLOCK_SIZE * i);
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_file, devbuf, BLOCK_SIZE,
				BLOCK_SIZE * i);
		}

		*pct = (double) i / numSampleBlocks;
//		if (((int) (pct * 1000) % 10) == 0) {
//	printf("mod: %d\n", ((int) (pct * 1000) % 10));
			printf("pct done: %f\n", *pct);
//		}
	}

	*pct = 1.0;

	graphData->numSamples = numSampleBlocks;
	graphData->data = graph_data;

	if (sampleInfo.bitsPerSample == 8) {
		graphData->maxSampleValue = UCHAR_MAX;
	} else if (sampleInfo.bitsPerSample == 16) {
		graphData->maxSampleValue = SHRT_MAX;
	}
}
