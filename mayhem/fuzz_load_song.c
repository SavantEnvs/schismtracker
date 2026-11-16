/* Feeds the input to every song loader in upstream's own load order (include/fmt-types.h), the same
 * walk as song_create_load() in schism/audio_loadsave.c, then lets the player render a bounded
 * number of buffers so pattern/envelope/effect handling runs on whatever the loader accepted. */
#include "headers.h"
#include "fmt.h"
#include "slurp.h"
#include "song.h"
#include "player/sndfile.h"
#include "timer.h"

#define LOAD_SONG(x) fmt_##x##_load_song,
static fmt_load_song_func load_song_funcs[] = {
#include "fmt-types.h"
	NULL,
};

#define RENDER_BUFFERS 16

/* Same startup call upstream's own main() and test harness make: the song code reads timer_ticks(). */
int LLVMFuzzerInitialize(SCHISM_UNUSED int *argc, SCHISM_UNUSED char ***argv)
{
	SCHISM_RUNTIME_ASSERT(timer_init(), "need timers");
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	slurp_t s;
	fmt_load_song_func *func;
	song_t *song;
	int ok = 0;

	if (slurp_memstream(&s, data, size) < 0)
		return 0;

	{
		uint8_t *mmdata;
		size_t mmlen;

		if (mmcmp_unpack(&s, &mmdata, &mmlen)) {
			unslurp(&s);
			slurp_memstream_free(&s, mmdata, mmlen);
		}
	}

	song = csf_allocate();

	for (func = load_song_funcs; *func && !ok; func++) {
		slurp_rewind(&s);
		switch ((*func)(song, &s, 0)) {
		case LOAD_SUCCESS:
			ok = 1;
			break;
		case LOAD_UNSUPPORTED:
			continue;
		default:
			goto done;
		}
	}

	if (ok) {
		uint8_t buf[4096];
		int i;

		csf_init_midi(song, NULL);
		csf_set_wave_config(song, 22050, 16, 2);
		song->mix_flags |= SNDMIX_DIRECTTODISK | SNDMIX_NOBACKWARDJUMPS;
		song->repeat_count = -1;
		song->stop_at_order = song->stop_at_row = -1;
		csf_set_current_order(song, 0);

		for (i = 0; i < RENDER_BUFFERS && !(song->flags & SONG_ENDREACHED); i++)
			csf_read(song, buf, sizeof(buf));
	}

done:
	unslurp(&s);
	csf_free(song);
	return 0;
}
