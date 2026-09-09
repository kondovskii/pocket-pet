#include "pet.h"

/* How much each stat decays per tick. Slow enough that the pet does not die
 * while you are debugging, fast enough that changes are visible. */
#define HUNGER_DECAY   1
#define MOOD_DECAY     1

/* Mood decays faster when the pet is getting hungry. The threshold is well
 * above zero so the penalty has time to matter — keyed to hunger == 0 it
 * would never fire, since mood reaches 0 on the same tick hunger does. */
#define HUNGER_LOW_THRESHOLD  40

void pet_init(pet_state_t *pet)
{
    pet->hunger = 100;
    pet->mood   = 100;
    pet->age_s  = 0;
}

/*
 * Advance the pet by one tick.
 *
 * Saturating subtraction rather than plain decrement: hunger is unsigned, so
 * 0 - 1 wraps to 255 and the pet would appear to become fully fed the moment
 * it starved. Checking before subtracting is the fix.
 */
void pet_tick(pet_state_t *pet)
{
    if (pet->hunger >= HUNGER_DECAY) {
        pet->hunger -= HUNGER_DECAY;
    } else {
        pet->hunger = 0;
    }

    /* Mood decays faster when the pet is hungry. */
    uint8_t mood_loss = (pet->hunger < HUNGER_LOW_THRESHOLD) ? (MOOD_DECAY * 3) : MOOD_DECAY;

    if (pet->mood >= mood_loss) {
        pet->mood -= mood_loss;
    } else {
        pet->mood = 0;
    }

    pet->age_s++;
}


/*
 * Apply a user action. Saturating addition for the same reason the decay
 * saturates: hunger is uint8_t, so 100 + 20 is fine but 250 + 20 would wrap
 * to 14 and the pet would starve the instant you fed it.
 */
void pet_apply_event(pet_state_t *pet, pet_event_t event)
{
    switch (event) {
    case PET_EVENT_FEED:
        pet->hunger = (pet->hunger > 80) ? 100 : (uint8_t)(pet->hunger + 20);
        break;

    case PET_EVENT_PLAY:
        pet->mood = (pet->mood > 85) ? 100 : (uint8_t)(pet->mood + 15);
        /* Playing is tiring. */
        pet->hunger = (pet->hunger >= 5) ? (uint8_t)(pet->hunger - 5) : 0;
        break;

    default:
        break;
    }
}
