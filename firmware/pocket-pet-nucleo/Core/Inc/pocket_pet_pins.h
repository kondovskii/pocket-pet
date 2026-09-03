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

/* ---- Nucleo bench setup ------------------------------------------------- */
/* Filled in from UM2179 when the display is physically wired up.            */

#endif /* POCKET_PET_PINS_H */
