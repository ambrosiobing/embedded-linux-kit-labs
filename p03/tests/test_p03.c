/* The suite. No hardware, no serial port, no SIM, no sky.
 *
 * Five groups, kept apart because they fail for different reasons:
 *
 *   fix       whether a line with no fix in it yields no fix, rather than
 *             one made of empty strings
 *   bearer    whether Cat-M is read rather than assumed
 *   ordering  whether a transmit with the receiver powered is refused by
 *             the function that would have sent it, which is this lab's
 *             own claim
 *   audit     whether the same question can be answered about a recording
 *             this program did not make
 *   session   whether a cycle with nothing to say says nothing
 */

#include "../src/p03.h"

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

static const char *TMP_SCENE = "test_p03.transcript";
static const char *TMP_LOG   = "test_p03.log";

static const char *FIX_LINE =
    "+CGNSINF: 1,1,20261016103012.000,47.070714,15.439504,353.0,0.0,0.0";

static bool write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, fp);
    return fclose(fp) == 0 && w == n;
}

/* ---------------------------------------------------------------- groups */

static void test_fix(void)
{
    printf("\n-- the fix\n");

    p03_fix f;
    ok(p03_fix_parse(FIX_LINE, &f), "a line with a fix in it parses");
    ok(strcmp(f.stamp, "20261016103012.000") == 0, "the stamp is the receiver's");
    ok(strcmp(f.lat, "47.070714") == 0 && strcmp(f.lon, "15.439504") == 0,
       "and both coordinates arrive");

    ok(!p03_fix_parse("+CGNSINF: 1,0,,,,,,", &f),
       "a powered receiver that has found nothing yields no fix");
    ok(!p03_fix_parse("+CGNSINF: 0,0,,,,,,", &f),
       "and neither does one that is not running");
    ok(!p03_fix_parse("+CGNSINF: 1,1,,,,", &f),
       "a fix flag with no stamp behind it is refused, not believed");
    ok(!p03_fix_parse("+CGNSINF: 1,1,20261016103012.000,,", &f),
       "a stamp with no coordinates is refused as well");
    ok(!p03_fix_parse(NULL, &f), "and a line that is not there is not a fix");

    char payload[128];
    ok(p03_fix_parse(FIX_LINE, &f), "the good line parses again");
    size_t n = p03_payload(&f, payload, sizeof payload);
    ok(n > 0 && strcmp(payload,
                       "20261016103012.000,47.070714,15.439504") == 0,
       "the payload is the receiver's stamp and the two coordinates");

    p03_fix empty;
    p03_fix_clear(&empty);
    ok(p03_payload(&empty, payload, sizeof payload) == 0,
       "and an empty fix produces no payload at all, rather than commas");
}

static void test_bearer(void)
{
    printf("\n-- the bearer\n");

    ok(p03_bearer_is_catm("+CPSI: LTE CAT-M1,Online,232-01"),
       "LTE CAT-M1 is the bearer this lab wants");
    ok(p03_bearer_is_catm("+CPSI: eMTC,Online,232-01"),
       "and so is the same thing under its 3GPP name");
    ok(!p03_bearer_is_catm("+CPSI: NB-IOT,Online,232-01"),
       "NB-IoT is not, however willing it is to publish");
    ok(!p03_bearer_is_catm("+CPSI: GSM,Online,232-01"),
       "and neither is a GPRS fallback");
    ok(!p03_bearer_is_catm(NULL), "a module that said nothing is not on Cat-M");
}

static void test_ordering(void)
{
    printf("\n-- the ordering, which is this lab's claim\n");

    p03_config c;
    p03_config_init(&c);
    c.cycles = 2;

    ok(p03_scene_write(P03_SCENE_FIX, &c, TMP_SCENE) == P03_OK,
       "a scene with a fix on every cycle");

    atchat a;
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");

    p03_session s;
    p03_session_init(&s);
    ok(p03_bringup(&a, &c, &s) == P03_OK, "bring-up completes");
    ok(s.bearer_catm, "and the bearer was read as Cat-M");

    ok(p03_cycle(&a, &c, &s) == P03_OK, "the first cycle completes");
    ok(!s.gnss_powered, "with the receiver powered down at the end of it");
    ok(s.published == 1, "and one payload published");
    ok(s.order_refusals == 0, "nothing was refused for ordering");
    ok(p03_cycle(&a, &c, &s) == P03_OK, "the second cycle completes");
    ok(s.published == 2 && s.fixes_new == 2, "two fixes, two publishes");
    atchat_close(&a);

    /* The guard itself, called directly. As the cycle is written it cannot
     * be reached, because every path to a transmit has powered the receiver
     * down first, and a guard nothing can reach is a guard nothing tests.
     *
     * The check that matters is the second one. A refusal that still sent
     * the command would be a log line pretending to be a rule, and the way
     * to see the difference is that the transcript has not moved: the very
     * next command still matches its first recorded line. */
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "the scene opens again");
    p03_session_init(&s);
    s.gnss_powered = true;
    ok(p03_transmit(&a, &s, "AT+SMCONN") == P03_E_ORDER,
       "a transmit with the receiver powered is refused");
    ok(s.order_refusals == 1, "and counted");
    ok(a.commands == 0, "and nothing at all was sent to the module");
    s.gnss_powered = false;
    ok(p03_transmit(&a, &s, "AT+CPIN?") == P03_OK,
       "with the receiver down the same door opens, on the transcript's first "
       "line, which is how we know the refusal consumed nothing");
    atchat_close(&a);
}

static void test_audit(void)
{
    printf("\n-- the audit\n");

    p03_config c;
    p03_config_init(&c);
    c.cycles = 2;

    ok(p03_scene_write(P03_SCENE_FIX, &c, TMP_SCENE) == P03_OK,
       "a well ordered recording");
    p03_audit a;
    ok(p03_audit_file(TMP_SCENE, &a) == P03_OK, "it is audited");
    ok(a.gnss_on == 2 && a.gnss_off == 2, "the receiver went up and down twice");
    ok(a.transmits == 8, "eight transmit commands across the two cycles");
    ok(p03_audit_clean(&a), "and none of them while the receiver was powered");

    /* The known positive. A sweep that has never found anything has not
     * been shown to be able to. */
    ok(p03_scene_write(P03_SCENE_GNSS_LEFT_ON, &c, TMP_SCENE) == P03_OK,
       "a recording where the receiver was never powered down");
    ok(p03_audit_file(TMP_SCENE, &a) == P03_OK, "it is audited");
    ok(!p03_audit_clean(&a), "and the audit refuses it");
    ok(a.transmits_with_gnss_on == 4,
       "all four transmit commands were sent with the receiver on");
    ok(a.first_bad_line > 0 && strncmp(a.first_bad_cmd, "AT+SMCONF", 9) == 0,
       "and the first one is named, with its line");

    ok(write_text(TMP_SCENE, "# nothing in here at all\n"), "an empty recording");
    ok(p03_audit_file(TMP_SCENE, &a) == P03_OK, "audits");
    ok(a.transmits == 0 && p03_audit_clean(&a),
       "and passes, because a recording with no transmit in it broke no rule");
}

static void test_session(void)
{
    printf("\n-- the session\n");

    p03_config c;
    p03_config_init(&c);
    c.cycles = 3;

    atchat a;
    p03_session s;

    /* A fix on the first cycle only. The other two republish it, with the
     * stamp it came with, which is what makes a repeat visible. */
    ok(p03_scene_write(P03_SCENE_CACHED, &c, TMP_SCENE) == P03_OK,
       "a scene with one fix and two cycles without");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p03_session_init(&s);
    ok(p03_bringup(&a, &c, &s) == P03_OK, "bring-up completes");
    for (int i = 0; i < 3; i++)
        ok(p03_cycle(&a, &c, &s) == P03_OK, "a cycle completes");
    ok(s.fixes_new == 1 && s.fixes_cached == 2, "one new fix and two repeats");
    ok(s.published == 3, "all three cycles published something");
    ok(s.skipped == 0, "and none was skipped");
    ok(strcmp(s.last_payload, "20261016103012.000,47.070714,15.439504") == 0,
       "the last payload carries the stamp of the fix it came from");
    ok(s.last_fix.cached, "and is marked as a repeat");
    atchat_close(&a);

    /* Never a fix. Nothing is published and nothing is invented. */
    ok(p03_scene_write(P03_SCENE_NO_FIX, &c, TMP_SCENE) == P03_OK,
       "a scene where the receiver never finds anything");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p03_session_init(&s);
    ok(p03_bringup(&a, &c, &s) == P03_OK, "bring-up completes");
    for (int i = 0; i < 3; i++)
        ok(p03_cycle(&a, &c, &s) == P03_OK, "a cycle completes");
    ok(s.published == 0 && s.skipped == 3,
       "three cycles skipped rather than three payloads with a host clock in them");
    ok(s.last_payload[0] == '\0', "and nothing was ever published");
    atchat_close(&a);

    /* The wrong bearer stops the lab before it publishes anything. */
    ok(p03_scene_write(P03_SCENE_WRONG_BEARER, &c, TMP_SCENE) == P03_OK,
       "a scene on NB-IoT");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p03_session_init(&s);
    ok(p03_bringup(&a, &c, &s) == P03_E_BEARER, "bring-up refuses the bearer");
    ok(!s.bearer_catm, "and says which bearer it found");
    ok(s.published == 0, "nothing was published over it");
    atchat_close(&a);

    /* What the logger writes is auditable, which closes the loop: a session
     * run against a module leaves a recording the audit can read. */
    ok(p03_scene_write(P03_SCENE_FIX, &c, TMP_SCENE) == P03_OK, "a good scene");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    ok(atchat_log_to(&a, TMP_LOG) == AT_OK, "and the session is logged");
    p03_session_init(&s);
    ok(p03_bringup(&a, &c, &s) == P03_OK, "bring-up completes");
    for (int i = 0; i < 3; i++)
        p03_cycle(&a, &c, &s);
    atchat_close(&a);

    p03_audit audit;
    ok(p03_audit_file(TMP_LOG, &audit) == P03_OK, "the log audits");
    ok(p03_audit_clean(&audit),
       "and a session this program ran cannot fail its own ordering audit");
    ok(audit.gnss_on == 3 && audit.gnss_off == 3,
       "with the receiver powered and powered down once per cycle");
}

static void cleanup(void)
{
    remove(TMP_SCENE);
    remove(TMP_LOG);
}

int main(void)
{
    printf("P03 suite: no hardware, no serial port, no SIM, no sky\n");

    test_fix();
    test_bearer();
    test_ordering();
    test_audit();
    test_session();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No module answered any of this, and no satellite was involved.\n");
    return 0;
}
