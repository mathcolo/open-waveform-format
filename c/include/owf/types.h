#include <owf.h>
#include <owf/array.h>
#include <owf/alloc.h>

#ifndef OWF_TYPES_H
#define OWF_TYPES_H

typedef int64_t owf_time_t;

typedef struct owf_str {
    owf_array_t bytes;
} owf_str_t;

typedef struct owf_signal {
    owf_str_t id;
    owf_str_t unit;
    owf_array_t samples;
} owf_signal_t;

typedef struct owf_event {
    owf_time_t time;
    owf_str_t data;
} owf_event_t;

typedef struct owf_alarm {
    owf_time_t time;
    owf_str_t data;
} owf_alarm_t;

typedef struct owf_namespace {
    owf_str_t id;
    owf_time_t t0, dt;
    owf_array_t signals, events, alarms;
} owf_namespace_t;

typedef struct owf_channel {
    owf_str_t id;
    owf_array_t namespaces;
} owf_channel_t;

typedef struct owf {
    owf_array_t channels;
} owf_t;

bool owf_str_init(owf_str_t *str, owf_alloc_t *allocator, owf_error_t *error, uint32_t length);
uint32_t owf_str_bytesize(owf_str_t *str);

#endif /* OWF_TYPES_H */
