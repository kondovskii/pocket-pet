#ifndef PET_H
#define PET_H

#include <stdint.h>

/*
 * The pet's state, as a plain value type.
 *
 * Deliberately small and copyable: it gets passed through a queue by value,
 * so the display task works on its own snapshot rather than reading memory
 * the pet task might be modifying at the same time. No shared pointer, no
 * lock needed.
 */
typedef struct {
    uint8_t hunger;   /* 0 = starving, 100 = full     */
    uint8_t mood;     /* 0 = miserable, 100 = happy   */
    uint32_t age_s;   /* seconds since boot           */
} pet_state_t;

void pet_init(pet_state_t *pet);
void pet_tick(pet_state_t *pet);

/* --- Input events -------------------------------------------------------- */
typedef enum {
    PET_EVENT_FEED = 0,
    PET_EVENT_PLAY = 1
} pet_event_t;

void pet_apply_event(pet_state_t *pet, pet_event_t event);


#endif /* PET_H */
