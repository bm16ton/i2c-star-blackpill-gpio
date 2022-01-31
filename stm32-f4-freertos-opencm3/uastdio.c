
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <wctype.h>
#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


#include <syscall.h>
#include <buffer.h>

#include <uastdio.h>

#define BUFFER_SIZE 256

buffer_t stdin_buffer;
buffer_t stdout_buffer;

static uint8_t stdin_buffer_data[BUFFER_SIZE];
static uint8_t stdout_buffer_data[BUFFER_SIZE];

void io_setup(void) {
    buffer_init(&stdin_buffer, stdin_buffer_data, sizeof(stdin_buffer_data));
    buffer_init(&stdout_buffer, stdout_buffer_data, sizeof(stdout_buffer_data));
}

/* EOF */

