/* Minimal test harness: CHECK(cond) counts failures; RUN(fn) prints PASS/FAIL per test id. */
#ifndef ZTEST_H
#define ZTEST_H

#include <stdio.h>

static int zt_fail_count;
static int zt_total_fail;

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                       \
            zt_fail_count++;                                                                 \
        }                                                                                    \
    } while (0)

#define RUN(fn)                                                                              \
    do {                                                                                     \
        zt_fail_count = 0;                                                                   \
        fn();                                                                                \
        printf("%s %s\n", zt_fail_count ? "FAIL" : "PASS", #fn);                             \
        zt_total_fail += zt_fail_count;                                                      \
    } while (0)

#endif
