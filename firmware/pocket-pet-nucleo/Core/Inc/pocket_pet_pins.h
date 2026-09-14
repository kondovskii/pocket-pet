#ifndef POCKET_PET_PINS_H
#define POCKET_PET_PINS_H

#include "main.h"

/*
 * Board pin abstraction.
 *
 * The driver code never names a GPIO port or pin directly. It refers only to
 * the symbols below. To move the firmware from the Nucleo bench setup to the
 * custom board, only this file changes.
 *
 * This exists because PB3 is the user LED on the Nucleo and SWO on the custom
 * board: the same pin does a different job on each, and that class of mistake
 * is silent and expensive.
 */

/* ---- Custom board (pocket-pet v1.0) ------------------------------------- */
/* These alias the CubeMX-generated labels from main.h. The .ioc stays the   */
/* single source of truth; change a pin there and it propagates here.        */

#define OLED_CS_PORT     OLED_CS_GPIO_Port
#define OLED_CS_PIN      OLED_CS_Pin

#define OLED_DC_PORT     OLED_DC_GPIO_Port
#define OLED_DC_PIN      OLED_DC_Pin

#define OLED_RST_PORT    OLED_RST_GPIO_Port
#define OLED_RST_PIN     OLED_RST_Pin

#define OLED_SPI         hspi1

/* ---- Board selection ---------------------------------------------------- */
/* Exactly one. This is the only line that changes between targets. */
#define BOARD_NUCLEO
/* #define BOARD_POCKET_PET_V1 */

#if defined(BOARD_POCKET_PET_V1)
  /* J6 ties SA0 to GND and CS to 3V3, so the address is 0x18. */
  #define LIS3DH_ADDR_7BIT   0x18
#elif defined(BOARD_NUCLEO)
  /* SA0 and CS float on the bench wiring; the module's own strapping
   * gives 0x19. */
  #define LIS3DH_ADDR_7BIT   0x19
#else
  #error "No board selected in pocket_pet_pins.h"
#endif
/* ---- Nucleo bench setup ------------------------------------------------- */
/* Filled in from UM2179 when the display is physically wired up.            */

#endif /* POCKET_PET_PINS_H */
