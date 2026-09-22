#ifndef PET_H
#define PET_H

#include <stdint.h>

/*
 * Pet state machine.
 *
 * Three stats decay on their own timescales. Any stat hitting zero starts a
 * distress countdown; letting it run out kills the pet. The countdown exists
 * so neglect is recoverable if you notice — death should be a consequence of
 * ignoring a visible warning, not a surprise.
 */

#define PET_STAT_MAX          100

/* Ticks are one second. Decay is applied every N ticks, so a stat's lifetime
 * from full is PET_STAT_MAX * interval seconds. */
#define HUNGER_DECAY_TICKS    8    /* ~13 min from full to empty */
#define MOOD_DECAY_TICKS      12   /* ~20 min */
#define ENERGY_DECAY_TICKS    15   /* ~25 min */

/* How long the pet survives with a stat at zero before dying. */
#define DISTRESS_TICKS        120  /* 2 minutes of visible warning */

/* Age at which a baby becomes an adult. */
#define ADULT_AGE_S           600  /* 10 minutes */

/* How long a reaction animation plays. Ticks are one second. */
#define REACTION_TICKS  2

typedef enum {
    PET_STAGE_BABY = 0,
    PET_STAGE_ADULT
} pet_stage_t;

typedef enum {
    PET_MOOD_HAPPY = 0,
    PET_MOOD_NEUTRAL,
    PET_MOOD_SAD,
    PET_MOOD_SLEEPING,
    PET_MOOD_DISTRESSED,
    PET_MOOD_DEAD
} pet_mood_t;

typedef enum {
    PET_EVENT_FEED = 0,
    PET_EVENT_PLAY,        /* from a button */
    PET_EVENT_SHAKE,       /* from the accelerometer */
    PET_EVENT_SLEEP_TOGGLE
} pet_event_t;

typedef struct {
    uint8_t  hunger;
    uint8_t  mood;
    uint8_t  energy;

    uint32_t age_s;
    uint32_t tick_count;      /* drives the decay intervals */
    uint16_t distress_ticks;  /* counts up while any stat is zero */

    pet_stage_t stage;
    uint8_t  is_sleeping;
    uint8_t  is_alive;

    /* Reaction display: set when an event arrives, counts down each tick.
     * The display task shows a reaction animation while this is non-zero,
     * which both confirms the button press and gives the less-used
     * animations something to do. */
    pet_event_t last_event;
    uint8_t     reaction_ticks;

} pet_state_t;



void pet_init(pet_state_t *pet);
void pet_tick(pet_state_t *pet);
void pet_apply_event(pet_state_t *pet, pet_event_t event);

/* Derived state — what the display should show. Computed rather than stored,
 * so there is one place that decides and no chance of it going stale. */
pet_mood_t pet_get_mood(const pet_state_t *pet);

#endif /* PET_H */
