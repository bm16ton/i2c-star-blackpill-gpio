
#ifndef UART_STDIO_H_ITU 
#define UART_STDIO_H_ITU

#define BUFFER_SIZE 256

extern buffer_t stdin_buffer;
extern buffer_t stdout_buffer;

void io_setup(void);

#endif
