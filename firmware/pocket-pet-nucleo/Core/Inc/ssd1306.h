#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

/* Panel geometry. 128x64 = 8 pages of 128 bytes = 1024-byte framebuffer. */
#define SSD1306_WIDTH            128
#define SSD1306_HEIGHT           64
#define SSD1306_PAGES            (SSD1306_HEIGHT / 8)
#define SSD1306_BUFFER_SIZE      (SSD1306_WIDTH * SSD1306_PAGES)

/* --- Fundamental commands (datasheet section 9.1.1) --------------------- */
#define SSD1306_SET_CONTRAST         0x81  /* + one data byte, 0x00-0xFF     */
#define SSD1306_DISPLAY_RAM          0xA4  /* show framebuffer contents      */
#define SSD1306_DISPLAY_ALL_ON       0xA5  /* all pixels on, ignores RAM     */
#define SSD1306_DISPLAY_NORMAL       0xA6
#define SSD1306_DISPLAY_INVERT       0xA7
#define SSD1306_DISPLAY_OFF          0xAE
#define SSD1306_DISPLAY_ON           0xAF

/* --- Addressing (9.1.2) -------------------------------------------------- */
#define SSD1306_SET_MEM_ADDR_MODE    0x20  /* + 0x00 horiz, 0x01 vert, 0x02 page */
#define SSD1306_MEM_ADDR_HORIZONTAL  0x00
#define SSD1306_SET_COLUMN_ADDR      0x21  /* + start, + end                 */
#define SSD1306_SET_PAGE_ADDR        0x22  /* + start, + end                 */

/* --- Hardware configuration (9.1.3) -------------------------------------- */
#define SSD1306_SET_START_LINE       0x40  /* 0x40-0x7F, line 0-63           */
#define SSD1306_SET_SEGMENT_REMAP    0xA1  /* 0xA0 normal, 0xA1 mirrored     */
#define SSD1306_SET_MUX_RATIO        0xA8  /* + one data byte, height-1      */
#define SSD1306_SET_COM_SCAN_DEC     0xC8  /* 0xC0 normal, 0xC8 flipped      */
#define SSD1306_SET_DISPLAY_OFFSET   0xD3  /* + one data byte                */
#define SSD1306_SET_COM_PINS         0xDA  /* + 0x12 for 128x64              */

/* --- Timing and driving (9.1.4) ------------------------------------------ */
#define SSD1306_SET_CLOCK_DIV        0xD5  /* + divide ratio / osc freq      */
#define SSD1306_SET_PRECHARGE        0xD9  /* + phase 1/2 periods            */
#define SSD1306_SET_VCOM_DESELECT    0xDB  /* + deselect level               */

/* --- Charge pump (application note) -------------------------------------- */
#define SSD1306_SET_CHARGE_PUMP      0x8D  /* + 0x14 enable, 0x10 disable    */
#define SSD1306_CHARGE_PUMP_ON       0x14

/* --- Scrolling ----------------------------------------------------------- */
#define SSD1306_DEACTIVATE_SCROLL    0x2E

/* --- Transport ----------------------------------------------------------- */
void ssd1306_reset(void);
void ssd1306_write_cmd(uint8_t cmd);
void ssd1306_write_data(uint8_t *data, uint16_t size);

/* --- Initialisation ------------------------------------------------------ */
void ssd1306_init(void);

/* --- Framebuffer --------------------------------------------------------- */
void ssd1306_fill(uint8_t pattern);
void ssd1306_clear(void);
void ssd1306_update_screen(void);

/* --- Drawing primitives -------------------------------------------------- */
typedef enum {
    SSD1306_PIXEL_OFF = 0,
    SSD1306_PIXEL_ON  = 1
} ssd1306_colour_t;

void ssd1306_draw_pixel(uint8_t x, uint8_t y, ssd1306_colour_t colour);

void ssd1306_draw_hline(uint8_t x, uint8_t y, uint8_t w, ssd1306_colour_t colour);
void ssd1306_draw_vline(uint8_t x, uint8_t y, uint8_t h, ssd1306_colour_t colour);
void ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       ssd1306_colour_t colour);
void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       ssd1306_colour_t colour);
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                       ssd1306_colour_t colour);
/* --- Text ---------------------------------------------------------------- */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c, ssd1306_colour_t colour);
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str,
                         ssd1306_colour_t colour);

#endif /* SSD1306_H */
