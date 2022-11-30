#include <util/circlebuf.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include "janus-videoroom.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("janus-videoroom", "en-US")
OBS_MODULE_AUTHOR("Meonardo")

const char *obs_module_name(void)
{
	return "janus-videoroom";
}

const char *obs_module_description(void)
{
	return "push media stream to janus videoroom plugin";
}

os_cpu_usage_info_t *_cpuUsageInfo;
extern struct obs_output_info janus_output;

bool obs_module_load(void)
{
	// register output
	obs_register_output(&janus_output);
	// initialize the cpu stats
	_cpuUsageInfo = os_cpu_usage_info_start();

	blog(LOG_INFO, "[obs_module_load] Module loaded.");
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs_module_unload] Shutting down...");

	// destroy the cpu stats
	os_cpu_usage_info_destroy(_cpuUsageInfo);

	blog(LOG_INFO, "[obs_module_unload] Finished shutting down.");
}

os_cpu_usage_info_t *GetCpuUsageInfo()
{
	return _cpuUsageInfo;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// output defines

static void janus_output_full_stop(void *data);
static void janus_deactivate(struct janus_output *output);
static void *write_thread(void *data);
static void *start_thread(void *data);

// data init & deinit
bool janus_data_init(struct janus_data *data, struct janus_cfg *config)
{
	memset(data, 0, sizeof(struct janus_data));
	data->config = *config;
	data->num_audio_streams = config->audio_mix_count;
	data->audio_tracks = config->audio_tracks;
	if (!config->url || !*config->url)
		return false;

	data->initialized = true;
	return true;
}

void janus_data_free(struct janus_data *data)
{
	if (data->last_error)
		bfree(data->last_error);

	memset(data, 0, sizeof(struct janus_data));
}
// end data init & deinit

static inline const char *get_string_or_null(obs_data_t *settings,
					     const char *name)
{
	const char *value = obs_data_get_string(settings, name);
	if (!value || !strlen(value))
		return NULL;
	return value;
}

static int get_audio_mix_count(int audio_mix_mask)
{
	int mix_count = 0;
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		if ((audio_mix_mask & (1 << i)) != 0) {
			mix_count++;
		}
	}

	return mix_count;
}

static const char *janus_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("JanusVideoRoom");
}

static void *janus_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct janus_output *data = bzalloc(sizeof(struct janus_output));
	data->output = output;

	if (os_event_init(&data->stop_event, OS_EVENT_TYPE_AUTO) != 0) {
		os_event_destroy(data->stop_event);
		bfree(data);
		return NULL;
	}

	UNUSED_PARAMETER(settings);
	return data;
}

static bool janus_output_start(void *data)
{
	struct janus_output *output = data;
	int ret;

	if (output->connecting)
		return false;

	os_atomic_set_bool(&output->stopping, false);
	output->audio_start_ts = 0;
	output->video_start_ts = 0;
	output->total_bytes = 0;

	ret = pthread_create(&output->start_thread, NULL, start_thread, output);

	return (output->connecting = (ret == 0));
}

static void janus_output_destroy(void *data)
{
	struct janus_output *output = data;

	if (output) {
		if (output->connecting)
			pthread_join(output->start_thread, NULL);

		janus_output_full_stop(output);

		os_event_destroy(output->stop_event);
		bfree(data);
	}
}

static void janus_output_full_stop(void *data)
{
	struct janus_output *output = data;

	if (output->active) {
		obs_output_end_data_capture(output->output);
		janus_deactivate(output);
	}
}

static void janus_output_stop(void *data, uint64_t ts)
{
	struct janus_output *output = data;

	if (output->active) {
		if (ts > 0) {
			output->stop_ts = ts;
			os_atomic_set_bool(&output->stopping, true);
		}

		janus_output_full_stop(output);
	}
}

static void janus_deactivate(struct janus_output *output)
{
	janus_data_free(&output->js_data);
}

static uint64_t janus_output_total_bytes(void *data)
{
	struct janus_output *output = data;
	return output->total_bytes;
}

static bool try_connect(struct janus_output *output)
{
	struct janus_cfg config = {0};
	bool success;

	// get settings from fronted api
	// janus configs
	obs_data_t *settings = obs_output_get_settings(output->output);
	config.url = obs_data_get_string(settings, "url");
	config.display = get_string_or_null(settings, "display");
	config.room = (uint64_t)obs_data_get_int(settings, "room");
	config.pin = get_string_or_null(settings, "pin");

	// a/v configs
	config.width = (int)obs_output_get_width(output->output);
	config.height = (int)obs_output_get_height(output->output);
	config.audio_tracks = (int)obs_output_get_mixers(output->output);
	config.audio_mix_count = get_audio_mix_count(config.audio_tracks);

	// init struct `janus_data`
	success = janus_data_init(&output->js_data, &config);
	obs_data_release(settings);

	if (!success) {
		if (output->js_data.last_error) {
			obs_output_set_last_error(output->output,
						  output->js_data.last_error);
		}
		janus_data_free(&output->js_data);
		return false;
	}

	// set the output is active!
	output->active = true;

	if (!obs_output_can_begin_data_capture(output->output, 0))
		return false;
	// begin capture
	obs_output_begin_data_capture(output->output, 0);
	// will call `obs_output_end_data_capture()` in `janus_output_full_stop()`

	return true;
}

static void *start_thread(void *data)
{
	struct janus_output *output = data;

	// get janus configs & connect to janus ws server
	if (!try_connect(output))
		obs_output_signal_stop(output->output,
				       OBS_OUTPUT_CONNECT_FAILED);

	// set connecting 
	output->connecting = false;

	// set thread name
	os_set_thread_name("janus-output-thread");

	return NULL;
}

// audio callback from obs
static void receive_audio(void *param, struct audio_data *frame)
{
	struct janus_output *output = param;
	struct janus_data *data = &output->js_data;
}

// video callback from obs
static void receive_video(void *param, struct video_data *frame)
{
	struct janus_output *output = param;
	struct janus_data *data = &output->js_data;
}

struct obs_output_info janus_output = {
	.id = "janus_output",
	.flags = OBS_OUTPUT_AV,
	.get_name = janus_output_getname,
	.create = janus_output_create,
	.destroy = janus_output_destroy,
	.start = janus_output_start,
	.stop = janus_output_stop,
	.raw_video = receive_video,
	.raw_audio = receive_audio,
	.get_total_bytes = janus_output_total_bytes,
};
