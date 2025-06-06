﻿#pragma once

/*
 * madplay - MPEG audio decoder and player
 * Copyright (C) 2000-2004 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: audio.h,v 1.38 2004/01/23 09:41:31 rob Exp $
 */

# ifndef AUDIO_H
# define AUDIO_H

# define MAX_RESAMPLEFACTOR    6
# define MAX_NSAMPLES        (1152 * MAX_RESAMPLEFACTOR)

enum audio_command {
    AUDIO_COMMAND_INIT,
    AUDIO_COMMAND_CONFIG,
    AUDIO_COMMAND_PLAY,
    AUDIO_COMMAND_STOP,
    AUDIO_COMMAND_FINISH
};

enum audio_mode {
    AUDIO_MODE_ROUND,
    AUDIO_MODE_DITHER
};

struct audio_stats {
    unsigned long               clipped_samples;
    mad_fixed_t                 peak_clipping;
    mad_fixed_t                 peak_sample;
};

union audio_control {
    enum audio_command command;

    struct audio_init {
        enum audio_command command;
        char const                  *path;
    } init;

    struct audio_config {
        enum audio_command command;
        unsigned int                channels;
        unsigned int                speed;
        unsigned int                precision;
    } config;

    struct audio_play {
        enum audio_command command;
        unsigned int                nsamples;
        mad_fixed_t const           *samples[2];
        enum audio_mode mode;
        struct audio_stats *stats;
    } play;

    struct audio_stop {
        enum audio_command command;
        int                         flush;
    } stop;

    struct audio_finish {
        enum audio_command command;
    } finish;
};

struct audio_dither {
    mad_fixed_t                 error[3];
    mad_fixed_t                 random;
};

extern char const *audio_error;

typedef int audio_ctlfunc_t(union audio_control *);


void audio_control_init(union audio_control *, enum audio_command);

signed long audio_linear_round(unsigned int, mad_fixed_t,
    struct audio_stats *);
signed long audio_linear_dither(unsigned int, mad_fixed_t,
    struct audio_dither *, struct audio_stats *);

unsigned char audio_mulaw_round(mad_fixed_t, struct audio_stats *);
unsigned char audio_mulaw_dither(mad_fixed_t, struct audio_dither *,
    struct audio_stats *);

typedef unsigned int audio_pcmfunc_t(unsigned char *, unsigned int,
    mad_fixed_t const *, mad_fixed_t const *,
    enum audio_mode, struct audio_stats *);

unsigned int audio_pcm_u8(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s8(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s16le(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s16be(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s24le(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s24be(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s32le(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_s32be(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

unsigned int audio_pcm_mulaw(unsigned char *data, unsigned int nsamples,
    mad_fixed_t const *left, mad_fixed_t const *right,
    enum audio_mode mode, struct audio_stats *stats);

# endif
