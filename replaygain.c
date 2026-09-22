/*
 * MOC - music on console
 * Copyright (C) 2026 Nikita Shubin <nikita.shubin@maquefel.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <math.h>

#include "common.h"
#include "audio.h"
#include "audio_conversion.h"
#include "playlist.h"
#include "options.h"
#include "replaygain.h"

#define DEBUG

#include "log.h"

/* The current ReplayGain mode (REPLAYGAIN_MODE_*). */
static int mode = REPLAYGAIN_MODE_OFF;

/* Non-zero if clipping prevention is enabled. */
static int limit;

/* Non-zero if a ReplayGain value was set for the current file. */
static int value_set;

/* The gain multiplier derived from the ReplayGain value in dB. */
static double multiplier = 1.0;

void replaygain_init()
{
	const char *mode_str;

	mode = REPLAYGAIN_MODE_OFF;
	mode_str = options_get_symb("ReplayGainMode");
	if (mode_str && !strcasecmp(mode_str, "Track"))
		mode = REPLAYGAIN_MODE_TRACK;
	else if (mode_str && !strcasecmp(mode_str, "Album"))
		mode = REPLAYGAIN_MODE_ALBUM;

	limit = options_get_bool("ReplayGainLimit");

	debug ("ReplayGain: mode_str='%s' mode=%d limit=%d",
			mode_str ? mode_str : "(null)", mode, limit);

	value_set = 0;
	multiplier = 1.0;
}

void replaygain_shutdown()
{
	value_set = 0;
}

int replaygain_get_mode()
{
	return mode;
}

void replaygain_set_value_db(double db)
{
	/* Convert the gain in dB to a linear multiplier. */
	multiplier = pow(10.0, db / 20.0);

	/* With clipping prevention enabled never amplify above the
	 * original level. */
	if (limit && multiplier > 1.0)
		multiplier = 1.0;

	value_set = 1;
}

void replaygain_unset()
{
	value_set = 0;
	multiplier = 1.0;
}

int replaygain_is_active()
{
	return value_set && mode != REPLAYGAIN_MODE_OFF;
}

/* private code */

static void process_buffer_u8(uint8_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = ((int16_t)buf[i] - (UINT8_MAX >> 1)) * multiplier;
		tmp += (UINT8_MAX >> 1);
		tmp = CLAMP(0, tmp, UINT8_MAX);
		buf[i] = (uint8_t)tmp;
	}
}

static void process_buffer_s8(int8_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = buf[i] * multiplier;
		tmp = CLAMP(INT8_MIN, tmp, INT8_MAX);
		buf[i] = (int8_t)tmp;
	}
}

static void process_buffer_u16(uint16_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = ((int32_t)buf[i] - (UINT16_MAX >> 1)) * multiplier;
		tmp += (UINT16_MAX >> 1);
		tmp = CLAMP(0, tmp, UINT16_MAX);
		buf[i] = (uint16_t)tmp;
	}
}

static void process_buffer_s16(int16_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = buf[i] * multiplier;
		tmp = CLAMP(INT16_MIN, tmp, INT16_MAX);
		buf[i] = (int16_t)tmp;
	}
}

static void process_buffer_u32(uint32_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = ((int64_t)buf[i] - ((int64_t)UINT32_MAX >> 1))
			* multiplier;
		tmp += ((int64_t)UINT32_MAX >> 1);
		tmp = CLAMP(0, tmp, UINT32_MAX);
		buf[i] = (uint32_t)tmp;
	}
}

static void process_buffer_s32(int32_t *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = buf[i] * multiplier;
		tmp = CLAMP(INT32_MIN, tmp, INT32_MAX);
		buf[i] = (int32_t)tmp;
	}
}

static void process_buffer_float(float *buf, size_t samples)
{
	size_t i;

	for (i = 0; i < samples; i++) {
		double tmp = buf[i] * multiplier;
		tmp = CLAMP(-1.0, tmp, 1.0);
		buf[i] = (float)tmp;
	}
}

void replaygain_process_buffer(char *buf, const size_t size,
		const struct sound_params *sound_params)
{
	long sound_endianness;
	long sound_format;
	int samplewidth;
	int is_float;
	int need_endianness_swap = 0;

	if (!replaygain_is_active())
		return;

	if (multiplier == 1.0)
		return;

	debug ("ReplayGain: process_buffer active mult=%f fmt=0x%lx size=%zu",
			multiplier, (unsigned long)sound_params->fmt, size);

	sound_endianness = sound_params->fmt & SFMT_MASK_ENDIANNESS;
	sound_format = sound_params->fmt & SFMT_MASK_FORMAT;

	samplewidth = sfmt_Bps(sound_format);
	is_float = sound_format == SFMT_FLOAT;

	if (sound_endianness != SFMT_NE && samplewidth > 1 && !is_float)
		need_endianness_swap = 1;

	assert (size % (samplewidth * sound_params->channels) == 0);

	if (need_endianness_swap) {
		if (samplewidth == 4)
			audio_conv_bswap_32((int32_t *)buf, size / sizeof(int32_t));
		else
			audio_conv_bswap_16((int16_t *)buf, size / sizeof(int16_t));
	}

	switch (sound_format) {
	case SFMT_U8:
		process_buffer_u8((uint8_t *)buf, size);
		break;
	case SFMT_S8:
		process_buffer_s8((int8_t *)buf, size);
		break;
	case SFMT_U16:
		process_buffer_u16((uint16_t *)buf, size / sizeof(uint16_t));
		break;
	case SFMT_S16:
		process_buffer_s16((int16_t *)buf, size / sizeof(int16_t));
		break;
	case SFMT_U32:
		process_buffer_u32((uint32_t *)buf, size / sizeof(uint32_t));
		break;
	case SFMT_S32:
		process_buffer_s32((int32_t *)buf, size / sizeof(int32_t));
		break;
	case SFMT_FLOAT:
		process_buffer_float((float *)buf, size / sizeof(float));
		break;
	}

	if (need_endianness_swap) {
		if (samplewidth == 4)
			audio_conv_bswap_32((int32_t *)buf, size / sizeof(int32_t));
		else
			audio_conv_bswap_16((int16_t *)buf, size / sizeof(int16_t));
	}
}
