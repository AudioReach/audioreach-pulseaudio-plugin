/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef foopalpasinkfoo
#define foopalpasinkfoo

#include <pulsecore/device-port.h>
#include <pulse/sample.h>
#include <pulsecore/card.h>
#include <pulsecore/core.h>
#include <pulsecore/core-util.h>

#include <PalApi.h>
#include <PalDefs.h>

#include "pal-card.h"

typedef enum {
    PAL_SINK_MESSAGE_WRITE_READY,
} pal_msgs_t;

typedef struct {
    pa_msgobject parent;
    void *userdata;
} pal_msg_obj;

PA_DEFINE_PRIVATE_CLASS(pal_msg_obj, pa_msgobject);
#define PAL_MSG_OBJ(o) (pal_msg_obj_cast(o))

typedef struct {
    char *name;
    char *description;
    char *pal_devicepp_config;
    int id;
    pal_stream_type_t stream_type;
    bool use_hw_volume;
    pa_sample_spec default_spec;
    pa_encoding_t default_encoding;
    pa_channel_map default_map;
    uint32_t alternate_sample_rate;
    pa_pal_card_avoid_processing_config_id_t avoid_config_processing;
    pa_idxset *formats;
    pa_hashmap *ports;
    pa_hashmap *profiles;
    char **port_conf_string;
    pa_pal_card_usecase_type_t usecase_type;
    uint32_t buffer_size;
    uint32_t buffer_count;
} pa_pal_sink_config;

typedef struct {
    pal_stream_handle_t *stream_handle;

    struct pal_device *pal_device;
    struct pal_stream_attributes *stream_attributes;
    const char *device_url;

    size_t buffer_size;
    size_t buffer_count;
    uint32_t sink_latency_us;
    uint64_t bytes_written;

    int write_fd;
    int index;

    bool standby;
    pa_mutex *mutex;

    /* Sink events */
    pa_fdsem *pal_fdsem;
    pa_cond *cond_ctrl_thread;
    pa_pal_ctrl_event_t sink_event_id;

    /* PAL sink_write thread */
    pal_msg_obj *pal_msg;
    pa_thread_mq pal_thread_mq;
    pa_thread *pal_thread;
    pa_rtpoll *pal_thread_rtpoll;
    pa_rtpoll_item *pal_rtpoll_item;
    pa_atomic_t restart_in_progress;
    pa_atomic_t write_done;
    pa_atomic_t close_output;

    pa_encoding_t encoding;
    bool compressed;
    bool dynamic_usecase;
    pal_snd_dec_t *pal_snd_dec;
} pal_sink_data;

typedef struct {
    bool first;
    pa_sink *sink;
    pa_rtpoll *rtpoll;
    pa_thread_mq thread_mq;
    pa_thread *thread;
    pa_rtpoll_item *rtpoll_item;
    pa_idxset *formats;
    pa_pal_card_avoid_processing_config_id_t avoid_config_processing;
} pa_sink_data;

typedef struct {
    pal_sink_data *pal_sdata;
    pa_sink_data *pa_sdata;
    struct userdata *u;
    bool pal_sink_opened; /* set when PAL session is to enabled */

    pa_fdsem *fdsem; /* common resource between pa and pal sink */
} pa_pal_sink_data;

typedef size_t pa_pal_sink_handle_t;

typedef struct {
    pa_pal_sink_handle_t *handle;
} pa_pal_card_sink_info;

typedef enum {
    PA_PAL_SINK_MESSAGE_DRAIN_READY = PA_SINK_MESSAGE_MAX + 1,
} pa_pal_sink_msgs_t;

bool pa_pal_sink_is_supported_sample_rate(uint32_t sample_rate);
/* create pal session and pa sink */
int pa_pal_sink_create(pa_module *m, pa_card *card, const char *driver, const char *module_name, pa_pal_sink_config *sink, pa_pal_sink_handle_t **handle);
void pa_pal_sink_close(pa_pal_sink_handle_t *handle);
void pa_pal_sink_module_init(void);
void pa_pal_sink_module_deinit(void);
int pa_pal_sink_get_media_config(pa_pal_sink_handle_t *handle, pa_sample_spec *ss, pa_channel_map *map, pa_encoding_t *encoding);
pa_idxset* pa_pal_sink_get_config(pa_pal_sink_handle_t *handle);
int pa_pal_sink_set_a2dp_suspend(const char *prm_value);

static inline bool pa_pal_sink_is_supported_encoding(pa_encoding_t encoding) {
    bool supported = true;

    switch (encoding) {
        case PA_ENCODING_PCM:
#ifndef PAL_DISABLE_COMPRESS_AUDIO_SUPPORT
        case PA_ENCODING_MPEG:
        case PA_ENCODING_AAC:
#endif
            break;

        default :
            supported = false;
            pa_log_error("%s: unsupported encoding %s", __func__, pa_encoding_to_string(encoding));
    }

    return supported;
}

static inline pal_stream_type_t pa_pal_sink_get_type_from_string(const char *stream_type) {
    pal_stream_type_t type;

    if (pa_streq(stream_type, "PAL_STREAM_LOW_LATENCY")) {
        type = PAL_STREAM_LOW_LATENCY;
    } else if (pa_streq(stream_type,"PAL_STREAM_DEEP_BUFFER")) {
        type = PAL_STREAM_DEEP_BUFFER;
    } else if (pa_streq(stream_type,"PAL_STREAM_VOIP_TX")) {
        type = PAL_STREAM_VOIP_TX;
    } else if (pa_streq(stream_type,"PAL_STREAM_VOIP_RX")) {
        type = PAL_STREAM_VOIP_RX;
    } else if (pa_streq(stream_type, "PAL_STREAM_COMPRESSED")) {
        type = PAL_STREAM_COMPRESSED;
    } else {
        type = PAL_STREAM_GENERIC; //No PAL_STREAM_NONE. Hence using generic one.
        pa_log_error("%s: Unsupported stream_type %s", __func__, stream_type);
    }

    return type;
}

#endif
