/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#ifndef BUFFER_H_ITU
#define BUFFER_H_ITU

typedef struct buffer {
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint8_t *data;
    uint16_t length;
} buffer_t;

bool buffer_back(buffer_t *buffer);
bool buffer_empty(const buffer_t *buffer);
bool buffer_full(const buffer_t *buffer);
bool buffer_put_byte(buffer_t *buffer, uint8_t data);
bool buffer_scan_byte(buffer_t *buffer, uint8_t c);

uint16_t buffer_count(const buffer_t *buffer);
uint16_t buffer_get_token(buffer_t *buffer, uint8_t * str, uint16_t len, uint8_t term);
uint16_t buffer_peek(const buffer_t *buffer);
uint8_t buffer_get_byte(buffer_t *buffer);
uint8_t buffer_put_string(buffer_t *buffer, uint8_t * string);

void buffer_init(buffer_t *buffer, uint8_t * data, uint16_t length);

#endif
/* EOF */
