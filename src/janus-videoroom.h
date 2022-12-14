#pragma once

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#define blog(level, msg, ...) \
	blog(level, "[janus-videoroom] " msg, ##__VA_ARGS__)

#define USE_ENCODED_DATA 1

os_cpu_usage_info_t *GetCpuUsageInfo();

// janus configs
// janus server Websocket URL, room number, display name & etc...
struct janus_cfg {
	const char *url;
	const char *display;
	uint64_t room;
	uint32_t user_id;
	const char *pin;

	int width;
	int height;
};

struct janus_data {
	struct janus_cfg config;

	bool initialized;

	char *last_error;
};

// main data struct
struct janus_output {
	obs_output_t *output;

	struct janus_data js_data;
	// `JanusConnection` instance pointer
	void *janus_conn;

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
