#include <Arduino.h>
#include "oled/oled_display.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond)                                                          \
    do {                                                                           \
        g_tests_run++;                                                             \
        if (!(cond)) {                                                             \
            g_tests_failed++;                                                      \
            std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                          \
    } while (0)

