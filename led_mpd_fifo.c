/*
 * Copyright (C) 2008-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <glib.h>
#include "had.h"
#include "led_mpd_fifo.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "led_mpd_fifo"

#ifndef _OE
#include <fftw3.h>
static fftw_complex *fft_output;
static fftw_plan fft_plan;

#endif
#define SAMPLES 256
#define RESULTS (SAMPLES/2+1)
#define FREQ_PER_COL (RESULTS/64*4/5)

static int mpd_fifo_fd = 0;
static double *fft_input;
static unsigned int fft_magnitude[RESULTS];
static unsigned int col_magnitude_max;

double col_magnitude[64];

int mpdFifoInit(void)
{
#ifndef _OE
    if(!config.mpd_fifo_activated)
        return 1;
    if(mpd_fifo_fd > 0)
        return 2;
    mpd_fifo_fd = open(config.mpd_fifo_file,O_RDONLY | O_NONBLOCK);
    if(mpd_fifo_fd < 0)
        return 1;
    
    fft_input = (double*)fftw_malloc(sizeof(double)*SAMPLES);
    fft_output = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*RESULTS);
    fft_plan = fftw_plan_dft_r2c_1d(SAMPLES, fft_input, fft_output, FFTW_ESTIMATE);
#endif
    return 0;
}

void mpdFifoClose(void)
{
#ifndef _OE
    if(mpd_fifo_fd <= 0)
        return;
    close(mpd_fifo_fd);
    mpd_fifo_fd = 0;
    fftw_free(fft_input);
    fftw_free(fft_output);
#endif
}

void mpdFifoUpdate(void)
{
#ifndef _OE
    static int16_t buf[SAMPLES*2];
    ssize_t num;
    int i,p;

    if(mpd_fifo_fd <= 0)
        return;

    num = read(mpd_fifo_fd, buf, sizeof(buf));
    if(num < 0)
        return;

    for(i=num;i<SAMPLES;i++)
    {
        fft_input[i] = 0;
    }
    
    for(i=0;i<SAMPLES;i++)
    {
        fft_input[i] = buf[i];
    }
    
    fftw_execute(fft_plan);

    for(i=0;i<RESULTS;i++)
    {
        fft_magnitude[i] = sqrt(fft_output[i][0]*fft_output[i][0] + fft_output[i][1]*fft_output[i][1]);
    }
    col_magnitude_max = 0;
    for(i=0;i<64;i++)
    {
        col_magnitude[i] = 0;
        for(p=0;p<FREQ_PER_COL;p++)
        {
            col_magnitude[i] += fft_magnitude[p+i*FREQ_PER_COL];
        }
        col_magnitude[i] = col_magnitude[i]/FREQ_PER_COL;
        if(col_magnitude[i] > col_magnitude_max)
            col_magnitude_max = col_magnitude[i];
    }
#endif
}

