/* The top-level keys of one JSON object, without a JSON library.
 *
 * What this needs is narrow: the key set of a flat record, so that a drifting
 * schema can be named and a forbidden field can be refused. It does not need
 * the values, so it never parses a number and never allocates.
 *
 * It is still a parser, and the three ways a naive one goes wrong are each
 * handled and each tested. A brace inside a string is not a brace. A key
 * inside a nested object is not a key of the record. And a line that is not
 * a JSON object is refused whole rather than half read, because a half-read
 * record is how a file ends up with a schema nobody declared.
 *
 * **What it does not reach**, stated rather than left to be found: a string
 * escape is handled only far enough to keep the parser in step. The escaped
 * character is kept as itself, so a key written `"a\nb"` would be read as
 * `anb` rather than as a key containing a newline. No key in P07's or P08's
 * chapters has an escape in it, and a key that needs one is a key worth
 * objecting to for other reasons.
 */
#include "p07.h"

#include <string.h>

static const char *skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    return s;
}

/* Past a JSON string, optionally copying its contents. Returns NULL for an
 * unterminated one, which is a refusal of the whole line. */
static const char *scan_string(const char *s, char *out, size_t outsz,
                               bool *truncated)
{
    if (*s != '"')
        return NULL;
    s++;

    size_t i = 0;
    for (;;) {
        if (*s == '\0')
            return NULL;
        if (*s == '\\') {
            if (s[1] == '\0')
                return NULL;
            if (out != NULL) {
                if (i + 1u < outsz)
                    out[i++] = s[1];
                else if (truncated != NULL)
                    *truncated = true;
            }
            s += 2;
            continue;
        }
        if (*s == '"') {
            if (out != NULL && outsz > 0u)
                out[i] = '\0';
            return s + 1;
        }
        if (out != NULL) {
            if (i + 1u < outsz)
                out[i++] = *s;
            else if (truncated != NULL)
                *truncated = true;
        }
        s++;
    }
}

/* Past one JSON value of any kind. An object or an array is skipped whole,
 * which is what keeps a nested key from being counted as a key of the
 * record. */
static const char *scan_value(const char *s)
{
    s = skip_ws(s);
    if (*s == '"')
        return scan_string(s, NULL, 0u, NULL);

    if (*s == '{' || *s == '[') {
        char open = *s;
        char close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (*s != '\0') {
            if (*s == '"') {
                s = scan_string(s, NULL, 0u, NULL);
                if (s == NULL)
                    return NULL;
                continue;
            }
            if (*s == open) {
                depth++;
            } else if (*s == close) {
                depth--;
                if (depth == 0)
                    return s + 1;
            }
            s++;
        }
        return NULL;
    }

    /* A number, or true, false, null. It ends where the object does. */
    while (*s != '\0' && *s != ',' && *s != '}' && *s != ']' &&
           *s != ' ' && *s != '\t')
        s++;
    return s;
}

bool p07_keys_of(const char *line, p07_keyset *out)
{
    if (line == NULL || out == NULL)
        return false;
    memset(out, 0, sizeof *out);

    const char *s = skip_ws(line);
    if (*s != '{')
        return false;
    s = skip_ws(s + 1);
    if (*s == '}')
        return *skip_ws(s + 1) == '\0';   /* an empty object has no keys */

    for (;;) {
        s = skip_ws(s);
        if (*s != '"')
            return false;

        char key[P07_KEY_MAX];
        bool trunc = false;
        s = scan_string(s, key, sizeof key, &trunc);
        if (s == NULL)
            return false;
        if (trunc)
            out->truncated = true;

        s = skip_ws(s);
        if (*s != ':')
            return false;
        s++;

        if (out->n < P07_KEYS_MAX) {
            memcpy(out->k[out->n], key, sizeof key);
            out->n++;
        } else {
            out->overflowed = true;
        }

        s = scan_value(s);
        if (s == NULL)
            return false;
        s = skip_ws(s);
        if (*s == ',') {
            s++;
            continue;
        }
        if (*s == '}')
            break;
        return false;
    }

    /* Nothing after the object. One line, one record, which is what makes a
     * file of these readable end to end. */
    return *skip_ws(s + 1) == '\0';
}

bool p07_keys_have(const p07_keyset *s, const char *key)
{
    if (s == NULL || key == NULL)
        return false;
    for (unsigned i = 0; i < s->n; i++) {
        if (strcmp(s->k[i], key) == 0)
            return true;
    }
    return false;
}

/* As a set rather than as a list. The chapter's rule is "the same keys from
 * the first line to the last", and a reader of the file parses by name, so
 * two records carrying the same names in a different order carry the same
 * schema. Order would be a stricter test of the firmware and a weaker test
 * of the rule. */
bool p07_keys_equal(const p07_keyset *a, const p07_keyset *b)
{
    if (a == NULL || b == NULL)
        return false;
    if (a->n != b->n)
        return false;
    for (unsigned i = 0; i < a->n; i++) {
        if (!p07_keys_have(b, a->k[i]))
            return false;
    }
    return true;
}
