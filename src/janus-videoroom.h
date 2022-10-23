#pragma once

#include <obs-module.h>
#include <util/platform.h>

#define blog(level, msg, ...) \
	blog(level, "[janus-videoroom] " msg, ##__VA_ARGS__)

os_cpu_usage_info_t *GetCpuUsageInfo();

struct janus_cfg {
	const char *url;
	const char *display;
	uint64_t room;

	int audio_mix_count;
	int audio_tracks;

	int width;
	int height;
	int frame_size; // audio frame size
};

struct janus_data {
	int64_t total_frames;
	int frame_size;

	uint64_t start_timestamp;

	/* audio_tracks is a bitmask storing the indices of the mixes */
	int audio_tracks;
	int64_t total_samples[MAX_AUDIO_MIXES];
	uint32_t audio_samplerate;
	enum audio_format audio_format;
	size_t audio_planes;
	size_t audio_size;
	int num_audio_streams;

	struct janus_cfg config;

	bool initialized;

	char *last_error;
};

struct janus_output {
	obs_output_t *output;
	volatile bool active;
	struct janus_data js_data;

	bool connecting;
	pthread_t start_thread;

	uint64_t total_bytes;

	uint64_t audio_start_ts;
	uint64_t video_start_ts;
	uint64_t stop_ts;
	volatile bool stopping;

	bool write_thread_active;
	pthread_mutex_t write_mutex;
	pthread_t write_thread;
	os_sem_t *write_sem;
	os_event_t *stop_event;
};

bool janus_data_init(struct janus_data *data, struct janus_cfg *config);
void janus_data_free(struct janus_data *data);
