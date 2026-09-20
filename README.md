# Pocket Pet

Battery-powered handheld virtual pet on a custom 2-layer PCB. STM32L433,
FreeRTOS, hand-written SSD1306 driver, wake-on-motion.

**Status:** working. Board assembled and running the full firmware — display,
accelerometer, buttons, and battery power all verified on the custom hardware.

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
| Motion | LIS3DH, I2C | Hand-written driver; soldered direct, isolatable via R6 |
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
LIS3DH module carries its own. Measured at 10 k, which is weak for 400 kHz on
a board with no ground plane, so the bus runs at 100 kHz instead — the workload
is roughly 60 bytes per second, so there is nothing to gain from the higher
rate and the slower edges have four times the rise-time margin.

**Peripherals soldered directly.** The original plan was female headers so
modules could be swapped. They were dropped because SW1, the power switch, is
surface-mounted flat to the board and cannot rise with them — sockets would
have left it recessed roughly 8.5 mm below the display plane and complicated
the enclosure. The 0 ohm rail jumpers (R5, R6) preserve the debugging benefit:
either peripheral can be isolated or current-measured by lifting one resistor.

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

## Motion driver

I2C driver for the LIS3DH, written from the datasheet: register map, burst
reads, and a WHO_AM_I identity check that fails loudly rather than returning
plausible garbage.

**Auto-increment must be requested explicitly.** Bit 7 of the register address
tells the part to advance its internal pointer across a burst. Without it,
reading six bytes from `OUT_X_L` returns the same register six times — all
three axes identical, which looks exactly like a dead sensor rather than a
protocol mistake.

**Block Data Update is enabled** so the part cannot refresh an output register
pair between the read of the low byte and the high byte. Without it, occasional
samples are the low half of one reading stitched to the high half of the next.

**Signed arithmetic shift for the 12-bit values.** Output is left-justified
16-bit; high-resolution mode makes only the top 12 bits meaningful. Shifting a
signed type sign-extends and preserves negative readings, where the same shift
on an unsigned type would turn -1 into 4095.

**The I2C address depends on the board, not the chip.** On the Nucleo bench
wiring SA0 and CS float, and the module's own strapping gives 0x19. On the
custom board J6 ties SA0 to ground and CS to 3V3, giving 0x18. The address
therefore lives in `pocket_pet_pins.h` behind a board selector rather than in
the driver, because it is a board fact.

## RTOS architecture

Three tasks, no shared state.

    inputTask    polls and debounces the buttons, detects shake, publishes events
    petTask      owns the pet state, applies events, ticks it forward once a second
    displayTask  redraws at 8 fps, taking a new state when one is available

Tasks communicate only through queues, and the pet state is passed **by value**
rather than by pointer — each task works on its own copy, so there is no shared
memory, no mutex, and no race to get wrong.

The two queues have deliberately different depths. The state queue is depth 1
because the display only ever cares about the newest state; a deeper one would
just mean rendering stale frames. The event queue is depth 4 because a dropped
button press is a bug, where a dropped frame is not.

**Input is edge-triggered, not level.** A press registers on the
released-to-pressed transition, so holding a button feeds the pet once rather
than continuously.

**The display task runs on a timer, not on the queue.** It was originally
blocked on `osWaitForever`, which meant it only redrew when the pet state
changed — once a second, far too slow for animation. It now wakes every 125 ms
and takes a new state if one is waiting, so rendering rate and simulation rate
are decoupled.

## Game design

Three stats decay on separate timescales: hunger fastest, then mood, then
energy. Any stat reaching zero starts a two-minute distress countdown with a
visible warning; ignoring it kills the pet. The countdown exists so that death
is a consequence of ignoring something, not a surprise.

**The lowest stat decides the pet's expression**, so it looks as bad as its
worst problem rather than averaging out and hiding the thing the player needs
to notice.

**Two buttons, four actions.** Left feeds. Right plays on a short press and
toggles sleep on a one-second hold. Shaking the board is a third input,
detected by comparing squared acceleration magnitude against a threshold —
squared, so the comparison needs no square root and no floating point.

**Sleep is the only way to recover energy**, and playing costs it, so the
stats interact rather than being three independent bars.

**Actions trigger a two-second reaction animation.** This confirms the press
registered, and gives the less-frequently-seen animations a reason to exist —
most moods map to slow-moving stats you rarely watch change.



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
- **Cold joints on the USB-C VBUS pin killed the first assembly attempt.**
  VBUS and GND have multiple redundant pins; CC1 and CC2 have exactly one
  each, so a single bad joint silently kills power negotiation with no other
  symptom. Worth probing CC-to-GND for 5.1 k before troubleshooting anything
  else.
- **The Nucleo-32's ST-Link cannot program an external target.** Unlike
  Nucleo-64 and -144, it has no break-out SWD connector — the debugger is
  hardwired to the on-board MCU, and CN2 is reserved for reflashing the
  ST-Link's own processor. A standalone ST-Link V2 is required. Worth knowing
  before planning a bring-up around a Nucleo.
- Debug sessions corrupt I²C. With SWD active, lis3dh_init() fails intermittently; disconnecting the debugger makes it reliable.  SWDIO and SWCLK run adjacent to the I²C lines with no ground plane between them. Worth knowing that on this board a peripheral failure seen under the debugger may not be a real failure.
- **512-byte task stacks were too small.** With `snprintf` and newlib
  reentrancy in play, the display task overflowed its stack and the symptom
  was an intermittent HardFault several minutes after boot, with nothing
  obviously stack-related in the trace. 1 KB per task fixed it.
- **The battery arrived with its JST housing wired backwards.** With no
  reverse-polarity protection on v1.0 that would have destroyed the charger.
  Worth measuring cell polarity against the board's silkscreen before every
  first connection.
- **Cheap USB-C charger modules often omit the CC pull-downs.** A TP4056
  module appeared dead on a compliant USB-C supply because it never
  negotiated; it worked immediately from a legacy USB-A source. The same
  5.1 k resistors this board fits as R2/R3.

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

## Sprites

Sprite data is not included in this repository. The bunny animations are from
[VegaJourney's 32x32 Bunny pack](https://toffeecraft.itch.io/bunny-pixel-animations) (licence permits use but
not redistribution), converted to 1-bit bitmaps.

To build: copy `firmware/common/pet_sprites.h.template` to `pet_sprites.h`
and supply your own sprite arrays. Any 1-bit bitmap works — 
`ssd1306_draw_bitmap()` takes row-major, MSB-first data, which is what
[image2cpp](https://javl.github.io/image2cpp/) produces with horizontal
orientation selected.

## License

MIT