/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/core-util.h>
#include <pulsecore/thread.h>

#include "pal-jack-common.h"
#include "pal-jack-format.h"
#include "pal-utils.h"

#define PAL_DBUS_OBJECT_PATH_PREFIX        "/org/pulseaudio/ext/pal/port"
#define PAL_DBUS_MODULE_IFACE              "org.PulseAudio.Ext.Pal.Module"

/* Module data */
typedef struct {
    char *obj_path;
    pa_dbus_protocol *dbus_protocol;
    pa_hook event_hook;
    pa_pal_jack_type_t jack_type;
} pa_pal_external_jack_data;

typedef enum {
    JACK_THREAD_STATE_IDLE,
    JACK_THREAD_STATE_SET_PARAM,
    JACK_THREAD_STATE_EXIT,
} jack_ext_async_thread_state_t;

enum signal_index {
    SIGNAL_JACK_SET_PARAM_DONE_EVENT,
    SIGNAL_MAX
};

typedef struct {
    char *param;
    int jack_ref_count; /* Number of jacks sharing async thread */
    jack_ext_async_thread_state_t thread_state;
    pa_thread *async_thread;
    pa_mutex *mutex;
    pa_cond *cond;
    pa_pal_external_jack_data *external_jdata;
} pa_pal_async_thread_data;

pa_pal_async_thread_data *async_thr_data = NULL;

enum module_method_handler_index {
    METHOD_HANDLER_BT_CONNECT,
    METHOD_HANDLER_SET_PARAM,
    METHOD_HANDLER_START_STREAM,
    METHOD_HANDLER_STOP_STREAM,
    METHOD_HANDLER_MODULE_LAST = METHOD_HANDLER_STOP_STREAM,
    METHOD_HANDLER_MODULE_MAX = METHOD_HANDLER_MODULE_LAST + 1,
};

static char const *jack_prmkey_names[JACK_PARAM_KEY_MAX] = {
    [JACK_PARAM_KEY_DEVICE_CONNECTION]          = "device_connection",
    [JACK_PARAM_KEY_A2DP_SUSPEND]               = "a2dp_suspend",
    [JACK_PARAM_KEY_DEVICE_SAMPLERATE]          = "sample_rate",
};

static void pal_jack_external_bt_connection(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void pal_jack_external_set_param(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void pal_jack_external_start_stream(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void pal_jack_external_stop_stream(DBusConnection *conn, DBusMessage *msg, void *userdata);

static pa_dbus_arg_info connection_args[] = {
    {"connection_args", "b", "in"},
};
static pa_dbus_arg_info set_param_args[] = {
    {"param_string", "s", "in"},
};
static pa_dbus_arg_info start_stream_args[] = {
    {"stream_config", "(suss)", "in"},
};
static pa_dbus_arg_info stop_stream_args[] = {
};

pa_dbus_arg_info set_param_done_event_args[] = {
    {"status", "i", NULL},
};

static pa_dbus_method_handler module_method_handlers[METHOD_HANDLER_MODULE_MAX] = {
    [METHOD_HANDLER_BT_CONNECT] = {
        .method_name = "BtConnect",
        .arguments = connection_args,
        .n_arguments = sizeof(connection_args)/sizeof(pa_dbus_arg_info),
        .receive_cb = pal_jack_external_bt_connection },
    [METHOD_HANDLER_SET_PARAM] = {
        .method_name = "SetParam",
        .arguments = set_param_args,
        .n_arguments = sizeof(set_param_args)/sizeof(pa_dbus_arg_info),
        .receive_cb = pal_jack_external_set_param },
    [METHOD_HANDLER_START_STREAM] = {
        .method_name = "StartStream",
        .arguments = start_stream_args,
        .n_arguments = sizeof(start_stream_args)/sizeof(pa_dbus_arg_info),
        .receive_cb = pal_jack_external_start_stream },
    [METHOD_HANDLER_STOP_STREAM] = {
        .method_name = "StopStream",
        .arguments = stop_stream_args,
        .n_arguments = sizeof(stop_stream_args)/sizeof(pa_dbus_arg_info),
        .receive_cb = pal_jack_external_stop_stream },
};

static pa_dbus_signal_info det_event_signals[SIGNAL_MAX] = {
    [SIGNAL_JACK_SET_PARAM_DONE_EVENT] = {
        .name = "JackSetParamDone",
        .arguments = set_param_done_event_args,
        .n_arguments = sizeof(set_param_done_event_args)/sizeof(pa_dbus_arg_info)},
};

static pa_dbus_interface_info module_interface_info = {
    .name = PAL_DBUS_MODULE_IFACE,
    .method_handlers = module_method_handlers,
    .n_method_handlers = METHOD_HANDLER_MODULE_MAX,
    .property_handlers = NULL,
    .n_property_handlers = 0,
    .get_all_properties_cb = NULL,
    .signals = det_event_signals,
    .n_signals = SIGNAL_MAX
};

static void set_default_config(pa_pal_jack_type_t jack_type, pa_pal_jack_out_config *config) {
    config->preemph_status = 0;
    config->ss.format = PA_SAMPLE_S16LE;
    config->encoding = PA_ENCODING_PCM;
    if (jack_type == PA_PAL_JACK_TYPE_BTA2DP_OUT)
        config->ss.rate = 48000;
    else
        config->ss.rate = 16000;
    config->ss.channels = 2;
    pa_channel_map_init(&(config->map));
    pa_channel_map_init_auto(&(config->map), 2, PA_CHANNEL_MAP_DEFAULT);
}

static void pal_jack_external_bt_connection(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_pal_jack_event_data_t event_data;
    pa_pal_external_jack_data *external_jdata = userdata;
    bool is_connect = false;

    DBusError error;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(userdata);

    dbus_error_init(&error);

    pa_log_debug("%s", __func__);

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_BOOLEAN, &is_connect, DBUS_TYPE_INVALID)) {
        pa_log_error("Invalid signature for SetParam - %s\n", error.message);
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Invalid signature for SetParam");
        dbus_error_free(&error);
        return;
    }

    if (is_connect) {
        event_data.event = PA_PAL_JACK_AVAILABLE;
        pa_log_info("pal jack type %d available", external_jdata->jack_type);
    }
    else {
        event_data.event = PA_PAL_JACK_UNAVAILABLE;
        pa_log_info("pal jack type %d unavailable", external_jdata->jack_type);
    }

    /* Generate jack available event */
    event_data.jack_type = external_jdata->jack_type;
    pa_hook_fire(&(external_jdata->event_hook), &event_data);

    pa_dbus_send_empty_reply(conn, msg);

}

static void pal_jack_external_set_param(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    int ret = 0;
    pa_pal_jack_event_data_t event_data;
    pa_pal_external_jack_data *external_jdata = userdata;

    DBusError error;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(userdata);
    pa_assert(async_thr_data);

    dbus_error_init(&error);

    pa_log_debug("%s", __func__);

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &async_thr_data->param, DBUS_TYPE_INVALID)) {
        pa_log_error("Invalid signature for SetParam - %s\n", error.message);
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Invalid signature for SetParam");
        dbus_error_free(&error);
        return;
    }

    pa_mutex_lock(async_thr_data->mutex);
    pa_log_info("%s: external source port %s  set param %s", __func__,
            pa_pal_util_get_port_name_from_jack_type(external_jdata->jack_type), async_thr_data->param);

    async_thr_data->thread_state = JACK_THREAD_STATE_SET_PARAM;
    async_thr_data->external_jdata = external_jdata;
    pa_cond_signal(async_thr_data->cond, 0);
    pa_mutex_unlock(async_thr_data->mutex);

    pa_dbus_send_empty_reply(conn, msg);
}

static int parse_keyidx(const char *keystr)
{
    int key_idx = 0;
    for (key_idx = 1; key_idx < JACK_PARAM_KEY_MAX; key_idx++) {
        if (!strcmp(keystr, jack_prmkey_names[key_idx])) {
            break;
        }
    }

    if ((key_idx > 0) && (key_idx < JACK_PARAM_KEY_MAX))
        return key_idx;
    else
        return -1;
}

int pa_pal_external_jack_parse_kvpair(const char *kvpair, jack_prm_kvpair_t *kv)
{
    int ret = 0;
    int key_idx = 0;
    char *key_name, *value, *kvstr, *tmpstr;

    pa_assert(kvpair);
    pa_assert(kv);

    kvstr = strdup(kvpair);
    pa_assert(kvstr);
    key_name = strtok_r(kvstr, "=", &tmpstr);

    if (key_name == NULL) {
        ret = -EINVAL;
        goto exit;
    }

    key_idx = parse_keyidx(key_name);
    if (key_idx != -1) {
        value = strtok_r(NULL, "=", &tmpstr);
        if (value == NULL) {
            ret = -EINVAL;
            goto exit;
        }
        kv->value = strdup(value);
        kv->key = key_idx;
    }
    else {
        ret = -EINVAL;
    }

exit:
    free(kvstr);
    return ret;
}

static dbus_uint32_t pal_jack_external_get_array_size(DBusMessageIter array) {
    dbus_uint32_t cnt = 0;
    int arg_type;

    while ((arg_type = dbus_message_iter_get_arg_type(&array)) != DBUS_TYPE_INVALID) {
        cnt++;
        dbus_message_iter_next(&array);
    }

    return cnt;
}

static void pal_jack_external_start_stream(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_pal_jack_out_config config;
    pa_pal_jack_event_data_t event_data;
    pa_pal_external_jack_data *external_jdata = userdata;

    char *encoding_str = NULL;
    char *format_str = NULL;
    char *map = NULL;
    uint32_t rate;

    DBusMessageIter struct_i, arg_i;
    DBusError error;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(userdata);

    dbus_error_init(&error);

    pa_log_debug("%s", __func__);

    if (!dbus_message_iter_init(msg, &arg_i)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                "pal_jack_external_start_stream has no arguments");
        dbus_error_free(&error);
        return;
    }

    if (!pa_streq(dbus_message_get_signature(msg),
                start_stream_args[0].type)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                "Invalid signature for start_stream");
        dbus_error_free(&error);
        return;
    }

    dbus_message_iter_recurse(&arg_i, &struct_i);
    dbus_message_iter_get_basic(&struct_i, &encoding_str);

    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &rate);

    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &format_str);

    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &map);

    pa_log_info("%s: external source port %s, encoding %s, rate %d, format  %s map %s", __func__,
                            pa_pal_util_get_port_name_from_jack_type(external_jdata->jack_type),
                                                            encoding_str, rate, format_str, map);

    /* Initialize parameters to default values */
    config.ss.format = PA_SAMPLE_S16LE;
    config.encoding = pa_encoding_from_string(encoding_str);

    if (config.encoding == PA_ENCODING_INVALID) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Unsupported encoding %s", encoding_str);
        dbus_error_free(&error);
        return;
    } else if (config.encoding == PA_ENCODING_PCM) {
        config.ss.format = pa_parse_sample_format(format_str);
    }

    if (!pa_channel_map_parse(&config.map, map)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Unsupported channel map %s", map);
        dbus_error_free(&error);
        return;
    }

    config.ss.rate = rate;
    config.ss.channels = config.map.channels;

    event_data.jack_type = external_jdata->jack_type;

    /* generate jack available event */
    event_data.event = PA_PAL_JACK_AVAILABLE;
    pa_hook_fire(&(external_jdata->event_hook), &event_data);

    /* generate jack config update event */
    event_data.pa_pal_jack_info = &config;
    event_data.event = PA_PAL_JACK_CONFIG_UPDATE;
    pa_hook_fire(&(external_jdata->event_hook), &event_data);

    pa_dbus_send_empty_reply(conn, msg);
}

static void signal_jack_set_param_done(pa_pal_external_jack_data *external_jdata, int status) {
    DBusMessage *message = NULL;
    DBusMessageIter arg_i;

    pa_log_info("Jack set param done event received");

    pa_assert_se(message = dbus_message_new_signal(external_jdata->obj_path,
                module_interface_info.name,
                det_event_signals[SIGNAL_JACK_SET_PARAM_DONE_EVENT].name));
    dbus_message_iter_init_append(message, &arg_i);
    dbus_message_iter_append_basic(&arg_i, DBUS_TYPE_INT32, &status);
    pa_dbus_protocol_send_signal(external_jdata->dbus_protocol, message);
    dbus_message_unref(message);
}

static void jack_ext_async_thread_func(void *userdata) {
    pa_pal_jack_event_data_t event_data;
    pa_pal_jack_out_config config;
    jack_prm_kvpair_t kvpair;
    pa_hook_result_t ret = PA_HOOK_OK;

    pa_log_debug("Starting Jack ext Async Thread");

    pa_mutex_lock(async_thr_data->mutex);
    while (async_thr_data->thread_state != JACK_THREAD_STATE_EXIT) {
        pa_log_debug("Async Thread wait");
        pa_cond_wait(async_thr_data->cond, async_thr_data->mutex);
        pa_log_debug("Async Thread wakeup");

        if (async_thr_data->thread_state == JACK_THREAD_STATE_SET_PARAM) {
            pa_mutex_unlock(async_thr_data->mutex);
            pa_log_debug("Param to be set- %s", async_thr_data->param);

            /* Generate jack set param event */
            event_data.jack_type = async_thr_data->external_jdata->jack_type;
            event_data.event = PA_PAL_JACK_SET_PARAM;
            event_data.pa_pal_jack_info = (void *)async_thr_data->param;
            ret = pa_hook_fire(&(async_thr_data->external_jdata->event_hook), &event_data);

signal:
            pa_mutex_lock(async_thr_data->mutex);
            pa_log_debug("Sending signal for set param done. success = %d\n", ret);
            signal_jack_set_param_done(async_thr_data->external_jdata, ret);
        }
    }
    pa_mutex_unlock(async_thr_data->mutex);

    pa_log_debug("Exiting Async Thread");
}

struct pa_pal_jack_data* pa_pal_external_jack_detection_enable(pa_pal_jack_type_t jack_type, pa_module *m,
        pa_hook_slot **hook_slot, pa_pal_jack_callback_t callback, void *client_data) {
    struct pa_pal_jack_data *jdata = NULL;
    pa_pal_external_jack_data *external_jdata = NULL;
    const char *port_name = NULL;
    char *port_name_underscore = NULL;

    jdata = pa_xnew0(struct pa_pal_jack_data, 1);

    external_jdata = pa_xnew0(pa_pal_external_jack_data, 1);
    jdata->prv_data = external_jdata;

    port_name = pa_pal_util_get_port_name_from_jack_type(jack_type);
    if (!port_name) {
        pa_log_error("Invalid port jack %d\n", jack_type);
        return NULL;
    }

    /* replace hyphen with underscore as in dbus doesn't allow hyphen in name */
    port_name_underscore = pa_replace(port_name, "-", "_");

    external_jdata->obj_path = pa_sprintf_malloc("%s/%s", PAL_DBUS_OBJECT_PATH_PREFIX, port_name_underscore);
    external_jdata->dbus_protocol = pa_dbus_protocol_get(m->core);

    pa_xfree(port_name_underscore);

    /* Create async thread once for all jacks */
    if (!async_thr_data) {
        async_thr_data = pa_xnew0(pa_pal_async_thread_data, 1);
        async_thr_data->mutex = pa_mutex_new(false /* recursive  */, false /* inherit_priority */);
        async_thr_data->cond = pa_cond_new();
        async_thr_data->thread_state = JACK_THREAD_STATE_IDLE;
        if (!(async_thr_data->async_thread = pa_thread_new("jack_external_async_thread", jack_ext_async_thread_func, NULL)))
            pa_log_error("%s: Creation of async thread for set_param failed", __func__);
    }
    async_thr_data->jack_ref_count++;

    pa_assert_se(pa_dbus_protocol_add_interface(external_jdata->dbus_protocol, external_jdata->obj_path, &module_interface_info, external_jdata) >= 0);

    external_jdata->jack_type = jack_type;
    jdata->jack_type = jack_type;

    pa_hook_init(&(external_jdata->event_hook), NULL);
    jdata->event_hook = &(external_jdata->event_hook);

    *hook_slot = pa_hook_connect(&(external_jdata->event_hook), PA_HOOK_NORMAL, (pa_hook_cb_t)callback, client_data);

    return jdata;
}

void pa_pal_external_jack_detection_disable(struct pa_pal_jack_data *jdata, pa_module *m) {
    pa_pal_external_jack_data *external_jdata;
    pa_assert(jdata);

    external_jdata = (pa_pal_external_jack_data *)jdata->prv_data;

    async_thr_data->jack_ref_count--;

    /* Destroy async thread once all jacks are disabled */
    if (!async_thr_data->jack_ref_count) {
        async_thr_data->thread_state = JACK_THREAD_STATE_EXIT;
        pa_cond_signal(async_thr_data->cond, 0);
        pa_thread_free(async_thr_data->async_thread);
        pa_cond_free(async_thr_data->cond);
        pa_mutex_free(async_thr_data->mutex);
        pa_xfree(async_thr_data);
        async_thr_data = NULL;
    }

    pa_assert_se(pa_dbus_protocol_remove_interface(external_jdata->dbus_protocol, external_jdata->obj_path, module_interface_info.name) >= 0);

    pa_dbus_protocol_unref(external_jdata->dbus_protocol);

    pa_xfree(external_jdata->obj_path);

    pa_hook_done(&(external_jdata->event_hook));

    pa_xfree(external_jdata);

    pa_xfree(jdata);
    jdata = NULL;
}

static void pal_jack_external_stop_stream(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_pal_external_jack_data *external_jdata = userdata;
    pa_pal_jack_event_data_t event_data;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(userdata);

    pa_log_debug("%s\n", __func__);

    /* generate jack config update event */
    event_data.jack_type = external_jdata->jack_type;
    event_data.event = PA_PAL_JACK_UNAVAILABLE;
    pa_hook_fire(&(external_jdata->event_hook), &event_data);

    pa_dbus_send_empty_reply(conn, msg);
}
