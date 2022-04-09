/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2005 Timothy Robinson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SAMPLE_H
#define SAMPLE_H

#include <glib.h>
#include <stdio.h>

typedef struct Points_ Points;
struct Points_ {
        int min, max;
};

enum AudioType {
	WAVBREAKER_AUDIO_TYPE_UNKNOWN = 0,
	WAVBREAKER_AUDIO_TYPE_CDDA = 1,
	WAVBREAKER_AUDIO_TYPE_WAV = 2,
	WAVBREAKER_AUDIO_TYPE_MP3 = 3,
	WAVBREAKER_AUDIO_TYPE_OGG_VORBIS = 4,
};

typedef struct GraphData_ GraphData;
struct GraphData_{
	unsigned long numSamples;
	unsigned long maxSampleValue;
        unsigned long maxSampleAmp;
        unsigned long minSampleAmp;
	Points *data;
};

typedef struct WriteInfo_ WriteInfo;
struct WriteInfo_ {
	guint num_files;
	guint cur_file;
	char *cur_filename;
	char *merge_filename;
	double pct_done;
	guint sync;
	gint check_file_exists;
	gint skip_file; /* -1 = waiting for check
			 * 0 = don't overwrite file
			 * 1 = file is ok to overwrite
			 * 2 = overwrite all files
			 */
	gint sync_check_file_overwrite_to_write_progress;
	GList *errors;
};

#define DEFAULT_BUF_SIZE 4096

#define CD_BLOCKS_PER_SEC               (75)

/* Responses for the ask open raw dialog */
#define WB_RESPONSE_LITTLE_ENDIAN 1
#define WB_RESPONSE_BIG_ENDIAN 2

typedef struct SampleInfo_ SampleInfo;

struct SampleInfo_ {
    unsigned short  channels;
    unsigned int    samplesPerSec;
    unsigned int    avgBytesPerSec;
    unsigned short  blockAlign;
    unsigned short  bitsPerSample;
    unsigned long   numBytes;
    unsigned int    bufferSize;
    unsigned int    blockSize;
};

char *sample_get_error_message();

void sample_init();
void sample_set_audio_dev(char *str);
char * sample_get_audio_dev();

char * sample_get_sample_file();

int sample_is_playing();
int sample_is_writing();
int play_sample(gulong startpos, gulong *play_marker);
void stop_sample();
int sample_open_file(const char *, GraphData *, double *);
void sample_close_file();
void sample_write_files(GList *, WriteInfo *, char *);

#endif /* SAMPLE_H*/
