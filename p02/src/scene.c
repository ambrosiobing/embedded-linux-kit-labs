/* Transcripts of modules that do not exist, for the four ways this lab goes.
 *
 * A transcript is not a stub. The engine checks every command the program
 * sends against the one recorded here, so a scene is a statement about the
 * order of the conversation as much as about its content. When the session
 * code changes which command comes first, these stop matching, and that is
 * the point.
 */

#include "p02.h"

#include <string.h>

static void say(FILE *fp, const char *cmd, const char *reply, const char *final)
{
    fprintf(fp, "> %s\n", cmd);
    if (reply != NULL && reply[0] != '\0')
        fprintf(fp, "< %s\n", reply);
    if (final != NULL)
        fprintf(fp, "< %s\n", final);
}

static void bringup(FILE *fp, const p02_config *c, p02_scene scene)
{
    char cmd[AT_LINE_MAX];

    say(fp, "AT", NULL, "OK");
    say(fp, "AT+CPIN?", "+CPIN: READY", "OK");
    snprintf(cmd, sizeof cmd, "AT+CBAND=%u", (unsigned)c->band);
    say(fp, cmd, NULL, "OK");
    say(fp, "AT+COPS=0", NULL, "OK");

    snprintf(cmd, sizeof cmd, "AT+CPSMS=1,,\"%s\",\"%s\"",
             c->tau_bits, c->active_bits);
    if (scene == P02_SCENE_PSM_REFUSED) {
        /* A module that will not take the request at all. The chapter's
         * fallback exists for exactly this, and the session is meant to
         * carry on and say so rather than stop. */
        say(fp, cmd, NULL, "+CME ERROR: 4");
        say(fp, "AT+CPSMS?", "+CPSMS: 0", "OK");
        return;
    }
    say(fp, cmd, NULL, "OK");

    char readback[AT_LINE_MAX];
    snprintf(readback, sizeof readback, "+CPSMS: 1,,\"%s\",\"%s\"",
             c->tau_bits, c->active_bits);
    say(fp, "AT+CPSMS?", readback, "OK");
}

static void cycle(FILE *fp, const p02_config *c, p02_scene scene)
{
    char cmd[AT_LINE_MAX];

    if (scene == P02_SCENE_SEARCHING) {
        say(fp, "AT+CEREG?", "+CEREG: 0,2", "OK");
        say(fp, "AT+CSQ", "+CSQ: 99,99", "OK");
        return;   /* nothing is sent into a network that has not answered */
    }

    say(fp, "AT+CEREG?", "+CEREG: 0,1", "OK");
    say(fp, "AT+CSQ", "+CSQ: 19,99", "OK");
    say(fp, "AT+CSOC=1,2,1", NULL, "OK");
    snprintf(cmd, sizeof cmd, "AT+CSOCON=0,%u,\"%s\"",
             (unsigned)c->port, c->endpoint);
    say(fp, cmd, NULL, "OK");
    snprintf(cmd, sizeof cmd, "AT+CSOSEND=0,%lu,\"%s\"",
             (unsigned long)strlen(c->payload_hex), c->payload_hex);
    say(fp, cmd, NULL, "OK");
}

p02_status p02_scene_write(p02_scene scene, const p02_config *c, const char *path)
{
    if (c == NULL || path == NULL)
        return P02_E_ARG;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return P02_E_IO;

    fprintf(fp, "# A module that does not exist, answering the commands this\n"
                "# lab sends. Replay it with: p02 session --transcript\n");

    if (scene == P02_SCENE_SILENT) {
        /* The port is open and nothing is on the other end of it. This is
         * what a missing antenna, a sleeping module or the wrong ttyAMA
         * node all look like from here, and telling them apart is a bench
         * job rather than a software one. */
        fprintf(fp, "> AT\n! timeout\n");
        if (fclose(fp) != 0)
            return P02_E_IO;
        return P02_OK;
    }

    bringup(fp, c, scene);
    for (uint32_t i = 0; i < c->cycles; i++)
        cycle(fp, c, scene);

    if (fclose(fp) != 0)
        return P02_E_IO;
    return P02_OK;
}
