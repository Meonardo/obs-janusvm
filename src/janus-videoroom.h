#pragma once

#include <obs-module.h>
#include <util/platform.h>

#define blog(level, msg, ...) \
	blog(level, "[janus-videoroom] " msg, ##__VA_ARGS__)

os_cpu_usage_info_t *GetCpuUsageInfo();

// janus configs
// janus server Websocket URL, room number, display name & etc...
struct janus_cfg {
	const char *url;
	const char *display;
	uint64_t room;
	const char *pin;

	int audio_mix_count;
	int audio_tracks;

	int width;
	int height;
};

struct janus_data {
	int64_t total_frames;
	int frame_size;

	uint64_t start_timestamp;

	// audio info
 	int audio_tracks;
	int num_audio_streams;

	struct janus_cfg config;

	bool initialized;

	char *last_error;
};

// main data struct
struct janus_output {
	obs_output_t *output;

	struct janus_data js_data;

	bool connecting;
	volatile bool active;
	volatile bool stopping;

	// the thread of this module, created in `janus_output_start()`
	// name: 'janus-output-thread'
	pthread_t start_thread;
	// stop event
	os_event_t *stop_event;

	uint64_t total_bytes;
	uint64_t audio_start_ts;
	uint64_t video_start_ts;
	uint64_t stop_ts;
};

// defines please see in the janus-videoroom.c
bool janus_data_init(struct janus_data *data, struct janus_cfg *config);
void janus_data_free(struct janus_data *data);
