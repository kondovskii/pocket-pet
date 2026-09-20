#include "pet.h"

/* How much each action gives back. Feeding is the strongest because hunger
 * decays fastest; playing costs energy so the stats interact rather than
 * being three independent bars. */
#define FEED_AMOUNT       30
#define PLAY_MOOD_GAIN    20
#define PLAY_ENERGY_COST  10
#define SHAKE_MOOD_GAIN   10
#define SHAKE_ENERGY_COST  5

/* Saturating helpers. Stats are uint8_t, so 100 + 30 is fine but 0 - 1 wraps
 * to 255 — a starving pet would appear instantly full. */
static uint8_t stat_add(uint8_t value, uint8_t amount)
{
    uint16_t sum = (uint16_t)value + amount;
    return (sum > PET_STAT_MAX) ? PET_STAT_MAX : (uint8_t)sum;
}

static uint8_t stat_sub(uint8_t value, uint8_t amount)
{
    return (value > amount) ? (uint8_t)(value - amount) : 0;
}

void pet_init(pet_state_t *pet)
{
    pet->hunger         = PET_STAT_MAX;
    pet->mood           = PET_STAT_MAX;
    pet->energy         = PET_STAT_MAX;
    pet->age_s          = 0;
    pet->tick_count     = 0;
    pet->distress_ticks = 0;
    pet->stage          = PET_STAGE_BABY;
    pet->is_sleeping    = 0;
    pet->is_alive       = 1;
    pet->last_event     = PET_EVENT_FEED;
    pet->reaction_ticks = 0;
}

/*
 * Advance one second.
 *
 * Each stat decays on its own interval, driven off tick_count, so three
 * timescales come out of one tick. Sleeping halves the decay and slowly
 * restores energy, which is what makes sleep a real choice rather than just
 * a pause.
 */
void pet_tick(pet_state_t *pet)
{
    if (!pet->is_alive) {
        return;
    }

    pet->tick_count++;
    pet->age_s++;
    if (pet->reaction_ticks > 0) {
        pet->reaction_ticks--;
    }

    if (pet->stage == PET_STAGE_BABY && pet->age_s >= ADULT_AGE_S) {
        pet->stage = PET_STAGE_ADULT;
    }

    /* Sleeping slows decay and recovers energy. */
    uint32_t slow = pet->is_sleeping ? 2u : 1u;

    if (pet->tick_count % (HUNGER_DECAY_TICKS * slow) == 0) {
        pet->hunger = stat_sub(pet->hunger, 1);
    }
    if (pet->tick_count % (MOOD_DECAY_TICKS * slow) == 0) {
        pet->mood = stat_sub(pet->mood, 1);
    }

    if (pet->is_sleeping) {
        if (pet->tick_count % ENERGY_DECAY_TICKS == 0) {
            pet->energy = stat_add(pet->energy, 2);
        }
    } else {
        if (pet->tick_count % ENERGY_DECAY_TICKS == 0) {
            pet->energy = stat_sub(pet->energy, 1);
        }
    }

    /* Distress: any stat at zero starts a countdown. Recovering the stat
     * resets it, so neglect is forgivable right up until it isn't. */
    if (pet->hunger == 0 || pet->mood == 0 || pet->energy == 0) {
        pet->distress_ticks++;
        if (pet->distress_ticks >= DISTRESS_TICKS) {
            pet->is_alive = 0;
        }
    } else {
        pet->distress_ticks = 0;
    }
}

void pet_apply_event(pet_state_t *pet, pet_event_t event)
{
    if (!pet->is_alive) {
        return;
    }

    /* Most interactions wake the pet rather than being ignored. */
    if (pet->is_sleeping && event != PET_EVENT_SLEEP_TOGGLE) {
        pet->is_sleeping = 0;
        return;
    }

    switch (event) {
    case PET_EVENT_FEED:
        pet->hunger = stat_add(pet->hunger, FEED_AMOUNT);
        pet->last_event     = PET_EVENT_FEED;
        pet->reaction_ticks = REACTION_TICKS;
        break;

    case PET_EVENT_PLAY:
        pet->mood   = stat_add(pet->mood, PLAY_MOOD_GAIN);
        pet->energy = stat_sub(pet->energy, PLAY_ENERGY_COST);
        pet->last_event     = PET_EVENT_PLAY;
        pet->reaction_ticks = REACTION_TICKS;
        break;

    case PET_EVENT_SHAKE:
        pet->mood   = stat_add(pet->mood, SHAKE_MOOD_GAIN);
        pet->energy = stat_sub(pet->energy, SHAKE_ENERGY_COST);
        pet->last_event     = PET_EVENT_SHAKE;
        pet->reaction_ticks = REACTION_TICKS;
        break;

    case PET_EVENT_SLEEP_TOGGLE:
        pet->is_sleeping = !pet->is_sleeping;
        break;

    default:
        break;
    }
}

/*
 * What the display should show. Ordered by precedence: death beats distress,
 * distress beats sleep, sleep beats mood.
 */
pet_mood_t pet_get_mood(const pet_state_t *pet)
{
    if (!pet->is_alive) {
        return PET_MOOD_DEAD;
    }

    if (pet->hunger == 0 || pet->mood == 0 || pet->energy == 0) {
        return PET_MOOD_DISTRESSED;
    }

    if (pet->is_sleeping) {
        return PET_MOOD_SLEEPING;
    }

    /* The lowest stat decides the face — the pet looks as bad as its worst
     * problem, which is the thing you want the player to notice. */
    uint8_t lowest = pet->hunger;
    if (pet->mood < lowest)   lowest = pet->mood;
    if (pet->energy < lowest) lowest = pet->energy;

    if (lowest > 60) return PET_MOOD_HAPPY;
    if (lowest > 25) return PET_MOOD_NEUTRAL;
    return PET_MOOD_SAD;
}
