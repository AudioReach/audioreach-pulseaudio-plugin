/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef foopaljackhfoo
#define foopaljackhfoo

#include <pulsecore/module.h>
#include <pulsecore/thread.h>
#include <pal-sink.h>

typedef enum {
    PA_PAL_JACK_TYPE_INVALID = -1,
    PA_PAL_JACK_TYPE_WIRED_HEADSET = 0x1,
    PA_PAL_JACK_TYPE_WIRED_HEADPHONE = 0x2,
    PA_PAL_JACK_TYPE_LINEOUT = 0x4,
    PA_PAL_JACK_TYPE_WIRED_HEADSET_BUTTONS = 0x8,
    PA_PAL_JACK_TYPE_HDMI_IN = 0x10,
    PA_PAL_JACK_TYPE_BTA2DP_OUT = 0x20,
    PA_PAL_JACK_TYPE_BTA2DP_IN = 0x40,
    PA_PAL_JACK_TYPE_HDMI_ARC = 0x80,
    PA_PAL_JACK_TYPE_SPDIF = 0x100,
    PA_PAL_JACK_TYPE_BTSCO_IN = 0x200,
    PA_PAL_JACK_TYPE_BTSCO_OUT = 0x400,
    PA_PAL_JACK_TYPE_HDMI_OUT = 0x800,
    PA_PAL_JACK_TYPE_SPDIF_OUT_OPTICAL = 0x1000,
    PA_PAL_JACK_TYPE_SPDIF_OUT_COAXIAL = 0x2000,
    PA_PAL_JACK_TYPE_DISPLAY_IN = 0x4000,
    PA_PAL_JACK_TYPE_LAST = PA_PAL_JACK_TYPE_DISPLAY_IN,
    PA_PAL_JACK_TYPE_MAX = PA_PAL_JACK_TYPE_LAST,
} pa_pal_jack_type_t;

typedef enum {
    PA_PAL_JACK_ERROR,
    PA_PAL_JACK_AVAILABLE,
    PA_PAL_JACK_UNAVAILABLE,
    PA_PAL_JACK_CONFIG_UPDATE,
    PA_PAL_JACK_NO_VALID_STREAM,
    PA_PAL_JACK_SET_PARAM,
} pa_pal_jack_event_t;

typedef struct pa_pal_jack_event_data {
    pa_pal_jack_type_t jack_type;
    pa_pal_jack_event_t event;
    void *pa_pal_jack_info; /* can be used to send any info related to a jack */
} pa_pal_jack_event_data_t;

typedef size_t pa_pal_jack_handle_t;

struct jack_userdata {
    pa_pal_jack_type_t jack_type;
    pa_hook_slot *hook_slot;
};

typedef struct {
    const char *audio_state;
    const char *audio_format;
    const char *audio_rate;
    const char *audio_layout;
    const char *audio_channel;
    const char *audio_channel_alloc;
    const char *audio_preemph;
    const char *dsd_rate;

    const char *linkon_0;
    const char *power_on;
    const char *audio_path;
    const char *arc_enable;
    const char *earc_enable;

    const char *arc_audio_state;
    const char *arc_audio_format;
    const char *arc_audio_rate;
    const char *arc_audio_preemph;

    const char *hdmi_tx_state;
    const char *channel_status;
} pa_pal_jack_sys_path;

typedef struct {
    pa_pal_jack_sys_path jack_sys_path;
    char **linked_ports;
} pa_pal_jack_in_config;

typedef pa_hook_result_t (* pa_pal_jack_callback_t) (void *dummy __attribute__((unused)), pa_pal_jack_event_data_t *event_data, void *client_data);

pa_pal_jack_handle_t *pa_pal_jack_register_event_callback(pa_pal_jack_type_t jack_type, pa_pal_jack_callback_t callback, pa_module *m,
                         pa_pal_jack_in_config *jack_in_config, void *client_data, bool is_external);
bool pa_pal_jack_deregister_event_callback(pa_pal_jack_handle_t *handle, pa_module *m, bool is_external);

#endif
