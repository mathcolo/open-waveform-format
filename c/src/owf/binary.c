#include <owf/binary.h>
#include <signal.h>

#define OWF_BINARY_SAFE_SUB32(a, b) \
    do { \
        bool ok; \
        uint32_t tmp = owf_binary_safe_sub32(a, b, &ok); \
        if (OWF_NOEXPECT(!ok)) { \
            OWF_READER_ERRF(binary->reader, "unsigned subtraction error (%" PRIu32 " - %" PRIu32 ")", (uint32_t)a, (uint32_t)b); \
            return false; \
        } else { \
            a = tmp; \
        } \
    } while (0)

#define OWF_BINARY_SAFE_READ(binary, ptr, length) \
    do { \
        /* Length of zero is a no-op */ \
        if (OWF_NOEXPECT(length != 0 && !binary->reader.read(ptr, length, binary->reader.data))) { \
            OWF_READER_ERRF(binary->reader, "read error (%" PRIu32 " bytes)", (uint32_t)length); \
            return false; \
        } else { \
            OWF_BINARY_SAFE_SUB32(binary->segment_length, length); \
        } \
    } while (0)

#define OWF_BINARY_SAFE_VARIABLE_READ(binary, ptr, length) \
    do { \
        if (length > binary->reader.max_alloc) { \
            OWF_READER_ERRF(binary->reader, "max allocation size exceeded (attempted to allocate %" PRIu32 " bytes, max_alloc=%" PRIu32 " bytes)", (uint32_t)length, (uint32_t)binary->reader.max_alloc); \
            return false; \
        } else {\
            ptr = binary->reader.alloc(length); \
            if (ptr == NULL) { \
                OWF_READER_ERRF(binary->reader, "memory allocation failure (%" PRIu32 " bytes, max_alloc=%" PRIu32 " bytes)", (uint32_t)length, (uint32_t)binary->reader.max_alloc); \
                return false; \
            } \
        } \
        \
        if (OWF_NOEXPECT(length != 0 && !binary->reader.read(ptr, length, binary->reader.data))) { \
            OWF_READER_ERRF(binary->reader, "variable read error (%" PRIu32 " bytes)", (uint32_t)length); \
            binary->reader.free(ptr); \
            return false; \
        } else { \
            bool ok; \
            binary->segment_length = owf_binary_safe_sub32(binary->segment_length, length, &ok); \
            if (OWF_NOEXPECT(!ok)) { \
                OWF_READER_ERRF(binary->reader, "out-of-bounds length (%" PRIu32 " bytes for segment length %" PRIu32 ")", (uint32_t)length, binary->segment_length); \
                binary->reader.free(ptr); \
                return false; \
            } \
        } \
    } while (0)

#define OWF_BINARY_READER_VISIT(binary, type) \
    do { \
        if (!OWF_READER_VISIT(binary->reader, type)) { \
            /* Skip the rest of the segment */ \
            binary->skip_length = binary->segment_length; \
            return true; \
        } \
    } while (0)

void owf_binary_reader_init(owf_binary_reader_t *binary, owf_alloc_cb_t alloc_fn, owf_free_cb_t free_fn, owf_read_cb_t read_fn, owf_visit_cb_t visitor, size_t max_alloc, void *data) {
    owf_reader_init(&binary->reader, alloc_fn, free_fn, read_fn, visitor, max_alloc, data);
    binary->segment_length = binary->skip_length = 0;
}

static bool owf_binary_reader_file_read_cb(void *dest, size_t size, void *data) {
    owf_binary_reader_file_t *ptr = (owf_binary_reader_file_t *)data;
    return fread(dest, sizeof(uint8_t), size, ptr->file) == size;
}

void owf_binary_reader_init_file(owf_binary_reader_t *binary, FILE *file, owf_alloc_cb_t alloc_fn, owf_free_cb_t free_fn, owf_visit_cb_t visitor, size_t max_alloc) {
    owf_binary_reader_file_t *ptr = alloc_fn(sizeof(owf_binary_reader_file_t));
    ptr->file = file;
    owf_reader_init(&binary->reader, alloc_fn, free_fn, owf_binary_reader_file_read_cb, visitor, max_alloc, ptr);
}

void owf_binary_reader_destroy_file(owf_binary_reader_t *binary) {
    binary->reader.free(binary->reader.data);
}

const char *owf_binary_reader_strerror(owf_binary_reader_t *binary) {
    return binary->reader.error;
}

bool owf_binary_length_unwrap(owf_binary_reader_t *binary, owf_binary_reader_cb_t cb, void *ptr) {
    uint32_t length, old_length = binary->segment_length;

    /* Run the unwrap */
    if (OWF_NOEXPECT(!owf_binary_length_unwrap_top(binary, cb, &length, ptr))) {
        /* Just restore it */
        binary->segment_length = old_length;
        return false;
    } else {
        /* Subtract the length of what we just read, plus space for the length, and restore the modified length */
        OWF_BINARY_SAFE_SUB32(old_length, length);
        binary->segment_length = old_length;
        return true;
    }
}

bool owf_binary_length_unwrap_top(owf_binary_reader_t *binary, owf_binary_reader_cb_t cb, uint32_t *length_ptr, void *ptr) {
    uint32_t length;

    /* Read the length header */
    OWF_BINARY_SAFE_READ(binary, &length, sizeof(length));
    OWF_HOST32(length);

    /* Verify alignment */
    if (OWF_NOEXPECT(length % sizeof(uint32_t) != 0)) {
        OWF_READER_ERRF(binary->reader, "length was not %lu-byte aligned (got %" PRIu32 " bytes)", sizeof(uint32_t), length);
        return false;
    }

    /* Yield to the callback, and ensure that we have no data left to read after the callback executes */
    binary->segment_length = length;
    binary->skip_length = 0;
    if (OWF_NOEXPECT(!cb(binary, ptr))) {
        return false;
    }

    /* Read skipped bytes */
    while (binary->skip_length > 0) {
        const uint32_t to_skip = OWF_MIN(binary->skip_length, sizeof(binary->skip));
        OWF_BINARY_SAFE_READ(binary, binary->skip, to_skip);
        OWF_BINARY_SAFE_SUB32(binary->skip_length, to_skip);
    }

    /* Ensure we have no trailing bytes */
    if (OWF_NOEXPECT(binary->segment_length > 0)) {
        OWF_READER_ERRF(binary->reader, "trailing data when reading segment: (%" PRIu32 " bytes)", binary->segment_length);
        return false;
    }

    /* Emit the total length that we just read */
    *length_ptr = length + sizeof(uint32_t);
    return true;
}

bool owf_binary_length_unwrap_nested_multi(owf_binary_reader_t *binary, void *ptr) {
    owf_binary_reader_cb_t cb = (owf_binary_reader_cb_t)ptr;
    while (binary->segment_length > 0) {
        if (OWF_NOEXPECT(!owf_binary_length_unwrap(binary, cb, NULL))) {
            return false;
        }
    }
    return true;
}

bool owf_binary_length_unwrap_multi(owf_binary_reader_t *binary, void *ptr) {
    owf_binary_reader_cb_t cb = (owf_binary_reader_cb_t)ptr;
    while (binary->segment_length > 0) {
        if (OWF_NOEXPECT(!cb(binary, NULL))) {
            return false;
        }
    }
    return true;
}

bool owf_binary_read_str(owf_binary_reader_t *binary, void *ptr) {
    owf_str_t *str = (owf_str_t *)ptr;

    /*
     * Read the actual string.
     * The string's length is currently saved in binary->segment_length if this call is wrapped.
     */
    str->length = binary->segment_length;
    OWF_BINARY_SAFE_VARIABLE_READ(binary, str->data, str->length);
    return true;
}

bool owf_binary_read_samples(owf_binary_reader_t *binary, void *ptr) {
    owf_signal_t *signal = &binary->reader.ctx.signal;
    uint32_t length = binary->segment_length, num_samples;

    /* Length is stored in segment_length, ensure it's also double aligned */
    if (OWF_NOEXPECT(length % sizeof(double) != 0)) {
        OWF_READER_ERRF(binary->reader, "length of sample array is not %lu-byte aligned (got %" PRIu32 " bytes)", sizeof(double), length);
        return false;
    }

    /* Read the double array */
    num_samples = length / sizeof(double);
    signal->num_samples = num_samples;
    OWF_BINARY_SAFE_VARIABLE_READ(binary, signal->samples, length);

    /* Byteswap the samples */
    for (uint32_t i = 0; i < num_samples; i++) {
        OWF_HOST64(signal->samples[i]);
    }

    return true;
}

bool owf_binary_read_signal(owf_binary_reader_t *binary, void *ptr) {
    owf_signal_t *signal = &binary->reader.ctx.signal;

    if (!owf_binary_length_unwrap(binary, owf_binary_read_str, &signal->id) ||
        !owf_binary_length_unwrap(binary, owf_binary_read_str, &signal->unit) ||
        !owf_binary_length_unwrap(binary, owf_binary_read_samples, signal)) {
        return false;
    }

    /* Call the visitor */
    OWF_BINARY_READER_VISIT(binary, OWF_READ_SIGNAL);
    return true;
}

bool owf_binary_read_event(owf_binary_reader_t *binary, void *ptr) {
    owf_event_t *event = &binary->reader.ctx.event;

    /* Read the timestamp */
    OWF_BINARY_SAFE_READ(binary, &event->time, sizeof(event->time));
    OWF_HOST64(event->time);

    /* Read the data */
    if (OWF_NOEXPECT(!owf_binary_length_unwrap(binary, owf_binary_read_str, &event->data))) {
        return false;
    }

    /* Call the visitor */
    OWF_BINARY_READER_VISIT(binary, OWF_READ_EVENT);
    return true;
}

bool owf_binary_read_alarm(owf_binary_reader_t *binary, void *ptr) {
    owf_alarm_t *alarm = &binary->reader.ctx.alarm;

    /* Read the timestamp */
    OWF_BINARY_SAFE_READ(binary, &alarm->time, sizeof(alarm->time));
    OWF_HOST64(alarm->time);

    /* Read the data */
    if (OWF_NOEXPECT(!owf_binary_length_unwrap(binary, owf_binary_read_str, &alarm->data))) {
        return false;
    }

    /* Call the visitor */
    OWF_BINARY_READER_VISIT(binary, OWF_READ_ALARM);
    return true;
}

bool owf_binary_read_namespace(owf_binary_reader_t *binary, void *ptr) {
    owf_namespace_t *ns = &binary->reader.ctx.ns;

    /* Read timestamps */
    OWF_BINARY_SAFE_READ(binary, &ns->t0, sizeof(ns->t0));
    OWF_HOST64(ns->t0);
    OWF_BINARY_SAFE_READ(binary, &ns->dt, sizeof(ns->dt));
    OWF_HOST64(ns->dt);

    /* Read the ID */
    if (OWF_NOEXPECT(!owf_binary_length_unwrap(binary, owf_binary_read_str, ns))) {
        return false;
    }

    /* Call the visitor */
    OWF_BINARY_READER_VISIT(binary, OWF_READ_NAMESPACE);

    /* Read children */
    return
        owf_binary_length_unwrap(binary, owf_binary_length_unwrap_multi, owf_binary_read_signal) &&
        owf_binary_length_unwrap(binary, owf_binary_length_unwrap_multi, owf_binary_read_event) &&
        owf_binary_length_unwrap(binary, owf_binary_length_unwrap_multi, owf_binary_read_alarm);
}

bool owf_binary_read_channel(owf_binary_reader_t *binary, void *ptr) {
    owf_channel_t *channel = &binary->reader.ctx.channel;

    /* Read the channel id */
    if (OWF_NOEXPECT(!owf_binary_length_unwrap(binary, owf_binary_read_str, &channel->id))) {
        return false;
    }

    /* Call the visitor */
    OWF_BINARY_READER_VISIT(binary, OWF_READ_CHANNEL);

    /* Read children */
    return owf_binary_length_unwrap_nested_multi(binary, owf_binary_read_namespace);
}

bool owf_binary_read(owf_binary_reader_t *binary) {
    uint32_t magic, length;

    /* Read the implicitly-sized header */
    binary->segment_length = sizeof(magic);
    OWF_BINARY_SAFE_READ(binary, &magic, sizeof(magic));
    OWF_HOST32(magic);

    /* Make sure that the magic is correct */
    if (OWF_NOEXPECT(magic != OWF_MAGIC)) {
        OWF_READER_ERRF(binary->reader, "invalid magic header: %#08x", magic);
        return false;
    }

    /* Reset the segment length again, and start walking the tree */
    binary->segment_length = sizeof(length);
    return owf_binary_length_unwrap_top(binary, owf_binary_length_unwrap_nested_multi, &length, owf_binary_read_channel);
}

uint32_t owf_binary_safe_sub32(uint32_t a, uint32_t b, bool *ok) {
    /* Ensure that the operation will not underflow and that the result will be aligned */
    if (OWF_NOEXPECT(b > a || a % sizeof(uint32_t) != 0 || b % sizeof(uint32_t) != 0)) {
        *ok = false;
    } else {
        *ok = true;
    }
    return a - b;
}
