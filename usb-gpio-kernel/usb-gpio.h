#ifndef COMMON_H_
#define COMMON_H_





typedef struct __attribute__((__packed__)) _gpio_info
{
   uint8_t no;
   uint8_t data;
} gpio_info;

typedef struct __attribute__((__packed__)) _gpiopktheader
{
   uint8_t command;
   gpio_info gpio;
} gpiopktheader;

#endif /* COMMON_H_ */
