/* Sequence accounting, shared by every lab that records a numbered stream.
 *
 * A recorder that cannot say how much it lost is a listener. Both labs that
 * record one keep the same four numbers, so the arithmetic that produces them
 * lives here once instead of twice:
 *
 *   gaps        how many sequence numbers never arrived
 *   duplicates  how many arrived more than once
 *   reorders    how many arrived after a later one
 *   expected    how many should have arrived between the first and the last
 *
 * Nothing here repairs anything. A gap is counted and left as a gap, because
 * the count is the measurement the lab produces and a sink that fills one in
 * has thrown away its own result.
 *
 * One consequence of counting as the stream arrives, rather than sorting it
 * afterwards, is worth stating because it shows up in both labs' numbers. A
 * sample that arrives after its successor is counted twice over: once as a
 * gap, at the moment the successor arrived and the earlier number had not,
 * and once as a reorder when it turns up. The gap is not taken back. Taking
 * it back would need a window of sequence numbers held open, and a window
 * needs a policy for how long to hold it, which is a larger decision than
 * either lab needs: a USB CDC pipe and a recorded file both deliver in
 * order, so a reorder here means something unusual happened and two numbers
 * describing it is more useful than one number quietly adjusted.
 *
 * The comparison is unsigned serial arithmetic, which is the whole reason this
 * file exists rather than a plain less-than. A counter of 32 bits returns to
 * zero after about four billion samples, which is four and a half weeks at one
 * kilohertz, and a plain comparison reads that single step as a jump backwards
 * of four billion. Counting it as a reorder instead costs one subtraction: the
 * distance from the last sequence number to this one, taken in unsigned
 * arithmetic, is small when the stream moved forward across the wrap and close
 * to the full range when it genuinely went backwards. Half the range is the
 * dividing line, and a step of exactly half is read as backwards, which is
 * arbitrary and is written down here so that it is not rediscovered.
 *
 * Header only, and the functions are static inline. There is no library to
 * build, no object file for two programs to collide over, and no Makefile in
 * this volume has to learn about a second directory.
 */
#ifndef SEQTRACK_H
#define SEQTRACK_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t received;    /* every sample seen, duplicates and reorders included */
    uint32_t expected;    /* the span from the first sequence number to the last */
    uint32_t gaps;
    uint32_t duplicates;
    uint32_t reorders;
    uint32_t first_seq;
    uint32_t last_seq;
    bool started;
} seqtrack;

#define SEQTRACK_HALF 0x80000000u

static inline void seqtrack_init(seqtrack *t)
{
    if (t != NULL)
        memset(t, 0, sizeof *t);
}

/* True when seq sits behind last in serial order. See the note at the top of
 * this file: the distance is taken in unsigned arithmetic so that the wrap
 * from the largest sequence number to zero is one step forward, not a leap
 * backwards. */
static inline bool seqtrack_behind(uint32_t seq, uint32_t last)
{
    return (uint32_t)(seq - last) >= SEQTRACK_HALF;
}

static inline void seqtrack_see(seqtrack *t, uint32_t seq)
{
    if (t == NULL)
        return;
    if (!t->started) {
        t->started = true;
        t->first_seq = seq;
        t->last_seq = seq;
        t->received = 1;
        t->expected = 1;
        return;
    }
    t->received++;

    if (seq == t->last_seq) {
        t->duplicates++;
        return;
    }
    if (seqtrack_behind(seq, t->last_seq)) {
        /* Out of order rather than missing. Counted separately because the two
         * have different causes: a reorder is the transport shuffling, a gap
         * is the transport losing. */
        t->reorders++;
        return;
    }
    uint32_t step = seq - t->last_seq;
    if (step > 1)
        t->gaps += step - 1;
    t->last_seq = seq;
    t->expected = t->last_seq - t->first_seq + 1u;
}

#endif /* SEQTRACK_H */
