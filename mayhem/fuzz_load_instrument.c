/* Feeds the input to every instrument loader in upstream's load order (include/fmt-types.h), the same
 * walk as song_load_instrument_ex() in schism/audio_loadsave.c. The loaders fill slots of
 * `current_song`, so a fresh song is installed for the call and freed afterwards. */
#include "headers.h"
#include "fmt.h"
#include "slurp.h"
#include "song.h"
#include "player/sndfile.h"
#include "timer.h"

#define LOAD_INSTRUMENT(x) fmt_##x##_load_instrument,
static fmt_load_instrument_func load_instrument_funcs[] = {
#include "fmt-types.h"
	NULL,
};

/* Same startup call upstream's own main() and test harness make: the song code reads timer_ticks(). */
int LLVMFuzzerInitialize(SCHISM_UNUSED int *argc, SCHISM_UNUSED char ***argv)
{
	SCHISM_RUNTIME_ASSERT(timer_init(), "need timers");
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	slurp_t s;
	int x;
	song_t *song, *saved = current_song;

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
	current_song = song;

	for (x = 0; load_instrument_funcs[x]; x++) {
		slurp_rewind(&s);
		if (load_instrument_funcs[x](&s, 1))
			break;
	}

	unslurp(&s);
	current_song = saved;
	csf_free(song);
	return 0;
}
