#ifndef REPLAYGAIN_H
#define REPLAYGAIN_H

#include "audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ReplayGain modes. */
#define REPLAYGAIN_MODE_OFF	0
#define REPLAYGAIN_MODE_TRACK	1
#define REPLAYGAIN_MODE_ALBUM	2

void replaygain_init();
void replaygain_shutdown();

int replaygain_get_mode();

/* Set the ReplayGain value (in dB) to be applied to the sound. */
void replaygain_set_value_db(double db);

/* Forget the ReplayGain value (the current file has none). */
void replaygain_unset();

int replaygain_is_active();

void replaygain_process_buffer(char *buf, const size_t size,
		const struct sound_params *sound_params);

#ifdef __cplusplus
}
#endif

#endif
