
/* $Id$ */

#include <stdlib.h>
#include <stdint.h>

#include <ili9341.h>

#include <FreeRTOS.h>
#include <task.h>


#define LCD_COMM   *(volatile uint16_t *)(0x60000000)
#define LCD_DATA   *(volatile uint16_t *)(0x60080000)


inline void _delay_ms(uint32_t n) {
    for (volatile int i = 0; i < n * 80000; i++)
        __asm__("nop");
}

inline void delay_ms(uint32_t n) {
    for (volatile int i = 0; i < n * 80000; i++)
        __asm__("nop");
}

inline void lcd_write_command(uint8_t comm) {
    taskENTER_CRITICAL();
    LCD_COMM = (uint16_t) (comm & 0xFF);
    taskEXIT_CRITICAL();
}

inline void lcd_write_data(uint16_t data) {
    LCD_DATA = data;
}

inline void lcd_write_byte(uint8_t data) {
    taskENTER_CRITICAL();
    LCD_DATA = (uint16_t) (data && 0xFF);
    taskEXIT_CRITICAL();
}

inline inline void lcd_write_word(uint16_t w) {
    lcd_write_data((w >> 8) & 0xFF);
    lcd_write_data(w & 0xFF);
}


void lcd_soft_reset(void) {
    lcd_write_command(LCD_SWRESET);
}

void lcd_sleep_out(void) {
    lcd_write_command(LCD_SLPOUT);
}

void lcd_set_color_mode(uint8_t mode) {
    lcd_write_command(LCD_COLMOD);
    lcd_write_byte(mode);
}
void lcd_set_normal_mode(void) {
    lcd_write_command(LCD_NORON);
}

void lcd_set_display_on(void) {
    lcd_write_command(LCD_DISPON);
}

void lcd_set_display_off(void) {
    lcd_write_command(LCD_DISPOFF);
}

void lcd_set_brightness(uint8_t level) {
    lcd_write_command(LCD_WRDISBV);
    lcd_write_byte(level);
}

void lcd_set_frame_rate_normal(uint8_t divb, uint8_t rtnb) {
    lcd_write_command(LCD_FRMCTR1);
    lcd_write_byte(divb);
    lcd_write_byte(rtnb);
}

void lcd_set_frame_rate_idle(uint8_t divb, uint8_t rtnb) {
    lcd_write_command(LCD_FRMCTR2);
    lcd_write_byte(divb);
    lcd_write_byte(rtnb);
}

void lcd_set_frame_rate_partial(uint8_t divb, uint8_t rtnb) {
    lcd_write_command(LCD_FRMCTR3);
    lcd_write_byte(divb);
    lcd_write_byte(rtnb);
}


void lcd_set_memory_access(uint8_t mode) {
    lcd_write_command(LCD_MADCTL);
    lcd_write_byte(mode);
}

void lcd_setup(void) {

    lcd_soft_reset();
    _delay_ms(150);

    lcd_sleep_out();
    _delay_ms(250);

    lcd_write_command(LCD_PWCTR1);
    lcd_write_data(0x1B);

    lcd_write_command(LCD_PWCTR2);
    lcd_write_data(0x01);

    lcd_write_command(LCD_VMCTR1);
    lcd_write_data(0x30);
    lcd_write_data(0x30);

    lcd_write_command(LCD_VMOFCTR);
    lcd_write_data(0XB7);


    lcd_write_command(LCD_COLMOD);
    lcd_write_data(0x55);

    lcd_write_command(LCD_DISSET5);
    lcd_write_data(0x0A);
    lcd_write_data(0xA2);


    lcd_write_command(LCD_GAMSET);
    lcd_write_data(0x01);

    lcd_write_command(LCD_GMCTRP1);
    lcd_write_data(0x0F);
    lcd_write_data(0x2A);
    lcd_write_data(0x28);
    lcd_write_data(0x08);
    lcd_write_data(0x0E);
    lcd_write_data(0x08);
    lcd_write_data(0x54);
    lcd_write_data(0XA9);
    lcd_write_data(0x43);
    lcd_write_data(0x0A);
    lcd_write_data(0x0F);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);

    lcd_write_command(LCD_GMCTRN1);
    lcd_write_data(0x00);
    lcd_write_data(0x15);
    lcd_write_data(0x17);
    lcd_write_data(0x07);
    lcd_write_data(0x11);
    lcd_write_data(0x06);
    lcd_write_data(0x2B);
    lcd_write_data(0x56);
    lcd_write_data(0x3C);
    lcd_write_data(0x05);
    lcd_write_data(0x10);
    lcd_write_data(0x0F);
    lcd_write_data(0x3F);
    lcd_write_data(0x3F);
    lcd_write_data(0x0F);

    lcd_write_command(LCD_RASET);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x01);
    lcd_write_data(0x3f);

    lcd_write_command(LCD_CASET);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0xef);

    lcd_write_command(LCD_IDMOFF);

    lcd_set_normal_mode();

    lcd_write_command(LCD_COLMOD);
    lcd_write_data(0x55);

    lcd_write_command(LCD_FRMCTR1);
    lcd_write_data(0x00);
    lcd_write_data(0x18);

    lcd_sleep_out();
    delay_ms(120);
    lcd_set_display_on();

    _delay_ms(100);

}

void lcd_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_command(LCD_CASET);
    lcd_write_word(x0);
    lcd_write_word(x1);

    lcd_write_command(LCD_RASET);
    lcd_write_word(y0);
    lcd_write_word(y1);
}

void lcd_write_ram(uint16_t w, uint16_t n) {
    lcd_write_command(LCD_RAMWR);
    while (n--)
        lcd_write_data(w);
}

void lcd_write_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    lcd_addr_window(x, y, (x + w), (y + h));
    uint16_t c565 = lcd_rgb2c(color);
    lcd_write_ram(c565, (w + 1) * (h + 1 ) );
}

void lcd_clear(void) {
    lcd_write_rect(0, 0, LCD_WIDTH/2, LCD_HEIGHT, LCD_BLACK);
    lcd_write_rect(LCD_WIDTH/2, 0, LCD_WIDTH/2 + 1, LCD_HEIGHT, LCD_BLACK);
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
    lcd_addr_window(x, y, x, y);
    uint16_t c565 = lcd_rgb2c(color);
    lcd_write_command(LCD_RAMWR);
    lcd_write_data(c565);
}

#define swap(a, b) { int16_t t = a; a = b; b = t; }

void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);

    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    while (x0 <= x1) {
        if (steep) {
            lcd_draw_pixel(y0, x0, color);
        } else {
            lcd_draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
        x0++;
    }
}

void lcd_draw_vline(int16_t x, int16_t y, int16_t l, uint16_t color) {
    lcd_write_rect(x, y, (l - 1), 0, color);
}

void lcd_draw_hline(int16_t x, int16_t y, int16_t l, uint16_t color) {
    lcd_write_rect(x, y, 0, (l - 1), color);
}

void lcd_draw_rest(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint32_t color) {
    lcd_draw_hline(x1, y1, w, color);
    lcd_draw_vline(x1, y1, h, color);
    lcd_draw_hline((x1 + h) - 0, y1, w, color);
    lcd_draw_vline(x1, (y1 + w) - 0, h, color);
}

uint16_t lcd_rgb2c(uint32_t rgb) {
    return (uint16_t) 
        (((rgb & 0xF80000) >> 19)|
         ((rgb & 0x00F800) >> 6) |
         ((rgb & 0x0000F8) << 8));
}

void lcd_draw_char(uint16_t xbase, uint16_t ybase, font_t *font, uint8_t c) {
    if (c < font->start || c > (font->start + font->length))
        c = ' ';
    c = c - font->start;

    uint16_t fg = lcd_rgb2c(LCD_WHITE);
    uint16_t bg = lcd_rgb2c(LCD_BLACK);

    lcd_addr_window(xbase, ybase, xbase + font->height - 1, ybase + font->width - 1);

    lcd_write_command(LCD_RAMWR);

    for (uint8_t w = font->width; w > 0; w--) {
        for (uint8_t h = font->height; h > 0; h--) {
            if ((font->bitmap[(c) * font->height + (h - 1)]) & (1 << (w - 1)))
                lcd_write_data(0xEFEF);
            else
                lcd_write_data(0x0000);
        }
    }
}


/* EOF */
