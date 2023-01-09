#pragma once

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#define blog(level, msg, ...) \
	blog(level, "[janus-videoroom] " msg, ##__VA_ARGS__)

// uncomment this to enable send encoded data to janus 
//#define USE_ENCODED_DATA

// janus configs
struct janus_cfg {
	// janus websocket server url
	const char *url;
	// the user's display name in the room
	const char *display;
	// the room number 
	uint64_t room;
	// the user id
	uint32_t user_id;
	// optional 
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

	uint64_t total_bytes;
	uint64_t audio_start_ts;
	uint64_t video_start_ts;
	uint64_t stop_ts;
};

// defines please see in the janus-videoroom.c
bool janus_data_init(struct janus_data *data, struct janus_cfg *config);
void janus_data_free(struct janus_data *data);
