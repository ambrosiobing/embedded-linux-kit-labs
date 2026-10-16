/* The ordering, read off a recorded session instead of enforced during one.
 *
 * The session code cannot break the rule, because the function that would
 * transmit refuses while the receiver is powered. That protects this
 * program and nothing else. A bench session typed by hand in minicom, or a
 * script somebody wrote in an afternoon, has no such function in it, and the
 * evidence it leaves behind is a transcript.
 *
 * So this reads one and answers the same question about it: was the radio
 * ever asked to transmit while the receiver was still on. It needs nothing
 * from the rest of the program, and it works on any transcript, including
 * one this program never produced.
 */

#include "p03.h"

#include <string.h>

static bool is_transmit(const char *cmd)
{
    /* Every command that puts the radio on the air in this lab. SMCONF
     * configures the session rather than sending, and it is counted here
     * too: the chapter's ordering is that nothing MQTT happens until the
     * receiver is down, and a configure that works while it is on would
     * teach the reader the rule is advisory. */
    return strncmp(cmd, "AT+SMCONF", 9) == 0 ||
           strncmp(cmd, "AT+SMCONN", 9) == 0 ||
           strncmp(cmd, "AT+SMPUB", 8) == 0 ||
           strncmp(cmd, "AT+SMSUB", 8) == 0;
}

p03_status p03_audit_file(const char *path, p03_audit *out)
{
    if (path == NULL || out == NULL)
        return P03_E_ARG;
    memset(out, 0, sizeof *out);

    linereader r;
    if (linereader_open(&r, path) != LINEREADER_OK)
        return P03_E_IO;

    bool gnss_on = false;
    for (;;) {
        const char *line = NULL;
        size_t len = 0;
        if (linereader_line(&r, &line, &len) != LINEREADER_OK)
            break;
        out->lines++;

        if (line[0] == '#')
            continue;
        if (line[0] != '>' )
            continue;             /* only what the host sent matters here */
        const char *cmd = (line[1] == ' ') ? line + 2 : line + 1;

        if (strcmp(cmd, "AT+CGNSPWR=1") == 0) {
            gnss_on = true;
            out->gnss_on++;
            continue;
        }
        if (strcmp(cmd, "AT+CGNSPWR=0") == 0) {
            gnss_on = false;
            out->gnss_off++;
            continue;
        }
        if (is_transmit(cmd)) {
            out->transmits++;
            if (gnss_on) {
                if (out->transmits_with_gnss_on == 0) {
                    out->first_bad_line = out->lines;
                    snprintf(out->first_bad_cmd, sizeof out->first_bad_cmd,
                             "%s", cmd);
                }
                out->transmits_with_gnss_on++;
            }
        }
    }

    linereader_close(&r);
    return P03_OK;
}

bool p03_audit_clean(const p03_audit *a)
{
    if (a == NULL)
        return false;
    return a->transmits_with_gnss_on == 0;
}
