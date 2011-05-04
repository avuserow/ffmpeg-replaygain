/*
 * fbff - a small ffmpeg-based framebuffer/oss media player
 *
 * Copyright (C) 2009-2011 Ali Gholami Rudi
 *
 * This program is released under GNU GPL version 2.
 */
#include <fcntl.h>
#include <pty.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/soundcard.h>
#include <pthread.h>
#include "config.h"
#include "ffs.h"
#include "draw.h"
#include "gain_analysis.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

static struct termios termios;
static int paused;
static int exited;

static int audio = 1;

static struct ffs *affs;	/* audio ffmpeg stream */

static void stroll(void)
{
	usleep(10000);
}

#define ABUFSZ		(1 << 18)

static int a_cons;
static int a_prod;
static char a_buf[AUDIOBUFS][ABUFSZ];
static int a_len[AUDIOBUFS];
static int a_reset;

static int a_conswait(void)
{
	return a_cons == a_prod;
}

static int a_prodwait(void)
{
	return ((a_prod + 1) & (AUDIOBUFS - 1)) == a_cons;
}

static void a_doreset(int pause)
{
	a_reset = 1 + pause;
	while (audio && a_reset)
		stroll();
}

#define JMP1		(1 << 5)
#define JMP2		(JMP1 << 3)
#define JMP3		(JMP2 << 5)

static void mainloop(void)
{
	int eof = 0;
	while (eof < audio) {
		if (exited)
			break;
		if (paused) {
			a_doreset(1);
			continue;
		}
		while (audio && !eof && !a_prodwait()) {
			int ret = ffs_adec(affs, a_buf[a_prod], ABUFSZ);
			if (ret < 0)
				eof++;
			if (ret > 0) {
				a_len[a_prod] = ret;
				a_prod = (a_prod + 1) & (AUDIOBUFS - 1);
			}
		}
		stroll();
	}
	exited = 1;
}

static void *process_audio(void *dat)
{
	float max_pcm = 0;
	while (1) {
		while (!a_reset && (a_conswait() || paused) && !exited)
			stroll();
		if (exited)
			goto ret;
		if (a_reset) {
			if (a_reset == 1)
				a_cons = a_prod;
			a_reset = 0;
			continue;
		}
		Float_t analysis_buf[a_len[a_cons]];
		int i;
		for (i=0; i < a_len[a_cons]; i++) {
			analysis_buf[i] = (((float)a_buf[a_cons][i])/127.0);
			if (max_pcm < analysis_buf[i])
				max_pcm = analysis_buf[i];
		}
		if (AnalyzeSamples(analysis_buf, NULL, a_len[a_cons], 1) != GAIN_ANALYSIS_OK) {
			fprintf(stderr, "!!!!!!@$#!@#$# PROBLEM OFFICER: NYAN NYAN NYAN NYAN\n");
		}
		//write(afd, a_buf[a_cons], a_len[a_cons]);
		a_cons = (a_cons + 1) & (AUDIOBUFS - 1);
	}
ret:
	printf("Max PCM value found: %f\n", max_pcm);
	return NULL;
}

static void term_setup(void)
{
	struct termios newtermios;
	tcgetattr(STDIN_FILENO, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~ICANON;
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtermios);
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

static void term_cleanup(void)
{
	tcsetattr(STDIN_FILENO, 0, &termios);
}

static void sigcont(int sig)
{
	term_setup();
}

int main(int argc, char *argv[])
{
	int rate, ch, bps;
	pthread_t a_thread;
	char *path = argv[argc - 1];

	if (argc < 2) {
		printf("usage: %s filename\n", argv[0]);
		return 1;
	}
	ffs_globinit();
	if (audio && !(affs = ffs_alloc(path, 0)))
		audio = 0;
	if (!audio)
		return 1;
	ffs_ainfo(affs, &rate, &bps, &ch);
	printf("Detected sample rate %dHz\n", rate);
	long gain_rate = rate;
	InitGainAnalysis(gain_rate);
	pthread_create(&a_thread, NULL, process_audio, NULL);
	term_setup();
	signal(SIGCONT, sigcont);
	mainloop();
	term_cleanup();

	pthread_join(a_thread, NULL);
	ffs_free(affs);

	float title_gain = GetTitleGain();
	printf("Title gain: %f\n", title_gain);
	return 0;
}
