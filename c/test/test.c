#include <owf.h>
#include <owf/binary.h>
#include <owf/binary.h>

#include <stdio.h>
#include <stdarg.h>

#define OWF_TEST_SOFT_FAIL(str) {owf_test_fail(str); return 1;}
#define OWF_TEST_SOFT_FAILF(str, ...) {owf_test_fail(str, __VA_ARGS__); return 1;}
#define OWF_TEST_FAIL(str) {owf_test_fail(str); return 2;}
#define OWF_TEST_FAILF(str, ...) {owf_test_fail(str, __VA_ARGS__); return 2;}
#define OWF_TEST_OK return 0

void owf_test_fail(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    putc(' ', stderr);
    vfprintf(stderr, fmt, va);
    printf("\n");
    va_end(va);
}

typedef struct owf_test {
    const char *name;
    int (*fn)(void);
} owf_test_t;



static int test_example_1_binary(void) {
    owf_binary_reader_t reader;
    FILE *f = fopen("test/example/owf1_example_1.owf", "r");
    owf_binary_reader_init_file(&reader, f, malloc, free, NULL);
    if (!owf_binary_read(&reader)) {
        OWF_TEST_FAILF("unexpected error when reading OWF: %s", owf_binary_reader_strerror(&reader));
    }

    owf_binary_reader_destroy_file(&reader);
    fclose(f);
    OWF_TEST_OK;
}

static int test_example_2_binary(void) {
    owf_binary_reader_t reader;
    FILE *f = fopen("test/example/owf1_example_2.owf", "r");
    owf_binary_reader_init_file(&reader, f, malloc, free, NULL);
    if (!owf_binary_read(&reader)) {
        OWF_TEST_FAILF("unexpected error when reading OWF: %s", owf_binary_reader_strerror(&reader));
    }

    owf_binary_reader_destroy_file(&reader);
    fclose(f);
    OWF_TEST_OK;
}

static owf_test_t tests[] = {
    {"example_1_binary", test_example_1_binary},
    {"example_2_binary", test_example_2_binary}
};

int main(int argc, char **argv) {
    int ret = 0, success = 0;

    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // And here we go...
    fprintf(stderr, ">> running %lu %s\n", OWF_COUNT(tests), OWF_COUNT(tests) == 1 ? "test" : "tests");
    for (int i = 0; i < OWF_COUNT(tests); i++) {
        owf_test_t *test = &tests[i];
        fprintf(stderr, ">> test %d/%lu (%s)...", i + 1, OWF_COUNT(tests), test->name);
        int res = test->fn();
        if (res == 0) {
            success++;
            fprintf(stderr, " <OK>\n");
        } else if (res == 1) {
            fprintf(stderr, " <SOFT FAIL>\n");
        }

        if (res != 0 && res != 1) {
            ret = res;
        }
    }

    // Display results
    fprintf(stderr, ">> %d/%lu %s successful (%.2f%%)\n", success, OWF_COUNT(tests), OWF_COUNT(tests) != 1 ? "tests" : "test", (float)success / (float)OWF_COUNT(tests) * 100);
    return ret;
}