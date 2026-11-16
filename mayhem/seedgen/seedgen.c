/* One-off generator for the starter seeds in mayhem/<target>/testsuite/: builds a tiny song with
 * upstream's own in-memory song model and writes it out through upstream's own savers (MOD/S3M/IT,
 * ITS/AU/AIFF/WAV/SBI/FLAC/S3I/raw, ITI/XI). Not part of build.sh; see mayhem/seedgen/README.
 * Usage: seedgen <out-dir-prefix>   (writes <prefix>/{song,sample,instrument}/seed_<fmt>) */
#include "headers.h"
#include "fmt.h"
#include "disko.h"
#include "song.h"
#include "timer.h"
#include "player/sndfile.h"

#include <sys/stat.h>

static const char *prefix;

static void emit(const char *dir, const char *name, disko_t *ds)
{
	char path[1024];
	FILE *f;

	snprintf(path, sizeof(path), "%s/%s", prefix, dir);
	mkdir(path, 0777);
	snprintf(path, sizeof(path), "%s/%s/seed_%s", prefix, dir, name);
	f = fopen(path, "wb");
	if (!f) {
		perror(path);
		exit(1);
	}
	fwrite(ds->data, 1, ds->length, f);
	fclose(f);
	printf("%s: %zu bytes\n", path, ds->length);
	disko_memclose(ds, 1);
}

#define SAVE(dir, name, call) do { \
		disko_t ds = {0}; \
		disko_memopen(&ds); \
		if ((call) == SAVE_SUCCESS) emit(dir, name, &ds); \
		else { fprintf(stderr, "%s: save failed\n", name); disko_memclose(&ds, 1); } \
	} while (0)

static void fill_sample(song_sample_t *smp, int sixteen)
{
	uint32_t i, len = 256;

	memset(smp, 0, sizeof(*smp));
	smp->length = len;
	smp->c5speed = 8363;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	smp->loop_start = 64;
	smp->loop_end = 192;
	smp->flags = CHN_LOOP | (sixteen ? CHN_16BIT : 0);
	strcpy(smp->name, "seed sample");
	strcpy(smp->filename, "seed.smp");
	smp->data = csf_allocate_sample(len * (sixteen ? 2 : 1));
	for (i = 0; i < len; i++) {
		int v = (int) (((i % 64) < 32) ? (i % 64) * 4 : (64 - (i % 64)) * 4) - 64;
		if (sixteen)
			((int16_t *) smp->data)[i] = (int16_t) (v * 256);
		else
			smp->data[i] = (signed char) v;
	}
}

static song_t *make_song(int instrument_mode)
{
	song_t *song = csf_allocate();
	song_note_t *pat;
	int i;

	fill_sample(&song->samples[1], 0);
	fill_sample(&song->samples[2], 1);
	strcpy(song->title, "seed song");
	song->orderlist[0] = 0;
	song->orderlist[1] = 1;
	song->orderlist[2] = 255;
	for (i = 0; i < 2; i++) {
		pat = song->patterns[i] = csf_allocate_pattern(64);
		song->pattern_size[i] = song->pattern_alloc_size[i] = 64;
	}
	for (i = 0; i < 64; i += 4) {
		song_note_t *n = song->patterns[0] + i * MAX_CHANNELS;
		n[0].note = 49 + (i % 12);
		n[0].instrument = 1;
		n[1].note = 61 + (i % 7);
		n[1].instrument = 2;
		n[1].voleffect = VOLFX_VOLUME;
		n[1].volparam = 40;
		n[2].effect = FX_PANNING;
		n[2].param = 0x80;
		n = song->patterns[1] + i * MAX_CHANNELS;
		n[0].note = 55;
		n[0].instrument = 1;
		n[0].effect = FX_PORTAMENTOUP;
		n[0].param = 0x08;
		n[3].note = NOTE_CUT;
	}
	song->patterns[0][8 * MAX_CHANNELS].effect = FX_SPEED;
	song->patterns[0][8 * MAX_CHANNELS].param = 4;
	song->patterns[1][60 * MAX_CHANNELS].effect = FX_POSITIONJUMP;
	song->patterns[1][60 * MAX_CHANNELS].param = 0;
	song->initial_tempo = 125;
	song->initial_speed = 6;
	if (instrument_mode) {
		song->flags |= SONG_INSTRUMENTMODE;
		for (i = 1; i <= 2; i++) {
			song_instrument_t *ins = song->instruments[i] = csf_allocate_instrument();
			int j;
			for (j = 0; j < 128; j++)
				ins->sample_map[j] = i;
			strcpy(ins->name, i == 1 ? "seed inst 1" : "seed inst 2");
			ins->vol_env.nodes = 3;
			ins->vol_env.ticks[0] = 0;  ins->vol_env.values[0] = 64;
			ins->vol_env.ticks[1] = 20; ins->vol_env.values[1] = 32;
			ins->vol_env.ticks[2] = 40; ins->vol_env.values[2] = 0;
			ins->flags |= ENV_VOLUME;
			ins->fadeout = 256;
		}
	}
	return song;
}

int main(int argc, char **argv)
{
	song_t *song;
	song_sample_t smp;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <out-dir-prefix>\n", argv[0]);
		return 2;
	}
	prefix = argv[1];
	if (!timer_init()) {
		fprintf(stderr, "timer_init failed\n");
		return 1;
	}

	song = make_song(0);
	current_song = song;
	SAVE("fuzz_load_song", "mod", fmt_mod_save_song(&ds, song));
	SAVE("fuzz_load_song", "s3m", fmt_s3m_save_song(&ds, song));
	SAVE("fuzz_load_song", "it", fmt_it_save_song(&ds, song));
	csf_free(song);

	song = make_song(1);
	current_song = song;
	SAVE("fuzz_load_song", "it_instruments", fmt_it_save_song(&ds, song));
	SAVE("fuzz_load_instrument", "iti", fmt_iti_save_instrument(&ds, song, song->instruments[1]));
	SAVE("fuzz_load_instrument", "xi", fmt_xi_save_instrument(&ds, song, song->instruments[2]));
	csf_free(song);

	fill_sample(&smp, 0);
	SAVE("fuzz_load_sample", "its", fmt_its_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "au", fmt_au_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "aiff", fmt_aiff_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "wav", fmt_wav_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "sbi", fmt_sbi_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "s3i", fmt_s3i_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "raw", fmt_raw_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "flac", fmt_flac_save_sample(&ds, &smp));
	csf_free_sample(smp.data);

	fill_sample(&smp, 1);
	SAVE("fuzz_load_sample", "its16", fmt_its_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "wav16", fmt_wav_save_sample(&ds, &smp));
	SAVE("fuzz_load_sample", "aiff16", fmt_aiff_save_sample(&ds, &smp));
	csf_free_sample(smp.data);
	return 0;
}
