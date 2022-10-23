#include <util/circlebuf.h>
#include <util/threading.h>
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
	// Initialize the cpu stats
	_cpuUsageInfo = os_cpu_usage_info_start();

	blog(LOG_INFO, "[obs_module_load] Module loaded.");
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs_module_unload] Shutting down...");

	// Destroy the cpu stats
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
	pthread_mutex_init_value(&data->write_mutex);
	data->output = output;

	if (pthread_mutex_init(&data->write_mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&data->stop_event, OS_EVENT_TYPE_AUTO) != 0)
		goto fail;
	if (os_sem_init(&data->write_sem, 0) != 0)
		goto fail;

	//av_log_set_callback(ffmpeg_log_callback);

	UNUSED_PARAMETER(settings);
	return data;

fail:
	pthread_mutex_destroy(&data->write_mutex);
	os_event_destroy(data->stop_event);
	bfree(data);
	return NULL;
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

		pthread_mutex_destroy(&output->write_mutex);
		os_sem_destroy(output->write_sem);
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
	if (output->write_thread_active) {
		os_event_signal(output->stop_event);
		os_sem_post(output->write_sem);
		pthread_join(output->write_thread, NULL);
		output->write_thread_active = false;
	}

	pthread_mutex_lock(&output->write_mutex);

	/*for (size_t i = 0; i < output->packets.num; i++)
		av_packet_free(output->packets.array + i);
	da_free(output->packets);*/

	pthread_mutex_unlock(&output->write_mutex);

	janus_data_free(&output->js_data);
}

static uint64_t janus_output_total_bytes(void *data)
{
	struct janus_output *output = data;
	return output->total_bytes;
}

static bool try_connect(struct janus_output *output)
{
	video_t *video = obs_output_video(output->output);
	const struct video_output_info *voi = video_output_get_info(video);
	struct janus_cfg config;
	obs_data_t *settings;
	bool success;
	int ret;

	settings = obs_output_get_settings(output->output);

	config.url = obs_data_get_string(settings, "url");
	config.display = get_string_or_null(settings, "display");
	config.room = (uint64_t)obs_data_get_int(settings, "room");
	config.width = (int)obs_output_get_width(output->output);
	config.height = (int)obs_output_get_height(output->output);

	config.audio_tracks = (int)obs_output_get_mixers(output->output);
	config.audio_mix_count = get_audio_mix_count(config.audio_tracks);

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

	struct audio_convert_info aci = {
		.format = output->js_data.audio_format,
	};

	output->active = true;

	if (!obs_output_can_begin_data_capture(output->output, 0))
		return false;

	ret = pthread_create(&output->write_thread, NULL, write_thread, output);
	if (ret != 0) {
		blog(LOG_WARNING,
		     "janus_output_start: failed to create write thread.");
		janus_output_full_stop(output);
		return false;
	}

	obs_output_set_video_conversion(output->output, NULL);
	obs_output_set_audio_conversion(output->output, &aci);
	obs_output_begin_data_capture(output->output, 0);
	output->write_thread_active = true;
	return true;
}

static void *write_thread(void *data)
{
	struct janus_output *output = data;

	while (os_sem_wait(output->write_sem) == 0) {
		/* check to see if shutting down */
		if (os_event_try(output->stop_event) == 0)
			break;

		////////// TODO!!!
	}

	output->active = false;
	return NULL;
}

static void *start_thread(void *data)
{
	struct janus_output *output = data;

	if (!try_connect(output))
		obs_output_signal_stop(output->output,
				       OBS_OUTPUT_CONNECT_FAILED);

	output->connecting = false;
	return NULL;
}

/* Given a bitmask for the selected tracks and the mix index,
 * this returns the stream index which will be passed to the muxer. */
static int get_track_order(int track_config, size_t mix_index)
{
	int position = 0;
	for (size_t i = 0; i < mix_index; i++) {
		if (track_config & 1 << i)
			position++;
	}
	return position;
}

static void receive_audio(void *param, size_t mix_idx, struct audio_data *frame)
{
	struct janus_output *output = param;
	struct janus_data *data = &output->js_data;
	//size_t frame_size_bytes;
	struct audio_data in = *frame;
	int track_order;

	/* check that the track was selected */
	if ((data->audio_tracks & (1 << mix_idx)) == 0)
		return;

	/* get track order (first selected, etc ...) */
	track_order = get_track_order(data->audio_tracks, mix_idx);
}

static void receive_video(void *param, struct video_data *frame)
{
	struct janus_output *output = param;
	struct janus_data *data = &output->js_data;

	// codec doesn't support video or none configured
	/*if (!data->video)
		return;*/
}

bool janus_data_init(struct janus_data *data, struct janus_cfg *config)
{
	bool is_rtmp = false;

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

struct obs_output_info janus_output = {
	.id = "janus_output",
	.flags = OBS_OUTPUT_AUDIO | OBS_OUTPUT_VIDEO | OBS_OUTPUT_MULTI_TRACK,
	.get_name = janus_output_getname,
	.create = janus_output_create,
	.destroy = janus_output_destroy,
	.start = janus_output_start,
	.stop = janus_output_stop,
	.raw_video = receive_video,
	.raw_audio2 = receive_audio,
	.get_total_bytes = janus_output_total_bytes,
};
