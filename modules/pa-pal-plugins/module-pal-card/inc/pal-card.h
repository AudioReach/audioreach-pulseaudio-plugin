/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef foomodulepalcardfoo
#define foomodulepalcardfoo

#define PAL_PCM_CHANNEL_FL    1  /* Front left channel.                           */
#define PAL_PCM_CHANNEL_FR    2  /* Front right channel.                          */
#define PAL_PCM_CHANNEL_FC    3  /* Front center channel.                         */
#define PAL_PCM_CHANNEL_LS    4  /* Left surround channel.                        */
#define PAL_PCM_CHANNEL_RS    5  /* Right surround channel.                       */
#define PAL_PCM_CHANNEL_LFE   6  /* Low frequency effect channel.                 */
#define PAL_PCM_CHANNEL_CS    7  /* Center surround channel; Rear center channel. */
#define PAL_PCM_CHANNEL_LB    8  /* Left back channel; Rear left channel.         */
#define PAL_PCM_CHANNEL_RB    9  /* Right back channel; Rear right channel.       */
#define PAL_PCM_CHANNEL_TS   10  /* Top surround channel.                         */
#define PAL_PCM_CHANNEL_CVH  11  /* Center vertical height channel.               */
#define PAL_PCM_CHANNEL_MS   12  /* Mono surround channel.                        */
#define PAL_PCM_CHANNEL_FLC  13  /* Front left of center.                         */
#define PAL_PCM_CHANNEL_FRC  14  /* Front right of center.                        */
#define PAL_PCM_CHANNEL_RLC  15  /* Rear left of center.                          */
#define PAL_PCM_CHANNEL_RRC  16  /* Rear right of center.                         */

typedef enum {
    PA_PAL_CARD_SINK_NONE= 0x0,
    PA_PAL_CARD_SINK_LL_0 = 0x1,
    PA_PAL_CARD_SINK_OFFLOAD_0 = 0x3,
} pa_pal_card_sink_usecase_id_t;

typedef enum {
    PA_PAL_CARD_SOURCE_NONE = 0x0,
    PA_PAL_CARD_SOURCE_LL_0 = 0x7,
} pa_pal_card_source_usecase_id_t;

typedef enum {
    PA_PAL_CARD_AVOID_PROCESSING_FOR_NONE = 0x0,
    PA_PAL_CARD_AVOID_PROCESSING_FOR_SAMPLE_RATE = 0x1,
    PA_PAL_CARD_AVOID_PROCESSING_FOR_BIT_WIDTH = 0x2,
    PA_PAL_CARD_AVOID_PROCESSING_FOR_CHANNELS = 0x4,
    PA_PAL_CARD_AVOID_PROCESSING_FOR_ALL = (PA_PAL_CARD_AVOID_PROCESSING_FOR_SAMPLE_RATE |
                                             PA_PAL_CARD_AVOID_PROCESSING_FOR_BIT_WIDTH |
                                             PA_PAL_CARD_AVOID_PROCESSING_FOR_CHANNELS),
} pa_pal_card_avoid_processing_config_id_t;

typedef struct {
    char *name;
    char *description;

    uint32_t priority;
    pa_available_t available;

    pa_hashmap *ports;
    char **port_conf_string;

    uint32_t n_sinks;
    uint32_t n_sources;

    uint32_t max_sink_channels;
    uint32_t max_source_channels;
} pa_pal_card_profile_config;

typedef struct {
    char *name;
    char *description;
    pa_available_t available;
    pa_direction_t direction;
    pa_sample_spec default_spec;
    pa_channel_map default_map;
    uint32_t priority;
    pal_device_id_t device;

    pa_idxset *formats;

    char *port_type;
    char *detection;
    bool format_detection;
    char **linked_ports;

    char *hdmi_tx_state_path;
    char *state_node_path;
    char *sample_format_node_path;
    char *sample_rate_node_path;
    char *sample_layout_node_path;
    char *sample_channel_node_path;
    char *sample_channel_alloc_node_path;
    char *audio_preemph_node_path;
    char *dsd_rate_node_path;
    char *linkon0_node_path;
    char *poweron_node_path;
    char *audio_path_node_path;
    char *arc_enable_node_path;
    char *earc_enable_node_path;
    char *arc_state_node_path;
    char *arc_sample_format_node_path;
    char *arc_sample_rate_node_path;
    char *arc_audio_preemph_node_path;
    char *channel_status_path;
    char *pal_devicepp_config;
} pa_pal_card_port_config;

typedef union {
    pa_pal_card_source_usecase_id_t source_id;
    pa_pal_card_sink_usecase_id_t sink_id;
} pa_pal_card_usecase_id_t;

typedef enum {
    PA_PAL_CARD_USECASE_TYPE_STATIC = 0,
    PA_PAL_CARD_USECASE_TYPE_DYNAMIC = 1,
} pa_pal_card_usecase_type_t;

typedef enum {
    PA_PAL_NO_EVENT = -1,
    PA_PAL_VOLUME_APPLY = 1,
    PA_PAL_DEVICE_SWITCH,
} pa_pal_ctrl_event_t;

typedef struct {
    pal_device_id_t device;
    pa_pal_card_usecase_id_t usecase_id;
    pa_sample_spec default_spec;
    pa_channel_map default_map;
    bool is_connected;
    char *pal_devicepp_config;
} pa_pal_card_port_device_data;

#endif
