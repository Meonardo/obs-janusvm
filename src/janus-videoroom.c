#include <util/circlebuf.h>
#include <util/dstr.h>
#include <util/darray.h>

#include "janus-videoroom.h"
#include "janus_connection_api.h"

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

extern struct obs_output_info janus_output;

bool obs_module_load(void)
{
	// register output
	obs_register_output(&janus_output);

	blog(LOG_INFO, "[obs_module_load] module loaded.");

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "[obs_module_unload] shut down.");
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// output defines

static void janus_output_full_stop(void *data);
static void janus_deactivate(struct janus_output *output);
static bool try_connect(struct janus_output *output);
static bool init_ffmpeg_audio_info(struct janus_output *output);
static void cleanup(AVCodecContext *codec_context,
		    AVFormatContext *format_context);
static void encode_audio(struct janus_output *output, size_t block_size);

// data init & deinit
bool janus_data_init(struct janus_data *data, struct janus_cfg *config)
{
	memset(data, 0, sizeof(struct janus_data));
	data->config = *config;
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

static inline int64_t rescale_ts(int64_t val, AVCodecContext *context,
				 AVRational new_base)
{
	return av_rescale_q_rnd(val, context->time_base, new_base,
				AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
}

static inline const char *get_string_or_null(obs_data_t *settings,
					     const char *name)
{
	const char *value = obs_data_get_string(settings, name);
	if (!value || !strlen(value))
		return NULL;
	return value;
}

static const char *janus_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("janus-videoroom output");
}

static void *janus_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct janus_output *data = bzalloc(sizeof(struct janus_output));
	data->output = output;
	data->janus_conn = NULL;

	// here create janus connection instance
#ifdef USE_ENCODED_DATA
	data->janus_conn = CreateConncetion(true);
#else
	data->janus_conn = CreateConncetion(false);
#endif // USE_ENCODED_DATA

	UNUSED_PARAMETER(settings);
	return data;
}

static bool janus_output_start(void *data)
{
	struct janus_output *output = data;

	if (output->connecting)
		return false;
	output->connecting = true;

	// get janus configs & connect to janus ws server
	if (!try_connect(output)) {
		obs_output_signal_stop(output->output,
				       OBS_OUTPUT_CONNECT_FAILED);
		output->connecting = false;
		return false;
	}

	os_atomic_set_bool(&output->stopping, false);
	output->audio_start_ts = 0;
	output->video_start_ts = 0;
	output->total_bytes = 0;

	output->connecting = false;

	return true;
}

static void janus_output_destroy(void *data)
{
	struct janus_output *output = data;
	if (output) {
		if (output->janus_conn != NULL) {
			DestoryConnection(output->janus_conn);
			output->janus_conn = NULL;
		}

		janus_output_full_stop(output);

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

	// encode audio test
	//AVCodecContext *codec_context = output->js_data.audio_info.ctx;
	//AVFormatContext *format_context = output->js_data.audio_info.format_ctx;
	//// Write the trailer to the output file
	//if (av_write_trailer(format_context) < 0) {
	//	blog(LOG_ERROR, "Could not write output file trailer");
	//}
	//cleanup(codec_context, format_context);

	// Unpublish
	if (output->janus_conn != NULL) {
		Unpublish(output->janus_conn);
	}

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
	config.user_id = (uint32_t)obs_data_get_int(settings, "id");
	config.pin = get_string_or_null(settings, "pin");

	// a/v configs
	config.width = (int)obs_output_get_width(output->output);
	config.height = (int)obs_output_get_height(output->output);

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

	if (!obs_output_can_begin_data_capture(output->output, 0))
		return false;

#ifdef USE_ENCODED_DATA
	if (!obs_output_initialize_encoders(output->output, OBS_OUTPUT_VIDEO)) {
		return false;
	}
#endif

	// set the output is active!
	os_atomic_set_bool(&output->active, true);

	// set audio conversion info
	struct audio_convert_info conversion;
	conversion.format = AUDIO_FORMAT_16BIT; // WebRTC only support 16-bit signed PCM data
	audio_t *audio = obs_get_audio();
	conversion.samples_per_sec = audio_output_get_sample_rate(audio);
	conversion.speakers = audio_output_get_channels(audio);
	obs_output_set_audio_conversion(output->output, &conversion);

	// begin capture
	obs_output_begin_data_capture(output->output, 0);
	// will call `obs_output_end_data_capture()` in `janus_output_full_stop()`

	if (output->janus_conn != NULL) {
		// start publishing...
		Publish(output->janus_conn, config.url, config.user_id,
			config.display, config.room, config.pin);
	}

	// init ffmpeg audio info(audio encoding test)
	/*success = init_ffmpeg_audio_info(output);
	if (!success) {
		struct ffmpeg_audio_info *audio_info =
			&output->js_data.audio_info;
		cleanup(audio_info->ctx, audio_info->format_ctx);
	}*/

	return true;
}

// audio callback from obs
static void receive_audio(void *param, struct audio_data *a_frame)
{
	struct janus_output *output = param;
	if (output->janus_conn != NULL) {
		// send audio frame to janus connection
		SendAudioFrame(output->janus_conn, a_frame);
	}

	// encode raw data to xxx.aac
	/*struct ffmpeg_audio_info *audio_info = &output->js_data.audio_info;
	AVCodecContext *codec_context = audio_info->ctx;
	AVFormatContext *format_context = audio_info->format_ctx;

	if (!output->audio_start_ts)
		output->audio_start_ts = a_frame->timestamp;

	int audio_size = audio_info->audio_size;
	size_t frame_size_bytes = (size_t)audio_info->frame_size * audio_size;
	size_t channels = audio_info->channels;

	for (size_t i = 0; i < channels; i++)
		circlebuf_push_back(&audio_info->excess_frames[i],
				    a_frame->data[i],
				    a_frame->frames * audio_size);

	while (audio_info->excess_frames[0].size >= frame_size_bytes) {
		for (size_t i = 0; i < channels; i++)
			circlebuf_pop_front(&audio_info->excess_frames[i],
					    audio_info->samples[i],
					    frame_size_bytes);

		encode_audio(output, audio_size);
	}*/
}

// video callback from obs
static void receive_video(void *param, struct video_data *frame)
{
	struct janus_output *output = param;
	if (output->janus_conn != NULL) {
		// send video frame to janus connection
		SendVideoFrame(output->janus_conn, frame,
			       output->js_data.config.width,
			       output->js_data.config.height);
	}
}

static void receive_encoded_data(void *data, struct encoder_packet *packet)
{
	struct janus_output *output = data;

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (output->janus_conn != NULL) {
			// send encoded packet to janus connection
			SendVideoPacket(output->janus_conn, packet,
					output->js_data.config.width,
					output->js_data.config.height);
		}
	}

	/*if (packet->type == OBS_ENCODER_AUDIO) {
		
	}*/
}

static void encode_audio(struct janus_output *output, size_t block_size)
{
	struct janus_data *data = &output->js_data;
	struct ffmpeg_audio_info *audio_info = &data->audio_info;
	struct AVCodecContext *context = audio_info->ctx;

	AVPacket *packet = NULL;
	int ret, got_packet;
	int channels;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
	channels = context->ch_layout.nb_channels;
#else
	channels = context->channels;
#endif
	int frame_size = audio_info->frame_size;
	size_t total_size = frame_size * block_size * channels;

	audio_info->frame->nb_samples = frame_size;
	audio_info->frame->pts = av_rescale_q(
		audio_info->total_samples,
		(AVRational){1, context->sample_rate}, context->time_base);

	ret = avcodec_fill_audio_frame(audio_info->frame, channels,
				       context->sample_fmt,
				       audio_info->samples[0], (int)total_size,
				       1);
	if (ret < 0) {
		blog(LOG_WARNING,
		     "encode_audio: avcodec_fill_audio_frame "
		     "failed: %s",
		     av_err2str(ret));
		//FIXME: stop the encode with an error
		return;
	}

	audio_info->total_samples += frame_size;

	packet = av_packet_alloc();

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	ret = avcodec_send_frame(context, audio_info->frame);
	if (ret == 0)
		ret = avcodec_receive_packet(context, packet);

	got_packet = (ret == 0);

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		ret = 0;
#else
	ret = avcodec_encode_audio2(context, packet, data->aframe[idx],
				    &got_packet);
#endif
	if (ret < 0) {
		blog(LOG_WARNING, "Error encoding audio: %s", av_err2str(ret));
		//FIXME: stop the encode with an error
		goto end;
	}

	if (!got_packet)
		goto end;

	packet->pts =
		rescale_ts(packet->pts, context, audio_info->stream->time_base);
	packet->dts =
		rescale_ts(packet->dts, context, audio_info->stream->time_base);
	packet->duration = (int)av_rescale_q(packet->duration,
					     context->time_base,
					     audio_info->stream->time_base);
	packet->stream_index = audio_info->stream->index;

	ret = av_interleaved_write_frame(audio_info->format_ctx, packet);
	if (ret < 0) {
		blog(LOG_WARNING, "process_packet: Error writing packet: %s",
		     av_err2str(ret));
	}

	goto end;
end:
	av_packet_free(&packet);
}

static void cleanup(AVCodecContext *codec_context,
		    AVFormatContext *format_context)
{
	avcodec_close(codec_context);
	avcodec_free_context(&codec_context);
	avio_closep(&format_context->pb);
	avformat_free_context(format_context);
}

static bool init_ffmpeg_audio_info(struct janus_output *output)
{
	struct ffmpeg_audio_info *audio_info = &output->js_data.audio_info;

	// get basic raw audio info from obs
	audio_t *audio = obs_get_audio();
	const struct audio_output_info *info = audio_output_get_info(audio);
	audio_info->channels = audio_output_get_channels(audio);
	audio_info->sample_rate = audio_output_get_sample_rate(audio);
	audio_info->bitrate = 160000;
	audio_info->frame_size = AUDIO_OUTPUT_FRAMES;
	audio_info->filename = "C:\\Users\\Meonardo\\Downloads\\audio_test.aac";
	audio_info->audio_size =
		get_audio_size(info->format, info->speakers, 1);

	const char *filename = audio_info->filename;
	int sample_rate = audio_info->sample_rate;

	int ret = 0;
	AVCodecContext *codec_context = NULL;
	AVFormatContext *format_context = NULL;

	// open the output file to write to it
	AVIOContext *output_io_context;
	ret = avio_open(&output_io_context, filename, AVIO_FLAG_WRITE);
	if (ret < 0) {
		blog(LOG_ERROR, "Could not open file: %s, %s", filename,
		     av_err2str(ret));
		return false;
	}

	// create a format context for the output container format
	if (!(format_context = avformat_alloc_context())) {
		blog(LOG_ERROR,
		     "Could not allocate output format context for file: %s",
		     filename);
		return false;
	}

	// associate the output context with the output file
	format_context->pb = output_io_context;

	// guess the desired output file type
	if (!(format_context->oformat =
		      av_guess_format(NULL, filename, NULL))) {
		blog(LOG_ERROR,
		     "Could not find output file format for file: %s",
		     filename);
		return false;
	}

	// add the file pathname to the output context
	if (!(format_context->url = av_strdup(filename))) {
		blog(LOG_ERROR, "Could not process file path name for file: %s",
		     filename);
		return false;
	}

	// find an encoder based on the codec
	const AVCodec *output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (output_codec == NULL) {
		blog(LOG_ERROR, "Could not open codec with ID: %s",
		     avcodec_get_name(AV_CODEC_ID_AAC));
		return false;
	}

	// create a new audio stream in the output file container
	AVStream *stream = avformat_new_stream(format_context, NULL);
	if (stream == NULL) {
		blog(LOG_ERROR, "Could not create new stream");
		return false;
	}

	// allocate an encoding context
	codec_context = avcodec_alloc_context3(output_codec);
	if (codec_context == NULL) {
		blog(LOG_ERROR, "Could not allocate encoding context");
		return false;
	}

	// Set the parameters of the stream
	codec_context->channels = audio_info->channels;
	codec_context->channel_layout =
		av_get_default_channel_layout(audio_info->channels);
	codec_context->sample_rate = sample_rate;
	codec_context->sample_fmt = output_codec->sample_fmts[0];
	codec_context->bit_rate = audio_info->bitrate;

	// Set the sample rate of the container
	stream->time_base.den = sample_rate;
	stream->time_base.num = 1;

	// add a global header if necessary
	if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
		codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	// open the encoder for the audio stream to use
	ret = avcodec_open2(codec_context, output_codec, NULL);
	if (ret < 0) {
		blog(LOG_ERROR, "Could not open output codec: %s",
		     av_err2str(ret));
		return false;
	}

	// make sure everything has been initialized correctly
	ret = avcodec_parameters_from_context(stream->codecpar, codec_context);
	if (ret < 0) {
		blog(LOG_ERROR, "Could not initialize stream parameters: %s",
		     av_err2str(ret));
		return false;
	}

	// write the header to the output file
	ret = avformat_write_header(format_context, NULL);
	if (ret < 0) {
		blog(LOG_ERROR, "Could not write output file header: %s",
		     av_err2str(ret));
		return false;
	}

	// save all the ffmepg contexts
	audio_info->ctx = codec_context;
	audio_info->format_ctx = format_context;
	audio_info->stream = stream;

	// alloc AVFrame
	audio_info->frame = av_frame_alloc();
	if (!audio_info->frame) {
		blog(LOG_ERROR, "Failed to allocate audio frame");
		return false;
	}

	int channels = audio_info->channels;

	audio_info->frame->format = codec_context->sample_fmt;
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 24, 100)
	audio_info->frame->channels = codec_context->channels;
	audio_info->frame->channel_layout = codec_context->channel_layout;
	channels = codec_context->channels;
#else
	data->aframe[idx]->ch_layout = context->ch_layout;
	channels = context->ch_layout.nb_channels;
#endif
	audio_info->frame->sample_rate = codec_context->sample_rate;
	codec_context->strict_std_compliance = -2;

	ret = av_samples_alloc(audio_info->samples, NULL, channels,
			       audio_info->frame_size,
			       codec_context->sample_fmt, 0);
	if (ret < 0) {
		blog(LOG_ERROR, "Failed to create audio buffer: %s",
		     av_err2str(ret));
		// free the frame if failed
		av_frame_free(&audio_info->frame);
		return false;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	avcodec_parameters_from_context(audio_info->stream->codecpar,
					codec_context);
#endif

	return true;
}

struct obs_output_info janus_output = {
	.id = "janus_output",
	.get_name = janus_output_getname,
	.create = janus_output_create,
	.destroy = janus_output_destroy,
	.start = janus_output_start,
	.stop = janus_output_stop,
#ifdef USE_ENCODED_DATA
	.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
	.encoded_video_codecs = "h264",
	//.encoded_audio_codecs = "opus",
	.encoded_packet = receive_encoded_data,
#else
	.flags = OBS_OUTPUT_AV,
	.raw_video = receive_video,
	.raw_audio = receive_audio,
#endif // USE_ENCODED_DATA
	.get_total_bytes = janus_output_total_bytes,
};
