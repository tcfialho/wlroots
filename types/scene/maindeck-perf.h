/* Optional, event-loop-only scene diagnostics. No exported ABI or GPU queries.
 * Build with -DMAINDECK_SCENE_METRICS; set WLR_SCENE_METRICS to an output path.
 * Writes once at normal process exit. SIGKILL intentionally loses the sample.
 */
#ifdef MAINDECK_SCENE_METRICS
#include <inttypes.h>
#include <stdio.h>
static struct {
	bool enabled;
	const char *path;
	uint64_t builds, composed, scanout, entries, background_steps;
	uint64_t hidden_updates, hidden_updates_skipped, early_damage_rejects, sampled_ns, samples;
	uint64_t scanout_multi_entry, scanout_disabled, scanout_output_locked;
	uint64_t scanout_transform, scanout_color, scanout_test_failed;
} scene_perf;

static void scene_perf_dump(void) {
	if (!scene_perf.enabled) {
		return;
	}
	FILE *file = fopen(scene_perf.path, "w");
	if (!file) {
		return;
	}

#define FIELD(name) fprintf(file, "\"" #name "\":%" PRIu64 ",", scene_perf.name)
	fputs("{", file);
	FIELD(builds); FIELD(composed); FIELD(scanout); FIELD(entries);
	FIELD(background_steps); FIELD(hidden_updates); FIELD(early_damage_rejects);
	FIELD(hidden_updates_skipped); FIELD(scanout_multi_entry); FIELD(scanout_disabled);
	FIELD(scanout_output_locked); FIELD(scanout_transform); FIELD(scanout_color);
	FIELD(scanout_test_failed); FIELD(sampled_ns);
	fprintf(file, "\"samples\":%" PRIu64 "}\n", scene_perf.samples);
#undef FIELD
	fclose(file);
}

static void scene_perf_init(void) {
	static bool initialized;
	if (initialized) {
		return;
	}
	initialized = true;
	scene_perf.path = getenv("WLR_SCENE_METRICS");
	scene_perf.enabled = scene_perf.path && scene_perf.path[0];
	if (scene_perf.enabled && atexit(scene_perf_dump) != 0) {
		scene_perf.enabled = false;
	}
}
#define SCENE_COUNT(name) do { if (scene_perf.enabled) scene_perf.name++; } while (0)
#else
#define scene_perf_init() ((void)0)
#define SCENE_COUNT(name) ((void)0)
#endif
