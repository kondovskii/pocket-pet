# Pocket Pet

Battery-powered handheld virtual pet on a custom 2-layer PCB. STM32L433,
FreeRTOS, hand-written SSD1306 driver, wake-on-motion.

**Status:** boards at fab (`v1.0-boards-ordered`). SSD1306 driver written and
validated on hardware. FreeRTOS layer next.

## Why this project

Built to close four specific gaps: custom PCB design, bare-metal ARM
programming, RTOS usage, and datasheet-level driver work. Every part was chosen
and every driver written from the datasheet rather than pulled from a library,
so that each decision is defensible.

## Hardware

| Block | Part | Notes |
|---|---|---|
| MCU | STM32L433CCT6 (LQFP-48) | Cortex-M4F, 256K flash, low-power STOP2 modes |
| Charger | MCP73831 | ~196 mA charge current (R_PROG = 5.1 k) |
| Regulator | MCP1700-3302 | 3.3 V LDO, low quiescent current |
| Input | USB-C, charge only | 5.1 k CC1/CC2 pull-downs for correct sink advertisement |
| Display | SSD1306 128x64, SPI | Hand-written driver, no vendor library |
| Motion | LIS3DH, I2C | On a header so it can be swapped or removed |
| Battery | 500 mAh LiPo | JST-PH |

2-layer FR4, designed in KiCad, fabricated by JLCPCB.

## Design decisions

**MSI at 32 MHz, not HSI16, not 80 MHz.** MSI is the oscillator the L4 wakes on
coming out of STOP mode, and it can be trimmed against an LSE crystal. 32 MHz
was chosen over the 80 MHz maximum because the workload — a 128x64 display
refresh and an accelerometer read — does not need it, and lower active current
is free. The power story for a battery device is duty cycle (STOP2 at roughly
1 uA versus milliamps awake), not clock rate.

**USB-C is charge-only.** No USB data. The 5.1 k pull-downs on CC1 and CC2 are
what make a charger treat the board as a valid sink; without them a compliant
USB-C supply provides no VBUS at all. Programming and debug go over SWD, which
keeps the firmware simpler and the BOM smaller.

**0 ohm jumpers on the peripheral rails.** R5 and R6 sit in series with the
display and accelerometer supplies so either can be cut to measure that
peripheral's current draw in isolation, or to remove it from the picture while
debugging.

**I2C pull-ups fitted but not populated.** R7 and R8 are marked DNP because the
LIS3DH breakout carries its own. The footprints exist so they can be added if a
different module is used.

**Peripherals on headers, not soldered down.** The display and accelerometer are
socketed. On a first custom board, being able to swap a suspect module beats
saving a few millimetres.

**SWD broken out to a 6-pin header** with SWDIO, SWCLK, SWO, NRST, 3V3 and GND,
in ST-Link pin order.

## Display driver

Written from the SSD1306 datasheet rather than pulled from a library: SPI
transport with manual DC and CS control, the full initialisation sequence, a
1024-byte framebuffer, Bresenham line drawing, and a 5x7 column-major font.

**Draw to RAM, flush once.** Every primitive writes to the framebuffer and
nothing reaches the panel until `ssd1306_update_screen()`. Frames appear whole
rather than building up visibly, and the flush is a single 1024-byte SPI burst
instead of hundreds of small transfers.

**The font is column-major** because that matches the controller's own vertical
byte layout, so rendering a glyph is a single bit test per pixel with no
transposition.

**Clipping lives in one place.** Only `ssd1306_draw_pixel()` bounds-checks;
lines, rectangles and text all inherit it. One guard to get right instead of
six, and no way for them to disagree.

**Validated on a NUCLEO-L432KC with a 7-pin SPI module before the custom boards
arrived**, so that display bring-up and board bring-up could be debugged
independently. A blank screen on the custom board now means a hardware fault,
not an unproven driver.

## Known issues (v1.0)

Found during a design review after the boards were already at fab. Documented
rather than hidden — these are the v1.1 fix list.

- **No ground pour on either layer.** Every return path is a narrow trace, which
  raises loop inductance and leaves the MCP73831 with no thermal copper. Expect
  the charger to run warm and possibly fold back current.
- **No reverse-polarity protection on the battery input.** A backwards cell would
  destroy the charger. Mitigated only by silkscreen.
- **No load-sharing.** The load hangs off the battery node, so the board is not
  designed to run from USB with no cell installed.
- **No battery voltage sensing.** No way to report state of charge. A divider to
  an ADC pin is the fix.
- **Decoupling placement is loose** — several caps sit millimetres from the pins
  they serve, which without a plane makes them largely ineffective.
- **STAT pin unconnected**, so there is no charge indicator LED.
- Silkscreen needs a cleanup pass: unannotated mounting holes, overlapping value
  text, no pin-1 markers or header labels.

## Repository layout

    hardware/pocket-pet/     KiCad project, fab outputs under fab/
    firmware/pocket-pet-nucleo/   Bring-up project, NUCLEO-L432KC
    firmware/pocket-pet-board/    Custom board project
    docs/                    Schematic PDF, renders, BOM

## Building the firmware

Requires STM32CubeIDE. Import the project with "Copy projects into workspace"
unchecked so the IDE edits the repo in place; keep the workspace outside the
repository. The .ioc file is the source of truth for pin configuration — if you
change pins, regenerate from CubeMX rather than editing generated code.

## License

MIT