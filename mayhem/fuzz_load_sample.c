/* Feeds the input to every sample loader in upstream's load order (include/fmt-types.h), the same
 * walk as song_load_sample() in schism/audio_loadsave.c. */
#include "headers.h"
#include "fmt.h"
#include "slurp.h"
#include "song.h"
#include "player/sndfile.h"
#include "timer.h"

#define LOAD_SAMPLE(x) fmt_##x##_load_sample,
static fmt_load_sample_func load_sample_funcs[] = {
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
	fmt_load_sample_func *load;
	song_sample_t smp = {0};

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

	smp.volume = 64 * 4;
	smp.global_volume = 64;

	for (load = load_sample_funcs; *load; load++) {
		slurp_rewind(&s);
		if ((*load)(&s, &smp))
			break;
	}

	unslurp(&s);

	if (smp.data)
		csf_free_sample(smp.data);

	return 0;
}
