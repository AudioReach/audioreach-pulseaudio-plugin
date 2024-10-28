/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef foopalpasourcehfoo
#define foopalpasourcehfoo

#include <pulse/sample.h>
#include <pulsecore/card.h>
#include <pulsecore/core.h>

#include <PalApi.h>
#include <PalDefs.h>

#include "pal-card.h"

typedef size_t pa_pal_source_handle_t;

typedef struct {
    pa_pal_source_handle_t *handle;
} pa_pal_card_source_info;

typedef struct {
    char *name;
    char *description;
    char *pal_devicepp_config;
    int id;
    pal_stream_type_t stream_type;
    pa_sample_spec default_spec;
    pa_encoding_t default_encoding;
    pa_channel_map default_map;
    uint32_t alternate_sample_rate;
    bool use_hw_volume;
    pa_pal_card_avoid_processing_config_id_t avoid_config_processing;
    pa_idxset *formats;
    pa_hashmap *ports;
    pa_hashmap *profiles;
    char **port_conf_string;
    pa_pal_card_usecase_type_t usecase_type;
    uint32_t buffer_size;
    uint32_t buffer_count;
} pa_pal_source_config;

typedef struct {
    pal_stream_handle_t *stream_handle;

    struct pal_device *pal_device;
    struct pal_stream_attributes *stream_attributes;
    const char *device_url;

    int write_fd;

    pa_mutex *mutex;
    pa_cond *cond_ctrl_thread;
    pa_pal_ctrl_event_t source_event_id;

    size_t buffer_size;
    size_t buffer_count;
    int index;
    bool dynamic_usecase;

    bool standby;
} pal_source_data;

typedef struct {
    bool first;
    pa_source *source;
    pa_rtpoll *rtpoll;
    pa_thread_mq thread_mq;
    pa_thread *thread;
    pa_idxset *formats;
    pa_pal_card_avoid_processing_config_id_t avoid_config_processing;
} pa_source_data;

typedef struct {
    pal_source_data *pal_sdata;
    pa_source_data *pa_sdata;
    bool pal_source_opened;
} pa_pal_source_data;

/*create pal session and pa source */
int pa_pal_source_create(pa_module *m, pa_card *card, const char *driver, const char *module_name, pa_pal_source_config *source,
                         pa_pal_source_handle_t **handle);
void pa_pal_source_close(pa_pal_source_handle_t *handle);
bool pa_pal_source_is_supported_sample_rate(uint32_t sample_rate);
pa_idxset* pa_pal_source_get_config(pa_pal_source_handle_t *handle);
int pa_pal_source_get_media_config(pa_pal_source_handle_t *handle, pa_sample_spec *ss, pa_channel_map *map, pa_encoding_t *encoding);
int pa_pal_source_set_device_connection_params(pa_pal_source_handle_t *handle, const char *prm_value);

static inline bool pa_pal_source_is_supported_type(char *source_type) {
    pa_assert(source_type);

    if (pa_streq(source_type, "low-latency") || pa_streq(source_type, "regular") || pa_streq(source_type, "compress") || pa_streq(source_type, "passthrough"))
        return true;

    return false;
}

static inline bool pa_pal_source_is_supported_encoding(pa_encoding_t encoding) {
    bool supported = true;

    switch (encoding) {
        case PA_ENCODING_PCM:
#ifndef PAL_DISABLE_COMPRESS_AUDIO_SUPPORT
        case PA_ENCODING_UNKNOWN_IEC61937:
        case PA_ENCODING_UNKNOWN_4X_IEC61937:
        case PA_ENCODING_UNKNOWN_HBR_IEC61937:
#endif
            break;

        default :
            supported = false;
            pa_log_error("%s: unsupported encoding %s", __func__, pa_encoding_to_string(encoding));
    }

    return supported;
}

static inline pal_stream_type_t pa_pal_source_get_type_from_string(const char *stream_type) {
    pal_stream_type_t type;

    if (pa_streq(stream_type, "PAL_STREAM_LOW_LATENCY")) {
        type = PAL_STREAM_LOW_LATENCY;
    } else if (pa_streq(stream_type,"PAL_STREAM_DEEP_BUFFER")) {
        type = PAL_STREAM_DEEP_BUFFER;
    } else if (pa_streq(stream_type, "PAL_STREAM_COMPRESSED")) {
        type = PAL_STREAM_COMPRESSED;
    } else if (pa_streq(stream_type, "PAL_STREAM_VOIP_TX")) {
        type = PAL_STREAM_VOIP_TX;
    } else if (pa_streq(stream_type, "PAL_STREAM_VOIP_RX")) {
        type = PAL_STREAM_VOIP_RX;
    } else if (pa_streq(stream_type, "PAL_STREAM_RAW")) {
        type = PAL_STREAM_RAW;
    } else {
        type = PAL_STREAM_GENERIC;
        pa_log_error("%s: Unsupported flag name %s", __func__, stream_type);
    }

    return type;
}

#endif
