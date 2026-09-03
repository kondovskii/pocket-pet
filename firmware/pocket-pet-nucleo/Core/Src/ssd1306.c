#include "ssd1306.h"
#include "pocket_pet_pins.h"
#include "spi.h"
#include "main.h"
#include "ssd1306_font.h"

#define SSD1306_SPI_TIMEOUT  100  /* ms; a stalled bus should fail, not hang */
/*
 * Framebuffer. 1024 bytes: 8 pages of 128 columns.
 *
 * One byte holds 8 vertically-stacked pixels. Bit 0 is the topmost pixel of
 * the page, bit 7 the bottom. This is the controller's native layout, so a
 * flush is a straight memcpy-style burst with no bit rearranging.
 *
 * Drawing writes here only. Nothing reaches the panel until update_screen().
 */
static uint8_t ssd1306_buffer[SSD1306_BUFFER_SIZE];
/*
 * Hardware reset.
 *
 * The SSD1306 needs RES held low briefly at power-up to reach a known state.
 * The datasheet specifies a minimum pulse of 3 us; 10 ms is used here because
 * it costs nothing at startup and removes any doubt.
 */
void ssd1306_reset(void)
{
    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}

/*
 * Send one command byte.
 *
 * DC low tells the controller the byte is a command, not display data. CS is
 * driven manually because hardware NSS deasserts between bytes, and a
 * multi-byte transfer must be held under a single continuous CS assertion.
 */
void ssd1306_write_cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);

    HAL_SPI_Transmit(&OLED_SPI, &cmd, 1, SSD1306_SPI_TIMEOUT);

    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}

/*
 * Send a block of display data.
 *
 * DC high routes the bytes into display RAM. Used only for the framebuffer
 * flush. Command argument bytes go through write_cmd() with DC low, since
 * they are part of the command stream rather than pixel data.
 */
void ssd1306_write_data(uint8_t *data, uint16_t size)
{
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);

    HAL_SPI_Transmit(&OLED_SPI, data, size, SSD1306_SPI_TIMEOUT);

    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}

/*
 * Bring the panel from reset to a displaying state.
 *
 * Order matters: the display is left OFF for the whole configuration and only
 * enabled at the end, so nothing partially-configured is ever shown. Values
 * are from the SSD1306 datasheet section 9 and the application note's
 * recommended initialisation for a 128x64 panel.
 */
void ssd1306_init(void)
{
    ssd1306_reset();

    ssd1306_write_cmd(SSD1306_DISPLAY_OFF);

    /* Timing. 0x80 = divide ratio 1, oscillator at its nominal frequency.
     * This sets the refresh rate; the default is fine for a static UI. */
    ssd1306_write_cmd(SSD1306_SET_CLOCK_DIV);
    ssd1306_write_cmd(0x80);

    /* Multiplex ratio: how many rows are scanned. 63 = 64 rows (height - 1). */
    ssd1306_write_cmd(SSD1306_SET_MUX_RATIO);
    ssd1306_write_cmd(SSD1306_HEIGHT - 1);

    /* Vertical shift of the visible area. No offset. */
    ssd1306_write_cmd(SSD1306_SET_DISPLAY_OFFSET);
    ssd1306_write_cmd(0x00);

    /* RAM row mapped to the top line of the panel. */
    ssd1306_write_cmd(SSD1306_SET_START_LINE);

    /* Charge pump ON. The panel needs roughly 7-9 V to light a pixel and only
     * has 3.3 V, so the controller generates it internally. This is OFF at
     * reset: omit it and every command still succeeds, the framebuffer still
     * fills, and the screen stays completely black with no error anywhere. */
    ssd1306_write_cmd(SSD1306_SET_CHARGE_PUMP);
    ssd1306_write_cmd(SSD1306_CHARGE_PUMP_ON);

    /* Horizontal addressing: the column pointer auto-advances and wraps to the
     * next page, so the whole 1024-byte framebuffer goes out in one SPI burst
     * rather than page-by-page with a pointer reset between each. */
    ssd1306_write_cmd(SSD1306_SET_MEM_ADDR_MODE);
    ssd1306_write_cmd(SSD1306_MEM_ADDR_HORIZONTAL);

    /* Orientation. These two mirror the image horizontally and vertically.
     * If the first image comes up flipped, change them here rather than
     * touching any drawing code. */
    ssd1306_write_cmd(SSD1306_SET_SEGMENT_REMAP);
    ssd1306_write_cmd(SSD1306_SET_COM_SCAN_DEC);

    /* COM pin hardware layout. 0x12 is the alternative configuration required
     * by 128x64 panels; 0x02 is for 128x32 and gives a squashed image here. */
    ssd1306_write_cmd(SSD1306_SET_COM_PINS);
    ssd1306_write_cmd(0x12);

    /* Contrast. 0xFF is maximum; lower values reduce current draw, which
     * matters on a battery device. Tune once it is running. */
    ssd1306_write_cmd(SSD1306_SET_CONTRAST);
    ssd1306_write_cmd(0xFF);

    /* Pre-charge period. 0xF1 is the value recommended when the charge pump
     * is enabled. */
    ssd1306_write_cmd(SSD1306_SET_PRECHARGE);
    ssd1306_write_cmd(0xF1);

    /* VCOMH deselect level, 0x30 = ~0.83 x VCC. Affects contrast and how
     * cleanly pixels switch off. */
    ssd1306_write_cmd(SSD1306_SET_VCOM_DESELECT);
    ssd1306_write_cmd(0x30);

    /* Show RAM contents rather than forcing all pixels on, and use normal
     * (not inverted) polarity. */
    ssd1306_write_cmd(SSD1306_DISPLAY_RAM);
    ssd1306_write_cmd(SSD1306_DISPLAY_NORMAL);

    ssd1306_write_cmd(SSD1306_DEACTIVATE_SCROLL);

    ssd1306_write_cmd(SSD1306_DISPLAY_ON);
}
/* Fill every byte with a pattern. 0x00 all off, 0xFF all on. */
void ssd1306_fill(uint8_t pattern)
{
    for (uint16_t i = 0; i < SSD1306_BUFFER_SIZE; i++) {
        ssd1306_buffer[i] = pattern;
    }
}

void ssd1306_clear(void)
{
    ssd1306_fill(0x00);
}

/*
 * Push the whole framebuffer to the panel.
 *
 * The address window is reset first because commands elsewhere can leave the
 * pointer somewhere unexpected; setting it explicitly every flush makes this
 * function independent of whatever ran before it.
 *
 * With horizontal addressing mode the column pointer auto-advances and wraps
 * across pages, so all 1024 bytes go out as a single SPI transfer rather than
 * eight page-by-page writes.
 */
void ssd1306_update_screen(void)
{
    ssd1306_write_cmd(SSD1306_SET_COLUMN_ADDR);
    ssd1306_write_cmd(0);
    ssd1306_write_cmd(SSD1306_WIDTH - 1);

    ssd1306_write_cmd(SSD1306_SET_PAGE_ADDR);
    ssd1306_write_cmd(0);
    ssd1306_write_cmd(SSD1306_PAGES - 1);

    ssd1306_write_data(ssd1306_buffer, SSD1306_BUFFER_SIZE);
}

/*
 * Set or clear a single pixel in the framebuffer.
 *
 * Coordinates are (0,0) at top-left, x rightward, y downward.
 *
 * The buffer is 8 pages of 128 columns, and each byte holds 8 vertically
 * stacked pixels. So:
 *
 *   page   = y / 8     which of the 8 horizontal bands the pixel is in
 *   bit    = y % 8     how far down within that band, bit 0 at the top
 *   index  = page * SSD1306_WIDTH + x
 *
 * Out-of-range coordinates are dropped rather than wrapping. A wrapped pixel
 * appears somewhere unexpected on screen and looks like a drawing bug; a
 * dropped one simply does not appear, which is far easier to trace back to
 * bad input.
 */
void ssd1306_draw_pixel(uint8_t x, uint8_t y, ssd1306_colour_t colour)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }

    uint16_t index = ((y / 8) * SSD1306_WIDTH) + x;
    uint8_t  mask  = (uint8_t)(1u << (y % 8));

    if (colour == SSD1306_PIXEL_ON) {
        ssd1306_buffer[index] |= mask;
    } else {
        ssd1306_buffer[index] &= (uint8_t)~mask;
    }
}

/*
 * Horizontal and vertical lines are separate from the general case because
 * they are the common ones (borders, UI dividers, progress bars) and a plain
 * loop is both faster and easier to read than Bresenham.
 *
 * Clipping is handled by draw_pixel, so an overlong width simply stops at the
 * screen edge rather than needing a bounds check here.
 */
void ssd1306_draw_hline(uint8_t x, uint8_t y, uint8_t w, ssd1306_colour_t colour)
{
    for (uint8_t i = 0; i < w; i++) {
        ssd1306_draw_pixel((uint8_t)(x + i), y, colour);
    }
}

void ssd1306_draw_vline(uint8_t x, uint8_t y, uint8_t h, ssd1306_colour_t colour)
{
    for (uint8_t i = 0; i < h; i++) {
        ssd1306_draw_pixel(x, (uint8_t)(y + i), colour);
    }
}

/*
 * Arbitrary line, Bresenham's algorithm.
 *
 * Bresenham draws a line using only integer addition, subtraction and
 * comparison: no floating point, no division, no multiplication. On a
 * Cortex-M4 that matters less than it did historically, but the L433 has no
 * FPU-accelerated divide and this runs in a handful of cycles per pixel.
 *
 * The idea: step along the longer axis one pixel at a time, accumulating the
 * error between the ideal line and the pixel grid. When the accumulated error
 * exceeds half a pixel, step the shorter axis too and subtract it back off.
 *
 * Signed locals are needed for the deltas and error term even though the
 * arguments are unsigned, since dy is negative-going and err swings both ways.
 */
void ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       ssd1306_colour_t colour)
{
    int16_t dx =  (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    int16_t dy = -(int16_t)((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int16_t sx =  (x0 < x1) ? 1 : -1;
    int16_t sy =  (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    int16_t x = (int16_t)x0;
    int16_t y = (int16_t)y0;

    for (;;) {
        ssd1306_draw_pixel((uint8_t)x, (uint8_t)y, colour);

        if (x == (int16_t)x1 && y == (int16_t)y1) {
            break;
        }

        int16_t e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x   += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y   += sy;
        }
    }
}

/* Rectangle outline: four lines. Corners are drawn twice, which is harmless
 * since setting a bit that is already set is a no-op. */
void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       ssd1306_colour_t colour)
{
    if (w == 0 || h == 0) {
        return;
    }

    ssd1306_draw_hline(x, y, w, colour);
    ssd1306_draw_hline(x, (uint8_t)(y + h - 1), w, colour);
    ssd1306_draw_vline(x, y, h, colour);
    ssd1306_draw_vline((uint8_t)(x + w - 1), y, h, colour);
}

/* Solid rectangle. Built from horizontal lines rather than vertical ones
 * because consecutive x values sit in adjacent bytes, so the writes are
 * sequential in memory. */
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       ssd1306_colour_t colour)
{
    for (uint8_t i = 0; i < h; i++) {
        ssd1306_draw_hline(x, (uint8_t)(y + i), w, colour);
    }
}

/*
 * Render one character with its top-left corner at (x, y).
 *
 * Each of the 5 font bytes is a column; each bit within it is a row. Testing
 * bit `row` of column `col` gives the pixel state directly, with no
 * transposition, because the font's column-major layout matches the
 * controller's own.
 *
 * Unsupported characters render as a space rather than indexing off the end
 * of the table.
 */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c, ssd1306_colour_t colour)
{
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
        c = ' ';
    }

    const uint8_t *glyph = ssd1306_font5x7[(uint8_t)c - FONT_FIRST_CHAR];

    for (uint8_t col = 0; col < FONT_WIDTH; col++) {
        for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
            if (glyph[col] & (1u << row)) {
                ssd1306_draw_pixel((uint8_t)(x + col), (uint8_t)(y + row), colour);
            }
        }
    }
}

/*
 * Render a null-terminated string.
 *
 * Only pixels that are set get drawn, so text composites onto whatever is
 * already in the framebuffer rather than clearing a rectangle behind itself.
 * Clear the region first if that is not what you want.
 *
 * Advancing past the right edge is left to draw_pixel's clipping: the glyph is
 * silently dropped rather than wrapping to the next line.
 */
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str,
                         ssd1306_colour_t colour)
{
    while (*str) {
        ssd1306_draw_char(x, y, *str, colour);
        x = (uint8_t)(x + FONT_ADVANCE);
        str++;
    }
}
