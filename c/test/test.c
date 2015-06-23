#include <owf.h>
#include <owf/types.h>
#include <owf/binary.h>
#include <owf/reader.h>
#include <owf/platform.h>
#include <owf/version.h>

#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define OWF_TEST_SOFT_FAIL(str) {owf_test_fail(str); return 1;}
#define OWF_TEST_SOFT_FAILF(str, ...) {owf_test_fail(str, __VA_ARGS__); return 1;}
#define OWF_TEST_FAIL(str) {owf_test_fail(str); return 2;}
#define OWF_TEST_FAILF(str, ...) {owf_test_fail(str, __VA_ARGS__); return 2;}
#define OWF_TEST_OK return 0
#define OWF_TEST_PATH_TO(file) ("../example/owf1_" file ".owf")

#define OWF_TEST_VISITOR_FILE(str, result) owf_test_binary_visitor_file_execute(OWF_TEST_PATH_TO(str), result)
#define OWF_TEST_VISITOR_BUFFER(str, result) owf_test_binary_visitor_buffer_execute(OWF_TEST_PATH_TO(str), result)
#define OWF_TEST_MATERIALIZE_FILE(str, result) owf_test_binary_materialize_file_execute(OWF_TEST_PATH_TO(str), result)
#define OWF_TEST_MATERIALIZE_BUFFER(str, result) owf_test_binary_materialize_buffer_execute(OWF_TEST_PATH_TO(str), result)

static bool owf_test_verbose;

typedef struct owf_test {
    const char *name;
    int (*fn)(void);
} owf_test_t;

static void owf_test_fail(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    printf("\n");
    va_end(va);
}

/* ---------- HELPERS ---------- */
static void owf_test_print_channel(owf_channel_t *channel) {
    if (owf_test_verbose) {
        fprintf(stderr, "[CHANNEL] %s\n", OWF_STR_PTR(channel->id));
    }
}

static void owf_test_print_namespace(owf_namespace_t *namespace) {
    if (owf_test_verbose) {
        fprintf(stderr, "  [NS] %s <t0=" OWF_PRINT_U64 ", dt=" OWF_PRINT_U64 ">\n", OWF_STR_PTR(namespace->id), namespace->t0, namespace->dt);
    }
}

static void owf_test_print_signal(owf_signal_t *signal) {
    if (owf_test_verbose) {
        fprintf(stderr, "    [SIGNAL] <id=%s, units=%s> [", OWF_STR_PTR(signal->id), OWF_STR_PTR(signal->unit));
        for (uint32_t i = 0; i < OWF_ARRAY_LEN(signal->samples); i++) {
            double d = OWF_ARRAY_GET(signal->samples, double, i);
            if (isfinite(d)) {
                fprintf(stderr, "\"%.2f\"", d);
            } else if (isnan(d)) {
                fprintf(stderr, "\"NaN\"");
            } else if (d < 0) {
                fprintf(stderr, "\"-Infinity\"");
            } else {
                fprintf(stderr, "\"Infinity\"");
            }
            if (OWF_ARRAY_LEN(signal->samples) > 0 && i < OWF_ARRAY_LEN(signal->samples) - 1) {
                fprintf(stderr, ", ");
            }
        }
        fprintf(stderr, "]\n");
    }
}

static void owf_test_print_event(owf_event_t *event) {
    if (owf_test_verbose) {
        fprintf(stderr, "    [EVENT] <message=%s, t0=" OWF_PRINT_U64 ">\n", OWF_STR_PTR(event->message), event->t0);
    }
}

static void owf_test_print_alarm(owf_alarm_t *alarm) {
    if (owf_test_verbose) {
        fprintf(stderr, "    [ALARM] <type=%s, message=%s, t0=" OWF_PRINT_U64 ", dt=" OWF_PRINT_U64 ", level=" OWF_PRINT_U8 ", volume=" OWF_PRINT_U8 ">\n", OWF_STR_PTR(alarm->type), OWF_STR_PTR(alarm->message), alarm->t0, alarm->dt, alarm->details.level, alarm->details.volume);
    }
}

static bool owf_test_visitor(owf_reader_t *reader, owf_reader_ctx_t *ctx, owf_reader_cb_type_t type, void *data) {
    switch (type) {
        case OWF_READ_CHANNEL:
            owf_test_print_channel(&ctx->channel);
            owf_channel_destroy(&ctx->channel, reader->alloc);
            break;
        case OWF_READ_NAMESPACE:
            owf_test_print_namespace(&ctx->ns);
            owf_namespace_destroy(&ctx->ns, reader->alloc);
            break;
        case OWF_READ_SIGNAL:
            owf_test_print_signal(&ctx->signal);
            owf_signal_destroy(&ctx->signal, reader->alloc);
            break;
        case OWF_READ_EVENT:
            owf_test_print_event(&ctx->event);
            owf_event_destroy(&ctx->event, reader->alloc);
            break;
        case OWF_READ_ALARM:
            owf_test_print_alarm(&ctx->alarm);
            owf_alarm_destroy(&ctx->alarm, reader->alloc);
            break;
        default:
            break;
    }
    return true;
}

static bool owf_test_binary_read_file(const char *filename, owf_binary_reader_t *binary, owf_alloc_t *alloc, owf_buffer_t *buf, owf_reader_visit_cb_t visitor) {
    FILE *f = fopen(filename, "rb");
    void *dest;
    size_t size;
    long val;

    if (f == NULL) {
        return false;
    }

    /* Seek to the end */
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    /* Get the buffer position */
    if ((val = ftell(f)) == -1L) {
        fclose(f);
        return false;
    } else {
        size = (size_t)val;
    }
    
    /* Rewind the file */
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    /* Allocate space for it */
    dest = owf_malloc(alloc, &binary->reader.error, size);
    if (dest == NULL) {
        fclose(f);
        return false;
    }

    /* Copy to the buffer */
    if (fread(dest, 1, size, f) != size) {
        fclose(f);
        owf_free(alloc, dest);
        return false;
    }

    /* Write the pointer and length to the reader_buffer_t */
    owf_buffer_init(buf, dest, size);
    fclose(f);

    /* Init the binary reader */
    owf_binary_reader_init_buffer(binary, buf, alloc, visitor);

    return true;
}

static bool owf_test_binary_open(owf_binary_reader_t *reader, const char *filename, owf_alloc_t *alloc, owf_reader_visit_cb_t visitor) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return false;
    }
    owf_binary_reader_init_file(reader, f, alloc, visitor);
    return true;
}

static void owf_test_binary_file_close(owf_binary_reader_t *reader) {
    fclose((FILE *)reader->reader.data);
}

static void owf_test_binary_buffer_close(owf_binary_reader_t *reader) {
    owf_free(reader->reader.alloc, ((owf_buffer_t *)reader->reader.data)->ptr);
}

/* ---------- TESTS ---------- */
static int owf_test_binary_visitor_file_execute(const char *filename, bool result) {
    owf_alloc_t alloc = {.malloc = malloc, .realloc = realloc, .free = free, .max_alloc = OWF_ALLOC_DEFAULT_MAX};
    owf_binary_reader_t reader;

    if (!owf_test_binary_open(&reader, filename, &alloc, owf_test_visitor)) {
        OWF_TEST_FAIL("error opening file");
    }

    fprintf(stderr, "\n");
    if (owf_binary_read(&reader) != result) {
        OWF_TEST_FAILF("unexpected result when reading OWF in file mode: %s", owf_binary_reader_strerror(&reader));
    }
    fprintf(stdout, "** result: %s\n", owf_binary_reader_strerror(&reader));
    owf_test_binary_file_close(&reader);
    OWF_TEST_OK;
}

static int owf_test_binary_visitor_buffer_execute(const char *filename, bool result) {
    owf_alloc_t alloc = {.malloc = malloc, .realloc = realloc, .free = free, .max_alloc = OWF_ALLOC_DEFAULT_MAX};
    owf_binary_reader_t reader;
    owf_buffer_t buf;

    if (!owf_test_binary_read_file(filename, &reader, &alloc, &buf, owf_test_visitor)) {
        OWF_TEST_FAIL("error reading file");
    }

    fprintf(stderr, "\n");
    if (owf_binary_read(&reader) != result) {
        OWF_TEST_FAILF("unexpected result when reading OWF in buffer mode: %s", owf_binary_reader_strerror(&reader));
    }
    fprintf(stdout, "** result: %s\n", owf_binary_reader_strerror(&reader));
    owf_test_binary_buffer_close(&reader);

    OWF_TEST_OK;
}

static int owf_test_binary_materialize_print(owf_binary_reader_t *reader, owf_t *owf) {
    fprintf(stderr, "\n");
    for (size_t channel_idx = 0; channel_idx < OWF_ARRAY_LEN(owf->channels); channel_idx++) {
        owf_channel_t *channel = OWF_ARRAY_PTR(owf->channels, owf_channel_t, channel_idx);
        owf_test_print_channel(channel);
        for (size_t ns_idx = 0; ns_idx < OWF_ARRAY_LEN(channel->namespaces); ns_idx++) {
            owf_namespace_t *ns = OWF_ARRAY_PTR(channel->namespaces, owf_namespace_t, ns_idx);
            owf_test_print_namespace(ns);
            for (size_t sig_idx = 0; sig_idx < OWF_ARRAY_LEN(ns->signals); sig_idx++) {
                owf_signal_t *sig = OWF_ARRAY_PTR(ns->signals, owf_signal_t, sig_idx);
                owf_test_print_signal(sig);
            }

            for (size_t event_idx = 0; event_idx < OWF_ARRAY_LEN(ns->events); event_idx++) {
                owf_event_t *event = OWF_ARRAY_PTR(ns->events, owf_event_t, event_idx);
                owf_test_print_event(event);
            }

            for (size_t alarm_idx = 0; alarm_idx < OWF_ARRAY_LEN(ns->alarms); alarm_idx++) {
                owf_alarm_t *alarm = OWF_ARRAY_PTR(ns->alarms, owf_alarm_t, alarm_idx);
                owf_test_print_alarm(alarm);
            }
        }
    }

    fprintf(stdout, "** result: %s\n", owf_binary_reader_strerror(reader));
    OWF_TEST_OK;
}

static int owf_test_binary_materialize_file_execute(const char *filename, bool result) {
    owf_alloc_t alloc = {.malloc = malloc, .realloc = realloc, .free = free, .max_alloc = OWF_ALLOC_DEFAULT_MAX};
    owf_binary_reader_t reader;
    owf_t *owf;

    if (!owf_test_binary_open(&reader, filename, &alloc, NULL)) {
        OWF_TEST_FAIL("error opening file");
    }
    owf = owf_binary_materialize(&reader);
    if (owf != NULL) {
        owf_test_binary_materialize_print(&reader, owf);
        owf_destroy(owf, &alloc);
    }
    if ((result == true && owf == NULL) || (result == false && owf != NULL)) {
        OWF_TEST_FAILF("unexpected result when reading OWF: %s", owf_binary_reader_strerror(&reader));
    }
    owf_test_binary_file_close(&reader);

    OWF_TEST_OK;
}

static int owf_test_binary_materialize_buffer_execute(const char *filename, bool result) {
    owf_alloc_t alloc = {.malloc = malloc, .realloc = realloc, .free = free, .max_alloc = OWF_ALLOC_DEFAULT_MAX};
    owf_buffer_t buf;
    owf_binary_reader_t reader;
    owf_t *owf;

    if (!owf_test_binary_read_file(filename, &reader, &alloc, &buf, NULL)) {
        OWF_TEST_FAIL("error reading file");
    }
    owf = owf_binary_materialize(&reader);
    if (owf != NULL) {
        owf_test_binary_materialize_print(&reader, owf);
        owf_destroy(owf, &alloc);
    }
    if ((result == true && owf == NULL) || (result == false && owf != NULL)) {
        OWF_TEST_FAILF("unexpected result when reading OWF: %s", owf_binary_reader_strerror(&reader));
    }
    owf_test_binary_buffer_close(&reader);

    OWF_TEST_OK;
}

static int owf_test_binary_visitor_file_valid_1(void) {
    return OWF_TEST_VISITOR_FILE("binary_valid_1", true);
}

static int owf_test_binary_visitor_buffer_valid_1(void) {
    return OWF_TEST_VISITOR_BUFFER("binary_valid_1", true);
}

static int owf_test_binary_visitor_file_valid_2(void) {
    return OWF_TEST_VISITOR_FILE("binary_valid_2", true);
}

static int owf_test_binary_visitor_buffer_valid_2(void) {
    return OWF_TEST_VISITOR_BUFFER("binary_valid_2", true);
}

static int owf_test_binary_visitor_file_valid_3(void) {
    return OWF_TEST_VISITOR_FILE("binary_valid_3", true);
}

static int owf_test_binary_visitor_buffer_valid_3(void) {
    return OWF_TEST_VISITOR_BUFFER("binary_valid_3", true);
}

static int owf_test_binary_visitor_file_valid_empty(void) {
    return OWF_TEST_VISITOR_FILE("binary_valid_empty", true);
}

static int owf_test_binary_visitor_buffer_valid_empty(void) {
    return OWF_TEST_VISITOR_BUFFER("binary_valid_empty", true);
}

static int owf_test_binary_materialize_file_valid_1(void) {
    return OWF_TEST_MATERIALIZE_FILE("binary_valid_1", true);
}

static int owf_test_binary_materialize_buffer_valid_1(void) {
    return OWF_TEST_MATERIALIZE_BUFFER("binary_valid_1", true);
}

static int owf_test_binary_materialize_file_valid_2(void) {
    return OWF_TEST_MATERIALIZE_FILE("binary_valid_2", true);
}

static int owf_test_binary_materialize_buffer_valid_2(void) {
    return OWF_TEST_MATERIALIZE_BUFFER("binary_valid_2", true);
}

static int owf_test_binary_materialize_file_valid_3(void) {
    return OWF_TEST_MATERIALIZE_FILE("binary_valid_3", true);
}

static int owf_test_binary_materialize_buffer_valid_3(void) {
    return OWF_TEST_MATERIALIZE_BUFFER("binary_valid_3", true);
}

static int owf_test_binary_materialize_file_valid_empty(void) {
    return OWF_TEST_MATERIALIZE_FILE("binary_valid_empty", true);
}

static int owf_test_binary_materialize_buffer_valid_empty(void) {
    return OWF_TEST_MATERIALIZE_BUFFER("binary_valid_empty", true);
}

static owf_test_t tests[] = {
    {"binary_visitor_file_valid_1", owf_test_binary_visitor_file_valid_1},
    {"binary_visitor_buffer_valid_1", owf_test_binary_visitor_buffer_valid_1},
    {"binary_visitor_file_valid_2", owf_test_binary_visitor_file_valid_2},
    {"binary_visitor_buffer_valid_2", owf_test_binary_visitor_buffer_valid_2},
    {"binary_visitor_file_valid_3", owf_test_binary_visitor_file_valid_3},
    {"binary_visitor_buffer_valid_3", owf_test_binary_visitor_buffer_valid_3},
    {"binary_visitor_file_valid_empty", owf_test_binary_visitor_file_valid_empty},
    {"binary_visitor_buffer_valid_empty", owf_test_binary_visitor_buffer_valid_empty},
    {"binary_materialize_file_valid_1", owf_test_binary_materialize_file_valid_1},
    {"binary_materialize_buffer_valid_1", owf_test_binary_materialize_buffer_valid_1},
    {"binary_materialize_file_valid_2", owf_test_binary_materialize_file_valid_2},
    {"binary_materialize_buffer_valid_2", owf_test_binary_materialize_buffer_valid_2},
    {"binary_materialize_file_valid_3", owf_test_binary_materialize_file_valid_3},
    {"binary_materialize_buffer_valid_3", owf_test_binary_materialize_buffer_valid_3},
    {"binary_materialize_file_valid_empty", owf_test_binary_materialize_file_valid_empty},
    {"binary_materialize_buffer_valid_empty", owf_test_binary_materialize_buffer_valid_empty}
};

static bool owf_test_opt(const char *opt, int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], opt) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char **argv) {
    char dir[1024];
    size_t success = 0;
    int ret = 0;

    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Set verbosity
    owf_test_verbose = owf_test_opt("--verbose", argc, argv);

    // And here we go...
    getcwd(dir, sizeof(dir));
    fprintf(stdout, "--------------------------------\n");
    fprintf(stdout, "libowf " OWF_LIBRARY_VERSION_STRING " test harness starting\n");
    fprintf(stdout, "--------------------------------\n");
    fprintf(stdout, "(in %s)\n\n", dir);
    fprintf(stdout, ">> running " OWF_PRINT_SIZE " %s\n", OWF_COUNT(tests), "tests");
    for (size_t i = 0; i < OWF_COUNT(tests); i++) {
        owf_test_t *test = &tests[i];
        fprintf(stdout, ">> test " OWF_PRINT_SIZE "/" OWF_PRINT_SIZE " (%s)...", i + 1, OWF_COUNT(tests), test->name);
        int res = test->fn();
        if (res == 0) {
            success++;
            fprintf(stdout, "PASSED\n");
        } else if (res == 1) {
            fprintf(stdout, "FAILED\n");
        }

        if (res != 0 && res != 1) {
            ret = res;
        }

        fprintf(stdout, "--------------------------------\n");
    }

    // Display results
    fprintf(stdout, ">> " OWF_PRINT_SIZE "/" OWF_PRINT_SIZE " %s successful (%.2f%%)\n", success, OWF_COUNT(tests), "tests", (float)success / (float)OWF_COUNT(tests) * 100);

    if (owf_test_opt("--pause", argc, argv)) {
        fprintf(stdout, "(press enter to exit)\n");
        getc(stdin);
    }

    return ret;
}
