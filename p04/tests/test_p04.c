/* The suite. No framework, no hardware, no network.
 *
 * Four groups, kept apart because they fail for different reasons:
 *
 *   transport      whether the stream arrived whole
 *   determinism    whether the same input gives the same output
 *   physics        whether the signal obeys the one absolute check available
 *   orthogonality  whether vibration and sound move independently, which is
 *                  the lab's own claim and the only one worth publishing
 *
 * Each check prints what it compared, so a failure names the numbers rather
 * than only the line.
 */

#include "../src/p04.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0;
static int g_failed = 0;

static void ok(bool cond, const char *what)
{
    g_checks++;
    if (cond) {
        printf("ok   %s\n", what);
    } else {
        g_failed++;
        printf("FAIL %s\n", what);
    }
}

static void ok_near(double got, double want, double tol, const char *what)
{
    g_checks++;
    if (fabs(got - want) <= tol) {
        printf("ok   %s (%.6g)\n", what, got);
    } else {
        g_failed++;
        printf("FAIL %s: got %.6g, wanted %.6g within %.6g\n", what, got, want, tol);
    }
}

/* ---------------------------------------------------------------- helpers */

static const char *TMP_IDLE   = "test_idle.p04";
static const char *TMP_TOUCH  = "test_touch.p04";
static const char *TMP_SPEECH = "test_speech.p04";
static const char *TMP_BOOK   = "test_book.lab";

#define TEST_LEN 2048
#define TEST_RATE 26667.0f
#define TEST_G_PER_COUNT (1.0f / 4000.0f)

static bool write_capture(const char *path, p04_synth_kind kind, uint32_t blocks,
                          const uint32_t *drop, size_t n_drop, float rate, uint32_t seed)
{
    p04_capture cap;
    if (p04_capture_create(&cap, path, TEST_LEN, rate, TEST_G_PER_COUNT) != P04_OK)
        return false;
    static p04_block b;
    uint32_t rng = seed;
    for (uint32_t i = 0; i < blocks; i++) {
        p04_synth_block(&b, kind, i, TEST_LEN, rate, &rng);
        bool skip = false;
        for (size_t k = 0; k < n_drop; k++)
            if (drop[k] == i)
                skip = true;
        if (skip)
            continue;
        if (p04_capture_write(&cap, &b) != P04_OK) {
            p04_capture_close(&cap);
            return false;
        }
    }
    return p04_capture_close(&cap) == P04_OK;
}

static bool band_energy_of(const char *path, const p04_thresholds *t, float *mean_out,
                           float *mic_out)
{
    p04_capture cap;
    if (p04_capture_open(&cap, path) != P04_OK)
        return false;
    static p04_result r;
    p04_status s = p04_run(&cap, t, &r);
    p04_capture_close(&cap);
    if (s != P04_OK || r.count == 0)
        return false;
    double sum = 0.0;
    for (uint32_t i = 0; i < r.count; i++)
        sum += (double)r.readings[i].vib_energy;
    *mean_out = (float)(sum / (double)r.count);
    *mic_out = p04_result_mean_mic_rms(&r);
    return true;
}

/* ---------------------------------------------------------------- groups */

static void test_transport(void)
{
    printf("\n-- transport\n");

    p04_transport t;
    p04_transport_init(&t);
    for (uint32_t i = 0; i < 5; i++)
        p04_transport_see(&t, i);
    ok(p04_transport_whole(&t), "five blocks in order are a whole stream");
    ok(t.gaps == 0 && t.duplicates == 0, "no gaps and no duplicates counted");

    p04_transport_init(&t);
    p04_transport_see(&t, 0);
    p04_transport_see(&t, 1);
    p04_transport_see(&t, 4);
    ok(!p04_transport_whole(&t), "a jump from 1 to 4 is not a whole stream");
    ok(t.gaps == 2, "two sequence numbers are reported missing");

    p04_transport_init(&t);
    p04_transport_see(&t, 7);
    p04_transport_see(&t, 7);
    ok(t.duplicates == 1, "a repeated sequence number is a duplicate");

    p04_transport_init(&t);
    p04_transport_see(&t, 9);
    p04_transport_see(&t, 8);
    ok(t.reorders == 1 && t.gaps == 0,
       "an earlier sequence number is a reorder and not a gap");

    /* A capture with two deliberate holes must report exactly those holes,
     * which is the check the workflow runs on the command line. */
    const uint32_t drop[2] = {3, 4};
    ok(write_capture(TMP_IDLE, P04_SYNTH_IDLE, 8, drop, 2, TEST_RATE, 1u),
       "a capture with two blocks withheld is written");

    p04_capture cap;
    ok(p04_capture_open(&cap, TMP_IDLE) == P04_OK, "that capture opens");
    p04_transport_init(&t);
    static p04_block b;
    while (p04_capture_read(&cap, &b) == P04_OK)
        p04_transport_see(&t, b.seq);
    p04_capture_close(&cap);
    ok(t.received == 6, "six blocks are present out of eight");
    ok(t.gaps == 2, "the two withheld sequence numbers are reported");
}

static void test_capture_format(void)
{
    printf("\n-- capture format\n");

    ok(write_capture(TMP_IDLE, P04_SYNTH_IDLE, 4, NULL, 0, TEST_RATE, 7u),
       "a capture is written");

    p04_capture cap;
    ok(p04_capture_open(&cap, TMP_IDLE) == P04_OK, "it opens");
    ok(cap.block_len == TEST_LEN, "the block length survives the round trip");
    ok_near((double)cap.rate_hz, (double)TEST_RATE, 0.5, "the rate survives");
    ok(cap.block_count == 4, "the header carries the true block count");
    p04_capture_close(&cap);

    /* A file that is not a capture is refused at the header rather than part
     * way through an analysis. */
    FILE *fp = fopen("test_notacapture.bin", "wb");
    if (fp != NULL) {
        fputs("this is not a capture", fp);
        fclose(fp);
    }
    ok(p04_capture_open(&cap, "test_notacapture.bin") == P04_E_FORMAT,
       "a file with the wrong magic is refused");
    remove("test_notacapture.bin");
}

static void test_determinism(void)
{
    printf("\n-- determinism\n");

    ok(write_capture(TMP_IDLE, P04_SYNTH_IDLE, 4, NULL, 0, TEST_RATE, 42u),
       "a capture is written from seed 42");
    ok(write_capture(TMP_TOUCH, P04_SYNTH_IDLE, 4, NULL, 0, TEST_RATE, 42u),
       "a second capture is written from the same seed");

    p04_thresholds t;
    p04_thresholds_default(&t);
    t.rate_hz = TEST_RATE;

    float a_energy = 0.0f, a_mic = 0.0f, b_energy = 0.0f, b_mic = 0.0f;
    ok(band_energy_of(TMP_IDLE, &t, &a_energy, &a_mic), "the first analyses");
    ok(band_energy_of(TMP_TOUCH, &t, &b_energy, &b_mic), "the second analyses");
    ok(a_energy == b_energy, "the same seed gives bit-identical band energy");
    ok(a_mic == b_mic, "and bit-identical microphone level");

    /* The transform itself: a cosine at a bin centre puts its energy in that
     * bin and almost nothing anywhere else. */
    static float re[64], im[64];
    for (int i = 0; i < 64; i++) {
        re[i] = (float)cos(2.0 * 3.14159265358979323846 * 8.0 * (double)i / 64.0);
        im[i] = 0.0f;
    }
    ok(p04_fft(re, im, 64) == P04_OK, "a 64 point transform runs");
    double mag8 = sqrt((double)re[8] * (double)re[8] + (double)im[8] * (double)im[8]);
    double mag9 = sqrt((double)re[9] * (double)re[9] + (double)im[9] * (double)im[9]);
    ok(mag8 > 20.0 * mag9, "a tone at a bin centre stays in that bin");

    ok(p04_fft(re, im, 63) == P04_E_RANGE, "a length that is not a power of two is refused");
}

static void test_physics(void)
{
    printf("\n-- physics\n");

    ok(write_capture(TMP_IDLE, P04_SYNTH_IDLE, 4, NULL, 0, TEST_RATE, 3u),
       "an idle capture is written");

    p04_capture cap;
    ok(p04_capture_open(&cap, TMP_IDLE) == P04_OK, "it opens");
    static p04_block b;
    double sum = 0.0;
    uint32_t n = 0;
    while (p04_capture_read(&cap, &b) == P04_OK) {
        sum += (double)p04_dc_level_g(b.vib, b.len, cap.g_per_count);
        n++;
    }
    p04_capture_close(&cap);
    ok(n == 4, "four blocks are read");
    ok_near(sum / (double)n, 1.0, 0.05,
            "a probe at rest reads one gravity on the vibration channel");

    /* A band that sits entirely above Nyquist is refused rather than summed to
     * zero, because zero reads as silence and silence is the wrong answer. */
    float e = 0.0f;
    static p04_window win;
    p04_window_hann(&win, TEST_LEN);
    static int16_t flat[TEST_LEN];
    memset(flat, 0, sizeof flat);
    ok(p04_band_energy(flat, TEST_LEN, &win, TEST_RATE, 20000.0f, 25000.0f, &e) == P04_E_RANGE,
       "a band above Nyquist is refused, not reported as silence");

    uint16_t first = 0, last = 0;
    ok(p04_band_bins(1025, TEST_RATE, 2000.0f, 6000.0f, &first, &last) == P04_OK,
       "the default band maps to bins");
    ok(first < last && last < 1025, "and those bins are inside the spectrum");
}

static void test_orthogonality(void)
{
    printf("\n-- orthogonality, which is the lab's own claim\n");

    ok(write_capture(TMP_IDLE,   P04_SYNTH_IDLE,   6, NULL, 0, TEST_RATE, 11u),
       "an idle capture is written");
    ok(write_capture(TMP_TOUCH,  P04_SYNTH_TOUCH,  6, NULL, 0, TEST_RATE, 11u),
       "a touch capture is written");
    ok(write_capture(TMP_SPEECH, P04_SYNTH_SPEECH, 6, NULL, 0, TEST_RATE, 11u),
       "a speech capture is written");

    p04_thresholds t;
    p04_thresholds_default(&t);
    t.rate_hz = TEST_RATE;

    float idle_e = 0, idle_m = 0, touch_e = 0, touch_m = 0, speech_e = 0, speech_m = 0;
    ok(band_energy_of(TMP_IDLE, &t, &idle_e, &idle_m), "idle analyses");
    ok(band_energy_of(TMP_TOUCH, &t, &touch_e, &touch_m), "touch analyses");
    ok(band_energy_of(TMP_SPEECH, &t, &speech_e, &speech_m), "speech analyses");

    printf("     idle   band %.6g  mic %.2f\n", (double)idle_e, (double)idle_m);
    printf("     touch  band %.6g  mic %.2f\n", (double)touch_e, (double)touch_m);
    printf("     speech band %.6g  mic %.2f\n", (double)speech_e, (double)speech_m);

    ok(touch_e > 5.0f * idle_e,
       "a case touch raises the vibration band well above idle");
    ok(touch_m < 2.0f * idle_m,
       "and leaves the microphone where it was");
    ok(speech_m > 5.0f * idle_m,
       "a voice raises the microphone well above idle");
    ok(speech_e < 2.0f * idle_e,
       "and leaves the vibration band where it was");

    /* That separation is the lab. Stated as the comparison a reader can
     * repeat, rather than as a sentence they have to believe. */
    ok(touch_e / idle_e > speech_e / idle_e,
       "the touch moves the band more than the voice does");
    ok(speech_m / idle_m > touch_m / idle_m,
       "and the voice moves the microphone more than the touch does");
}

static void test_labbook_and_classify(void)
{
    printf("\n-- the lab book and classification\n");

    p04_thresholds t;
    p04_thresholds_default(&t);
    t.baseline = 10.0f;
    t.warn_ratio = 1.5f;
    t.fault_ratio = 3.0f;
    t.rate_hz = TEST_RATE;
    t.g_per_count = TEST_G_PER_COUNT;
    snprintf(t.mounting, sizeof t.mounting, "%s", "synthetic, no probe");

    ok(p04_labbook_write(TMP_BOOK, &t) == P04_OK, "a lab book is written");

    p04_thresholds back;
    ok(p04_labbook_read(TMP_BOOK, &back) == P04_OK, "it reads back");
    ok_near((double)back.baseline, 10.0, 1e-6, "the baseline survives");
    ok_near((double)back.warn_ratio, 1.5, 1e-6, "the warn ratio survives");
    ok(strcmp(back.mounting, "synthetic, no probe") == 0, "the mounting survives");

    float ratio = 0.0f;
    ok(p04_classify(9.0f, &t, &ratio) == P04_NORMAL, "below the warn ratio is normal");
    ok(p04_classify(20.0f, &t, &ratio) == P04_WARNING, "above the warn ratio warns");
    ok(p04_classify(40.0f, &t, &ratio) == P04_FAULT, "above the fault ratio faults");
    ok_near((double)ratio, 4.0, 1e-6, "and the ratio is reported");

    /* A book with no baseline cannot classify, and must not turn every block
     * into a fault, which would read as a broken machine rather than a step
     * nobody took. */
    p04_thresholds empty;
    p04_thresholds_default(&empty);
    ok(p04_classify(1000.0f, &empty, &ratio) == P04_NORMAL,
       "with no baseline nothing is called a fault");

    /* The frequency axis check, as a unit rather than through the command. */
    p04_timing timing;
    p04_timing_check(&timing, 26667.0f, 26700.0f, 0.01f);
    ok(timing.trustworthy, "a rate within one per cent is trusted");
    p04_timing_check(&timing, 26667.0f, 30000.0f, 0.01f);
    ok(!timing.trustworthy, "a rate twelve per cent out is not");
}

static void cleanup(void)
{
    remove(TMP_IDLE);
    remove(TMP_TOUCH);
    remove(TMP_SPEECH);
    remove(TMP_BOOK);
}

int main(void)
{
    printf("P04 suite: no hardware, no serial port, no network\n");

    test_capture_format();
    test_transport();
    test_determinism();
    test_physics();
    test_orthogonality();
    test_labbook_and_classify();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No number here is a measurement of anything physical.\n");
    return 0;
}
