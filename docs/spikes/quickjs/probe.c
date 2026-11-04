#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NBYTES 64
#define NCALLS 10000000
#define TRIALS 3
#define STARTUP_TRIALS 5

static int free_calls = 0;

static void *host_realloc(JSRuntime *rt, void *opaque, void *ptr, size_t size) {
    (void)rt;
    (void)opaque;
    if (size == 0) {
        free(ptr);
        free_calls++;
        return NULL;
    }
    return realloc(ptr, size);
}

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void die_if_exception(JSContext *ctx, JSValue v, const char *what) {
    if (JS_IsException(v)) {
        JSValue e = JS_GetException(ctx);
        const char *s = JS_ToCString(ctx, e);
        fprintf(stderr, "%s: exception: %s\n", what, s ? s : "?");
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, e);
        exit(1);
    }
}

static int run_zerocopy(void) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    uint8_t *host = malloc(NBYTES);
    memset(host, 0, NBYTES);
    printf("host_ptr=%p\n", (void *)host);

    JSValue ab = JS_NewArrayBuffer(ctx, host, NBYTES, 0, host_realloc, NULL, false);
    JSValue u8 = JS_NewUint8Array(ctx, host, NBYTES, NULL, NULL, false);

    size_t ab_len = 0, u8_len = 0;
    uint8_t *ab_ptr = JS_GetArrayBuffer(ctx, &ab_len, ab);
    uint8_t *u8_ptr = JS_GetUint8Array(ctx, &u8_len, u8);
    printf("arraybuffer_data_ptr=%p same_as_host=%d len=%zu\n",
           (void *)ab_ptr, ab_ptr == host, ab_len);
    printf("uint8array_data_ptr=%p same_as_host=%d len=%zu\n",
           (void *)u8_ptr, u8_ptr == host, u8_len);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, ab));
    JS_SetPropertyStr(ctx, global, "u8", JS_DupValue(ctx, u8));
    JS_FreeValue(ctx, global);

    const char *js_write =
        "for (let i = 0; i < 64; i++) u8[i] = (i * 7 + 3) & 255;"
        "1";
    JSValue r = JS_Eval(ctx, js_write, strlen(js_write), "<write>", JS_EVAL_TYPE_GLOBAL);
    die_if_exception(ctx, r, "js_write");
    JS_FreeValue(ctx, r);

    int mismatches = 0;
    for (int i = 0; i < NBYTES; i++) {
        if (host[i] != (uint8_t)((i * 7 + 3) & 255)) mismatches++;
    }
    printf("host_read_after_js_write: host[0..7]=");
    for (int i = 0; i < 8; i++) printf("%u ", host[i]);
    printf("| mismatches_over_64=%d\n", mismatches);

    host[63] = 0xAB;
    const char *js_read = "new Uint8Array(ab)[63]";
    r = JS_Eval(ctx, js_read, strlen(js_read), "<read>", JS_EVAL_TYPE_GLOBAL);
    die_if_exception(ctx, r, "js_read");
    int32_t seen = -1;
    JS_ToInt32(ctx, &seen, r);
    JS_FreeValue(ctx, r);
    printf("js_read_after_host_write: ab[63]=%d (host wrote 171) same_memory=%d\n",
           seen, seen == 0xAB);

    printf("free_calls_before_release=%d\n", free_calls);
    JS_FreeValue(ctx, u8);
    JS_FreeValue(ctx, ab);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    printf("free_calls_after_runtime_free=%d\n", free_calls);

    int ok = (ab_ptr == host) && (u8_ptr == host) && mismatches == 0 &&
             seen == 0xAB && free_calls == 1;
    printf("zerocopy_result=%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static JSValue native_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    int32_t a = 0, b = 0;
    JS_ToInt32(ctx, &a, argv[0]);
    JS_ToInt32(ctx, &b, argv[1]);
    return JS_NewInt32(ctx, (int32_t)((uint32_t)a + (uint32_t)b));
}

static double best_of(JSContext *ctx, const char *expr, double *out_result) {
    double best = 1e30;
    for (int t = 0; t < TRIALS; t++) {
        double t0 = now_ns();
        JSValue r = JS_Eval(ctx, expr, strlen(expr), "<bench>", JS_EVAL_TYPE_GLOBAL);
        double t1 = now_ns();
        die_if_exception(ctx, r, expr);
        int32_t v = 0;
        JS_ToInt32(ctx, &v, r);
        JS_FreeValue(ctx, r);
        if (out_result) *out_result = v;
        double d = t1 - t0;
        if (d < best) best = d;
    }
    return best;
}

static int run_bench(void) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "nadd",
                      JS_NewCFunction(ctx, native_add, "nadd", 2));
    JS_FreeValue(ctx, global);

    const char *defs =
        "function bench_js(n) { let s = 0; for (let i = 0; i < n; i++) { s = (s + i) | 0; } return s; }"
        "function bench_native(n) { let s = 0; for (let i = 0; i < n; i++) { s = nadd(s, i); } return s; }"
        "1";
    JSValue r = JS_Eval(ctx, defs, strlen(defs), "<defs>", JS_EVAL_TYPE_GLOBAL);
    die_if_exception(ctx, r, "defs");
    JS_FreeValue(ctx, r);

    double res_js = 0, res_native = 0;
    char expr_js[64], expr_native[64];
    snprintf(expr_js, sizeof expr_js, "bench_js(%d)", NCALLS);
    snprintf(expr_native, sizeof expr_native, "bench_native(%d)", NCALLS);

    double js_ns = best_of(ctx, expr_js, &res_js);
    double native_ns = best_of(ctx, expr_native, &res_native);

    printf("js_loop_best_total_ms=%.2f js_ns_per_iter=%.2f checksum=%.0f\n",
           js_ns / 1e6, js_ns / NCALLS, res_js);
    printf("native_call_loop_best_total_ms=%.2f native_ns_per_call=%.2f checksum=%.0f\n",
           native_ns / 1e6, native_ns / NCALLS, res_native);
    printf("native_minus_js_ns_per_call=%.2f checksums_equal=%d\n",
           (native_ns - js_ns) / NCALLS, res_js == res_native);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

static int run_startup(void) {
    double best = 1e30;
    printf("startup_trials_us=");
    for (int t = 0; t < STARTUP_TRIALS; t++) {
        double t0 = now_ns();
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue v = JS_Eval(ctx, "1", 1, "<startup>", JS_EVAL_TYPE_GLOBAL);
        double t1 = now_ns();
        die_if_exception(ctx, v, "startup");
        JS_FreeValue(ctx, v);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        double d = (t1 - t0) / 1000.0;
        printf("%.1f ", d);
        if (d < best) best = d;
    }
    printf("\nstartup_best_us=%.1f\n", best);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "all";
    int rc = 0;
    if (!strcmp(mode, "zerocopy") || !strcmp(mode, "all")) rc |= run_zerocopy();
    if (!strcmp(mode, "bench") || !strcmp(mode, "all")) rc |= run_bench();
    if (!strcmp(mode, "startup") || !strcmp(mode, "all")) rc |= run_startup();
    return rc;
}
