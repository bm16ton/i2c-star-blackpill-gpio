# CANopencm3
CanOpen 4.0 driver for libopencm3 with optional FreeRTOS support.

* Full package to get you started with CANOpen on stm32 devices. 
* Uses libopencm3 library for accessing hardware, which is very lightweight.
* Has freertos set up properly

## What's inside?

### CANopen driver 
  * Uses libopencm3
  * Can TX/RX are driven by interrupts
  * No buffering or queuing besides can mailboxes/fifo
  * Todo: Hardware filters

### Flash storage
  * Wear-levelling mechanism (append-only)
    * Only erases pages when allocated space is Full
    * Defragmentation skips unchanged pages at the start
  * Can support variable-length storage entries
  * Not limited to CANOpen structrs
  * Uses no buffering, tiny ram consumption
  * Todo: Implement crc check

### FreeRTOS main.c 
  * Uses Control, Mainline & Processing tasks
  * Driven by CANOpen callbacks, does not do polling
  * Can use tickless mode to save on power

### Regular main_blank.c that uses libopencm3
  * Uses flash storage
  * Traditional main_blank.c structure
  * Interrupt-driven



