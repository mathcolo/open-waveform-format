#include <owf.h>
#include <owf/reader.h>
#include <owf/endian.h>

#include <inttypes.h>
#include <stdio.h>

#ifndef OWF_BINARY_H
#define OWF_BINARY_H

#define OWF_MAGIC 0x4f574631UL

typedef struct owf_binary_reader {
    owf_reader_t reader;
    uint32_t segment_length;
} owf_binary_reader_t;

typedef struct owf_binary_reader_file {
    FILE *file;
} owf_binary_reader_file_t;

typedef bool (*owf_binary_reader_cb_t)(owf_binary_reader_t *, void *);

void owf_binary_reader_init(owf_binary_reader_t *binary, owf_alloc_cb_t alloc_fn, owf_free_cb_t free_fn, owf_read_cb_t read_fn, owf_visit_cb_t visitor, void *data);
void owf_binary_reader_init_file(owf_binary_reader_t *binary, FILE *file, owf_alloc_cb_t alloc_fn, owf_free_cb_t free_fn, owf_visit_cb_t visitor);
void owf_binary_reader_destroy_file(owf_binary_reader_t *binary);

const char *owf_binary_reader_strerror(owf_binary_reader_t *binary);

bool owf_binary_length_unwrap(owf_binary_reader_t *binary, owf_binary_reader_cb_t cb, void *ptr);
bool owf_binary_length_unwrap_top(owf_binary_reader_t *binary, owf_binary_reader_cb_t cb, uint32_t *length_ptr, void *ptr);
bool owf_binary_length_unwrap_multi(owf_binary_reader_t *binary, void *ptr);
bool owf_binary_length_unwrap_nested_multi(owf_binary_reader_t *binary, void *ptr);
bool owf_binary_read_str(owf_binary_reader_t *binary, void *ptr);
bool owf_binary_read_channel(owf_binary_reader_t *binary, void *ptr);
bool owf_binary_read_channels(owf_binary_reader_t *binary, void *ptr);
bool owf_binary_read(owf_binary_reader_t *binary);
uint32_t owf_binary_safe_sub32(uint32_t a, uint32_t b, bool *ok);

#endif /* OWF_BINARY_H */
