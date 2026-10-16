/* Transcripts of a module that is not here, for the five ways this lab goes.
 *
 * Four of them are sessions this program can run. The fifth is a recorded
 * session that breaks the lab's own rule, and it exists for the audit: a
 * transcript brought back from a bench, or typed by hand in minicom, in
 * which the receiver was still powered when the radio transmitted.
 */

#include "p03.h"

#include <string.h>

static void say(FILE *fp, const char *cmd, const char *reply, const char *final)
{
    fprintf(fp, "> %s\n", cmd);
    if (reply != NULL && reply[0] != '\0')
        fprintf(fp, "< %s\n", reply);
    if (final != NULL)
        fprintf(fp, "< %s\n", final);
}

/* One fix, the same every time, so that a cached republish is recognisable
 * by its stamp being the one from the cycle before. */
static void demo_fix(p03_fix *f)
{
    p03_fix_clear(f);
    f->valid = true;
    snprintf(f->stamp, sizeof f->stamp, "20261016103012.000");
    snprintf(f->lat, sizeof f->lat, "47.070714");
    snprintf(f->lon, sizeof f->lon, "15.439504");
}

static void publish(FILE *fp, const p03_config *c, const p03_fix *f)
{
    char payload[128];
    size_t n = p03_payload(f, payload, sizeof payload);
    if (n == 0)
        return;

    char cmd[AT_LINE_MAX];
    snprintf(cmd, sizeof cmd, "AT+SMCONF=\"URL\",\"%s\",%u",
             c->broker, (unsigned)c->port);
    say(fp, cmd, NULL, "OK");
    snprintf(cmd, sizeof cmd, "AT+SMCONF=\"CLIENTID\",\"%s\"", c->client_id);
    say(fp, cmd, NULL, "OK");
    say(fp, "AT+SMCONN", NULL, "OK");
    snprintf(cmd, sizeof cmd, "AT+SMPUB=\"%s\",%lu,0,1",
             c->topic, (unsigned long)n);
    say(fp, cmd, NULL, ">");
    say(fp, payload, NULL, "OK");
}

p03_status p03_scene_write(p03_scene scene, const p03_config *c, const char *path)
{
    if (c == NULL || path == NULL)
        return P03_E_ARG;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P03_E_IO;

    fprintf(fp, "# A module that is not here, answering the commands this\n"
                "# lab sends. Replay it with: p03 session --transcript\n");

    p03_fix f;
    demo_fix(&f);

    if (scene == P03_SCENE_GNSS_LEFT_ON) {
        /* Not a session this program can produce. It is what a hand-typed
         * bench session looks like when the rule was forgotten, and it is
         * here so that the audit has a known positive to find. */
        fprintf(fp, "# Recorded by hand. The receiver was never powered down.\n");
        say(fp, "AT+CGNSPWR=1", NULL, "OK");
        say(fp, "AT+CGNSINF",
            "+CGNSINF: 1,1,20261016103012.000,47.070714,15.439504,353.0,0.0,0.0", "OK");
        publish(fp, c, &f);
        if (fclose(fp) != 0)
            return P03_E_IO;
        return P03_OK;
    }

    say(fp, "AT+CPIN?", "+CPIN: READY", "OK");
    say(fp, "AT+CNMP=38", NULL, "OK");
    say(fp, "AT+CMNB=1", NULL, "OK");

    if (scene == P03_SCENE_WRONG_BEARER) {
        /* Registered and perfectly willing to publish, on the wrong bearer.
         * The bring-up stops here, so nothing follows. */
        say(fp, "AT+CPSI?", "+CPSI: NB-IOT,Online,232-01,0x1A2B,123456,255,"
                            "EUTRAN-BAND20,6300,3,3,-93,-850,-620,10", "OK");
        if (fclose(fp) != 0)
            return P03_E_IO;
        return P03_OK;
    }

    say(fp, "AT+CPSI?", "+CPSI: LTE CAT-M1,Online,232-01,0x1A2B,123456,255,"
                        "EUTRAN-BAND20,6300,3,3,-93,-850,-620,10", "OK");
    say(fp, "AT+CSQ", "+CSQ: 21,99", "OK");

    for (uint32_t i = 0; i < c->cycles; i++) {
        bool fix_now = (scene == P03_SCENE_FIX) ||
                       (scene == P03_SCENE_CACHED && i == 0);

        say(fp, "AT+CGNSPWR=1", NULL, "OK");
        if (fix_now) {
            say(fp, "AT+CGNSINF",
                "+CGNSINF: 1,1,20261016103012.000,47.070714,15.439504,353.0,0.0,0.0",
                "OK");
        } else {
            /* Powered and searching. Four commas and nothing between them
             * is the normal answer from a receiver that has not found
             * anything yet, and it is not an error. */
            say(fp, "AT+CGNSINF", "+CGNSINF: 1,0,,,,,,", "OK");
        }
        say(fp, "AT+CGNSPWR=0", NULL, "OK");

        if (scene == P03_SCENE_NO_FIX)
            continue;   /* nothing is published, and nothing is invented */
        publish(fp, c, &f);
    }

    if (fclose(fp) != 0)
        return P03_E_IO;
    return P03_OK;
}
