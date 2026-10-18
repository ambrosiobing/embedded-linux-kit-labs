/* Fleet files the bench did not write.
 *
 * One good sweep and five that are each wrong in exactly one way, which is
 * what makes them useful: a scene that is wrong in two ways cannot tell you
 * which check caught it.
 *
 * The blocks are shaped like what the chapter's own commands print, down to
 * the NanoPi having no `temp=` line at all. That absence is the whole point
 * of the honest-absence arithmetic, so a generator that quietly gave every
 * host a temperature would make the one interesting check unfalsifiable.
 */
#include "p20.h"

#include <string.h>

static void block(FILE *o, const char *name, const char *ram, bool temp,
                  const char *hat, const char *hat2)
{
    fprintf(o, "%s\n", name);
    fprintf(o, " 09:14:03 up 2 days,  3:11,  1 user,  "
               "load average: 0.08, 0.12, 0.09\n");
    fprintf(o, "               total        used        free      shared\n");
    if (ram != NULL)
        fprintf(o, "Mem:           %s       412Mi       6.8Gi        12Mi\n",
                ram);
    if (temp)
        fprintf(o, "temp=46.2'C\n");
    if (hat != NULL)
        fprintf(o, "hat: %s\n", hat);
    if (hat2 != NULL)
        fprintf(o, "hat: %s\n", hat2);
    fprintf(o, "Bus 001 Device 004: ID 10c4:ea60 Silicon Labs CP210x "
               "UART Bridge\n");
}

bool p20_scene_of(const char *name, p20_scene_kind *out)
{
    if (name == NULL || out == NULL)
        return false;
    if (strcmp(name, "good") == 0)
        *out = P20_SCENE_GOOD;
    else if (strcmp(name, "blank-hat") == 0)
        *out = P20_SCENE_BLANK_HAT;
    else if (strcmp(name, "fifth") == 0)
        *out = P20_SCENE_FIFTH;
    else if (strcmp(name, "two-hats") == 0)
        *out = P20_SCENE_TWO_HATS;
    else if (strcmp(name, "undated") == 0)
        *out = P20_SCENE_UNDATED;
    else if (strcmp(name, "pi-no-temp") == 0)
        *out = P20_SCENE_PI_NO_TEMP;
    else
        return false;
    return true;
}

p20_status p20_scene_write(p20_scene_kind kind, FILE *o)
{
    if (o == NULL)
        return P20_E_ARG;

    if (kind != P20_SCENE_UNDATED)
        fprintf(o, "2026-10-18T09:14:03+02:00\n");

    /* The Pi 4 carries the one seated HAT in every scene but one. */
    block(o, "pi4", "7.6Gi", true, "MCC 118",
          (kind == P20_SCENE_TWO_HATS) ? "SIM7600E-H" : NULL);

    block(o, "pi3bp", "923Mi", kind != P20_SCENE_PI_NO_TEMP, "none", NULL);

    /* The blank-hat scene is the chapter's fourth pitfall: a host whose HAT
     * line never appeared, which reads as a broken script rather than as an
     * empty header. */
    block(o, "pi3", "923Mi", true,
          (kind == P20_SCENE_BLANK_HAT) ? NULL : "none", NULL);

    /* No temp= here in any scene. vcgencmd is a Raspberry Pi tool and this
     * is a NanoPi, and the shorter block is a true statement about it. */
    block(o, "air", "485Mi", false, "none", NULL);

    if (kind == P20_SCENE_FIFTH)
        block(o, "pi5", "7.6Gi", true, "none", NULL);

    return P20_OK;
}
