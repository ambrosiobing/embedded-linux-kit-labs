/* Captures that no shield produced.
 *
 * Seven kinds. Three are what the chapters say a good log looks like, and
 * four are each wrong in exactly one way, so that a refusal can be traced to
 * one check rather than to a file that is simply a mess.
 *
 * Two of the four are worth telling apart and a careless generator would
 * merge them. `drift` is a key appearing half way through a capture, which
 * leaves a file no parser reads end to end. `hubbed` is a capture that was
 * short from its first line, because a sensor-hub or ISPU mode routes parts
 * through the LSM6 and the firmware scan finds fewer addresses than the
 * shield carries. The first fails the stability check and the second fails
 * the required-keys check, and a scene that did both would not show which.
 */
#include "p07.h"

#include <string.h>

bool p07_scene_of(const char *name, p07_scene_kind *out)
{
    if (name == NULL || out == NULL)
        return false;
    if (strcmp(name, "iks4a1") == 0)
        *out = P07_SCENE_IKS4A1;
    else if (strcmp(name, "lowg") == 0)
        *out = P07_SCENE_LOWG;
    else if (strcmp(name, "highg") == 0)
        *out = P07_SCENE_HIGHG;
    else if (strcmp(name, "drift") == 0)
        *out = P07_SCENE_DRIFT;
    else if (strcmp(name, "collapsed") == 0)
        *out = P07_SCENE_COLLAPSED;
    else if (strcmp(name, "hubbed") == 0)
        *out = P07_SCENE_HUBBED;
    else if (strcmp(name, "noscale") == 0)
        *out = P07_SCENE_NOSCALE;
    else
        return false;
    return true;
}

p07_status p07_scene_write(p07_scene_kind kind, uint32_t records, FILE *o)
{
    if (o == NULL || records == 0u)
        return P07_E_ARG;

    for (uint32_t i = 0; i < records; i++) {
        unsigned long t = 240u + (i % 7u);          /* tenths of a degree */
        unsigned long rh = 410u + (i % 11u);
        unsigned long pa = 10132u + (i % 5u);

        switch (kind) {
        case P07_SCENE_IKS4A1:
            fprintf(o, "{\"t\":%lu.%lu,\"rh\":%lu.%lu,\"p\":%lu.%lu,"
                       "\"ax\":0.0%lu,\"ay\":0.01,\"az\":1.00}\n",
                    t / 10u, t % 10u, rh / 10u, rh % 10u, pa / 10u, pa % 10u,
                    (unsigned long)(i % 9u));
            break;

        case P07_SCENE_DRIFT:
            /* The same record until half way, then one more key. */
            if (i * 2u < records)
                fprintf(o, "{\"t\":%lu.%lu,\"rh\":%lu.%lu,\"p\":%lu.%lu,"
                           "\"ax\":0.02,\"ay\":0.01,\"az\":1.00}\n",
                        t / 10u, t % 10u, rh / 10u, rh % 10u,
                        pa / 10u, pa % 10u);
            else
                fprintf(o, "{\"t\":%lu.%lu,\"rh\":%lu.%lu,\"p\":%lu.%lu,"
                           "\"ax\":0.02,\"ay\":0.01,\"az\":1.00,"
                           "\"mx\":0.11}\n",
                        t / 10u, t % 10u, rh / 10u, rh % 10u,
                        pa / 10u, pa % 10u);
            break;

        case P07_SCENE_HUBBED:
            /* Short from the first line, not half way through: the humidity
             * and the temperature never arrive at all. */
            fprintf(o, "{\"p\":%lu.%lu,\"ax\":0.02,\"ay\":0.01,"
                       "\"az\":1.00}\n",
                    pa / 10u, pa % 10u);
            break;

        case P07_SCENE_LOWG:
            fprintf(o, "{\"ts\":%lu,\"lowg\":1.0%lu,\"p\":%lu.%lu,"
                       "\"p_scale\":1260}\n",
                    (unsigned long)i, (unsigned long)(i % 9u),
                    pa / 10u, pa % 10u);
            break;

        case P07_SCENE_HIGHG:
            fprintf(o, "{\"ts\":%lu,\"highg_peak\":4%lu.3,\"p\":%lu.%lu,"
                       "\"p_scale\":4060}\n",
                    (unsigned long)i, (unsigned long)(i % 9u),
                    pa / 10u, pa % 10u);
            break;

        case P07_SCENE_NOSCALE:
            /* A pressure with no range beside it, which is a number with two
             * possible meanings on a part that has two scales. */
            fprintf(o, "{\"ts\":%lu,\"lowg\":1.02,\"p\":%lu.%lu}\n",
                    (unsigned long)i, pa / 10u, pa % 10u);
            break;

        case P07_SCENE_COLLAPSED:
            /* The error P08 names: the two streams merged into one field. */
            fprintf(o, "{\"ts\":%lu,\"acc\":1.0%lu,\"p\":%lu.%lu,"
                       "\"p_scale\":1260}\n",
                    (unsigned long)i, (unsigned long)(i % 9u),
                    pa / 10u, pa % 10u);
            break;

        default:
            return P07_E_ARG;
        }
    }
    return P07_OK;
}
