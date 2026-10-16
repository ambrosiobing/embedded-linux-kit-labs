/* The suite. No hardware, no serial port, no network, no SIM.
 *
 * Six groups, kept apart because they fail for different reasons:
 *
 *   timers     whether the two bitmaps are read with their own unit tables,
 *              which are not the same table
 *   plan       whether a schedule that never lets the radio sleep is
 *              refused rather than reported
 *   payload    whether a record that outgrew its budget is caught here
 *   parsing    whether the registration state is read from the field that
 *              carries it
 *   engine     whether a command waits for its final line, and whether a
 *              transcript that disagrees with the program is an error
 *   session    whether the indicator follows registration and nothing else,
 *              which is this lab's own rule
 */

#include "../src/p02.h"

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

static const char *TMP_SCENE = "test_p02.transcript";
static const char *TMP_LOG   = "test_p02.log";

static bool write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, fp);
    return fclose(fp) == 0 && w == n;
}

static uint32_t seconds_of(p02_timer_kind kind, const char *bits)
{
    p02_timer t;
    if (p02_timer_parse(&t, kind, bits) != P02_OK)
        return 0xFFFFFFFFu;
    return t.deactivated ? 0u : t.seconds;
}

/* ---------------------------------------------------------------- groups */

static void test_timers(void)
{
    printf("\n-- timers\n");

    /* The chapter's own two bitmaps. Unit 001 of the update timer is an
     * hour and value 3, so three hours; unit 000 of the active timer is two
     * seconds and value 5, so ten. */
    ok(seconds_of(P02_TIMER_TAU, "00100011") == 3u * 3600u,
       "00100011 is a three hour update interval");
    ok(seconds_of(P02_TIMER_ACTIVE, "00000101") == 10u,
       "00000101 is ten seconds of active time");

    /* The same eight characters mean different things in the two tables,
     * which is the whole reason the kind is an argument. */
    ok(seconds_of(P02_TIMER_TAU, "00000101") == 5u * 600u,
       "the same bits read as an update interval are fifty minutes");
    ok(seconds_of(P02_TIMER_TAU, "00000101") != seconds_of(P02_TIMER_ACTIVE, "00000101"),
       "so reading one timer with the other's units gives a different answer");

    ok(seconds_of(P02_TIMER_TAU, "01000001") == 10u * 3600u,
       "unit 010 of the update timer is ten hours");
    ok(seconds_of(P02_TIMER_TAU, "01100010") == 2u * 2u,
       "unit 011 is two seconds, so value 2 is four");
    ok(seconds_of(P02_TIMER_TAU, "11000001") == 320u * 3600u,
       "unit 110 is three hundred and twenty hours");

    p02_timer t;
    ok(p02_timer_parse(&t, P02_TIMER_TAU, "11100001") == P02_OK &&
       t.deactivated && t.seconds == 0,
       "unit 111 is a deactivated update timer, and no seconds at all");
    ok(p02_timer_parse(&t, P02_TIMER_ACTIVE, "11100001") == P02_OK && t.deactivated,
       "unit 111 deactivates the active timer too");

    /* The specification's catch-all for the active timer: a unit it does not
     * define is to be read as one minute, not refused. */
    ok(seconds_of(P02_TIMER_ACTIVE, "10000011") == 3u * 60u,
       "an undefined active unit is read as minutes, as the specification says");

    ok(p02_timer_parse(&t, P02_TIMER_TAU, "0010001") != P02_OK,
       "seven characters is refused");
    ok(p02_timer_parse(&t, P02_TIMER_TAU, "00100012") != P02_OK,
       "a character that is not 0 or 1 is refused");
    ok(p02_timer_parse(&t, P02_TIMER_TAU, "") != P02_OK, "an empty string is refused");
}

static void test_plan(void)
{
    printf("\n-- the plan\n");

    p02_timer tau;
    p02_timer active;
    p02_plan p;

    ok(p02_timer_parse(&tau, P02_TIMER_TAU, "00100011") == P02_OK, "three hours");
    ok(p02_timer_parse(&active, P02_TIMER_ACTIVE, "00000101") == P02_OK, "ten seconds");

    ok(p02_plan_make(&p, &tau, &active, 600) == P02_OK, "a ten minute period plans");
    ok(p.sends_per_hour > 5.99 && p.sends_per_hour < 6.01,
       "which is six datagrams an hour, the chapter's target");
    ok(p.awake_seconds_per_hour > 59.9 && p.awake_seconds_per_hour < 60.1,
       "and about a minute of the hour reachable");
    ok(p.radio_ever_sleeps, "the radio reaches power saving between sends");
    ok(p.tau_outlives_period,
       "and the update interval outlives the send period, so the sends are "
       "not what keeps it registered");

    /* The failure the chapter warns about: sending again before the active
     * window has closed. */
    ok(p02_plan_make(&p, &tau, &active, 5) == P02_OK, "a five second period plans");
    ok(!p.radio_ever_sleeps,
       "and is refused, because the radio never reaches power saving");
    ok(p.sends_per_hour > 719.0, "that schedule is seven hundred sends an hour");

    ok(p02_plan_make(&p, &tau, &active, 0) != P02_OK, "a period of zero is refused");
}

static void test_payload(void)
{
    printf("\n-- the payload\n");

    uint32_t bytes = 0;
    ok(p02_payload_check("50494E47", 20, &bytes) == P02_OK && bytes == 4,
       "eight hexadecimal characters are four bytes on the wire");
    ok(p02_payload_check("50494E4", 20, NULL) == P02_E_FORMAT,
       "an odd number of characters is refused");
    ok(p02_payload_check("50494E4G", 20, NULL) == P02_E_FORMAT,
       "a character that is not hexadecimal is refused");
    ok(p02_payload_check("", 20, NULL) == P02_E_FORMAT, "an empty payload is refused");
    ok(p02_payload_check("aabbccdd", 20, NULL) == P02_OK, "lower case is accepted");

    /* Sixty-four characters, which is thirty-two bytes. An odd length would
     * be refused for being odd and would prove nothing about the budget. */
    char big[65];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    ok(p02_payload_check(big, 20, &bytes) == P02_E_RANGE && bytes == 32,
       "a record that outgrew the budget is refused, and the size is reported");

    char exact[41];
    memset(exact, 'a', sizeof exact - 1);
    exact[sizeof exact - 1] = '\0';
    ok(p02_payload_check(exact, 20, &bytes) == P02_OK && bytes == 20,
       "and twenty bytes exactly is inside it");
}

static void test_parsing(void)
{
    printf("\n-- parsing\n");

    /* The headline. The first field of +CEREG is what the host asked for,
     * not what the network did, and reading it as the state reports a
     * healthy registration whenever notifications are enabled. */
    ok(p02_reg_from_cereg("+CEREG: 2,0") == P02_REG_NOT_REGISTERED,
       "a module with notifications on and no registration reads as not registered");
    ok(p02_reg_from_cereg("+CEREG: 0,1") == P02_REG_HOME, "state 1 is the home network");
    ok(p02_reg_from_cereg("+CEREG: 0,5") == P02_REG_ROAMING, "state 5 is roaming");
    ok(p02_reg_from_cereg("+CEREG: 0,2") == P02_REG_SEARCHING, "state 2 is searching");
    ok(p02_reg_from_cereg("+CEREG: 0,3") == P02_REG_DENIED, "state 3 is denied");
    ok(p02_reg_from_cereg("+CEREG: 0") == P02_REG_UNKNOWN,
       "a line with no state field is unknown rather than assumed");

    char field[32];
    ok(at_field("+CSQ: 19,99", 0, field, sizeof field) && strcmp(field, "19") == 0,
       "the first field of +CSQ is the signal");
    ok(at_field("+CSQ: 19,99", 1, field, sizeof field) && strcmp(field, "99") == 0,
       "and the second is the bit error rate");
    ok(!at_field("+CSQ: 19,99", 2, field, sizeof field),
       "a field that is not there is not invented");
    ok(at_field("+CPSMS: 1,,\"00100011\",\"00000101\"", 2, field, sizeof field) &&
       strcmp(field, "\"00100011\"") == 0,
       "an empty field is still counted, so the bitmap is field two");
}

static void test_engine(void)
{
    printf("\n-- the engine\n");

    ok(write_text(TMP_SCENE,
                  "# a short conversation\n"
                  "> AT\n"
                  "< OK\n"
                  "> AT+CSQ\n"
                  "< +CSQ: 19,99\n"
                  "< OK\n"),
       "a two command transcript is written");

    atchat a;
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    ok(atchat_command(&a, "AT") == AT_OK, "the first command is accepted");
    ok(a.final == AT_FINAL_OK, "and its final line is OK");
    ok(atchat_command(&a, "AT+CSQ") == AT_OK, "the second is accepted");
    ok(atchat_find(&a, "+CSQ:") != NULL, "its reply line is found by prefix");
    ok(atchat_find(&a, "+CEREG:") == NULL, "and a line that is not there is not");
    ok(a.commands == 2, "two commands were sent");
    atchat_close(&a);

    /* A transcript is an assertion about the order of the conversation. */
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens again");
    ok(atchat_command(&a, "AT+CSQ") == AT_E_MISMATCH,
       "asking the second question first is a mismatch, not a silent pass");
    atchat_close(&a);

    ok(write_text(TMP_SCENE, "> AT+CPSMS=1\n< +CME ERROR: 4\n"),
       "a transcript where the module refuses");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    ok(atchat_command(&a, "AT+CPSMS=1") == AT_E_ERROR, "the refusal is reported");
    ok(a.errors == 1 && a.timeouts == 0, "counted as a refusal and not a silence");
    atchat_close(&a);

    ok(write_text(TMP_SCENE, "> AT\n! timeout\n"), "a transcript where it says nothing");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    ok(atchat_command(&a, "AT") == AT_E_TIMEOUT, "the silence is reported");
    ok(a.timeouts == 1 && a.errors == 0, "counted as a silence and not a refusal");
    atchat_close(&a);
}

static void test_session(void)
{
    printf("\n-- the session, and the indicator rule\n");

    p02_config c;
    p02_config_init(&c);
    c.cycles = 3;

    atchat a;
    p02_session s;

    /* Attached: three cycles, three sends, the indicator on throughout. */
    ok(p02_scene_write(P02_SCENE_ATTACHED, &c, TMP_SCENE) == P02_OK,
       "the attached scene is written");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) == P02_OK, "bring-up completes");
    ok(s.psm_requested, "power saving was accepted by the module");
    ok(s.readback_parsed && s.psm_readback_matches,
       "and the readback carries the octets it was given");
    ok(s.tau_requested.seconds == 3u * 3600u, "the request decodes to three hours");
    for (int i = 0; i < 3; i++)
        ok(p02_cycle(&a, &c, &s) == P02_OK, "a cycle completes");
    ok(s.sends_accepted == 3, "three datagrams were accepted");
    ok(s.led_on_cycles == 3, "and the indicator was on for all three cycles");
    atchat_close(&a);

    /* Searching: nothing is ever sent, and nothing is an error. */
    ok(p02_scene_write(P02_SCENE_SEARCHING, &c, TMP_SCENE) == P02_OK,
       "the searching scene is written");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) == P02_OK, "bring-up completes");
    for (int i = 0; i < 3; i++)
        ok(p02_cycle(&a, &c, &s) == P02_OK, "a cycle completes without sending");
    ok(s.sends_attempted == 0, "nothing was sent into a network that had not answered");
    ok(s.led_on_cycles == 0 && s.not_registered == 3, "and the indicator stayed off");
    ok(s.at_errors == 0 && s.at_timeouts == 0, "with no refusal and no silence");
    atchat_close(&a);

    /* Power saving refused: the session carries on and says so. */
    ok(p02_scene_write(P02_SCENE_PSM_REFUSED, &c, TMP_SCENE) == P02_OK,
       "the scene where the module refuses power saving");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) == P02_OK, "bring-up completes anyway");
    ok(!s.psm_requested, "and records that power saving was not accepted");
    ok(s.at_errors == 1, "with the refusal counted");
    atchat_close(&a);

    /* Silent: bring-up stops at the first command. */
    ok(p02_scene_write(P02_SCENE_SILENT, &c, TMP_SCENE) == P02_OK,
       "the scene where the module says nothing");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) != P02_OK, "bring-up stops");
    ok(s.at_timeouts == 1, "on a silence rather than on a refusal");
    atchat_close(&a);

    /* What the logger writes is the transcript format, which is the claim
     * that a session with a real module can be replayed with none. Record
     * one, replay the recording, and the counts must agree. */
    ok(p02_scene_write(P02_SCENE_ATTACHED, &c, TMP_SCENE) == P02_OK,
       "the attached scene again");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    ok(atchat_log_to(&a, TMP_LOG) == AT_OK, "and the session is logged");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) == P02_OK, "bring-up completes");
    for (int i = 0; i < 3; i++)
        p02_cycle(&a, &c, &s);
    uint32_t sends_live = s.sends_accepted;
    atchat_close(&a);

    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_LOG) == AT_OK, "the log opens as a transcript");
    p02_session_init(&s);
    ok(p02_bringup(&a, &c, &s) == P02_OK, "the recorded bring-up replays");
    for (int i = 0; i < 3; i++)
        p02_cycle(&a, &c, &s);
    ok(s.sends_accepted == sends_live && sends_live == 3,
       "and the replay reaches the same three datagrams as the session");
    atchat_close(&a);

    /* The rule, stated as a comparison. The send fails and the indicator,
     * which reports whether the network knows this module, does not care. */
    ok(write_text(TMP_SCENE,
                  "> AT+CEREG?\n< +CEREG: 0,1\n< OK\n"
                  "> AT+CSQ\n< +CSQ: 19,99\n< OK\n"
                  "> AT+CSOC=1,2,1\n< OK\n"
                  "> AT+CSOCON=0,5683,\"172.16.0.1\"\n< OK\n"
                  "> AT+CSOSEND=0,8,\"50494E47\"\n< +CME ERROR: 3\n"),
       "a transcript where the send is refused and the registration is good");
    atchat_init(&a);
    ok(atchat_open_transcript(&a, TMP_SCENE) == AT_OK, "it opens");
    p02_session_init(&s);
    ok(p02_cycle(&a, &c, &s) == P02_OK, "the cycle completes");
    ok(s.sends_attempted == 1 && s.sends_accepted == 0, "the datagram was refused");
    ok(s.led_on_cycles == 1,
       "and the indicator is on, because it follows the registration state "
       "and nothing else");
    atchat_close(&a);
}

static void cleanup(void)
{
    remove(TMP_SCENE);
    remove(TMP_LOG);
}

int main(void)
{
    printf("P02 suite: no hardware, no serial port, no network, no SIM\n");

    test_timers();
    test_plan();
    test_payload();
    test_parsing();
    test_engine();
    test_session();
    cleanup();

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    if (g_failed > 0)
        return 1;
    printf("No module answered any of this. Every reply came from a file\n"
           "this suite wrote a moment earlier.\n");
    return 0;
}
