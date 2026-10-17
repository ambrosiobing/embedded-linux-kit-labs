/* The blink, as arithmetic on a time.
 *
 * Five steps of six hundred milliseconds. Each channel alone, then the three
 * together, then dark.
 *
 * The step that matters to the acceptance test is the fourth, and the three
 * before it are what make it mean anything. Three LEDs lit at once show that
 * three pins can source together; three LEDs lit one at a time first, in a
 * known order, show which pin is which. Without that a reader who has
 * crossed two jumpers sees a passing semaphore, and the crossed pair is
 * found later by P01 or P02, where green means registered and the bench says
 * yellow.
 *
 * The dark step is not padding either. It ends every cycle at the state the
 * next lab needs to find, and it makes a program that is still running
 * distinguishable by eye from one that has exited: a cycling semaphore goes
 * dark and comes back, and an exited one goes dark and stays.
 */
#include "p15.h"

static const uint8_t STEP_LIT[P15_STEPS] = {
    P15_LIT(P15_GREEN),
    P15_LIT(P15_YELLOW),
    P15_LIT(P15_RED),
    P15_LIT_ALL,
    P15_LIT_NONE
};

unsigned p15_step_at(uint32_t ms)
{
    uint32_t within = ms % (uint32_t)P15_PLAN_MS;
    return (unsigned)(within / (uint32_t)P15_STEP_MS);
}

uint8_t p15_frame_at(uint32_t ms)
{
    return STEP_LIT[p15_step_at(ms)];
}

uint8_t p15_step_lit(unsigned i)
{
    if (i >= P15_STEPS)
        return P15_LIT_NONE;
    return STEP_LIT[i];
}
