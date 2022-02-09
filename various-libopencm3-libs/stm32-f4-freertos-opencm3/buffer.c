
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <buffer.h>

void buffer_init(buffer_t * buffer, uint8_t * data, uint16_t length) {
    if (buffer && buffer) {
        memset((void **)buffer, 0, length);
        buffer->length = length;
        buffer->data = data;
        buffer->head = 0;
        buffer->tail = 0;
    }
}

uint16_t buffer_count(const buffer_t * buffer) {
    if (buffer) {
        return (buffer->head - buffer->tail);
    }
    return 0;
}

bool buffer_full(const buffer_t * buffer) {
    if (buffer) {
        return (buffer_count(buffer) == buffer->length);
    }
    return true;
}

bool buffer_empty(const buffer_t * buffer) {
    if (buffer) {
        return (buffer_count(buffer) == 0);
    }
    return true;
}

uint16_t buffer_peek(const buffer_t * buffer) {
    uint8_t data = 0;

    if (!buffer_empty(buffer)) {
        data = buffer->data[buffer->tail % buffer->length];
    }
    return data;
}

bool buffer_back(buffer_t * buffer) {
    if (!buffer_empty(buffer)) {
        buffer->head--;
        return true;
    }
    return false;
}

uint8_t buffer_get_byte(buffer_t * buffer) {
    uint8_t data = 0;

    if (!buffer_empty(buffer)) {
        data = buffer->data[buffer->tail % buffer->length];
        buffer->tail++;
    }
    return data;
}

bool buffer_put_byte(buffer_t * buffer, uint8_t data) {
    bool status = false;

    if (buffer) {
        if (!buffer_full(buffer)) {
            buffer->data[buffer->head % buffer->length] = data;
            buffer->head++;
            status = true;
        }
    }
    return status;
}

uint8_t buffer_put_string(buffer_t * buffer, uint8_t * string) {
    if (buffer) {
        for (uint8_t i = 0; i < strlen(string); i++) {
            if (!buffer_put_byte(buffer, string[i]))
                return i;
        }
    }
    return 0;
}

bool buffer_scan_byte(buffer_t * buffer, uint8_t c) {
    if (buffer) {
        if (!buffer_empty(buffer)) {
            uint8_t tail = buffer->tail;

            for (uint8_t i = 0; i < buffer_count(buffer); i++) {
                uint8_t data = buffer->data[tail % buffer->length];

                if (data == c) {
                    return true;
                }
                tail++;
            }
        }
        return false;
    }
    return false;
}

uint16_t buffer_get_token(buffer_t * buffer, uint8_t * str, uint16_t len, uint8_t term) {
    if (buffer) {
        memset((void *)str, 0, len);

        if (buffer_scan_byte(buffer, term) && str) {
            uint8_t i = 0, c = 0;

            while ((c = buffer_get_byte(buffer)) != 0 && c != term && i < len) {
                str[i] = c;
                i++;
            }
            return i;
        }
        return 0;
    }
    return 0;
}

/* EOF */
