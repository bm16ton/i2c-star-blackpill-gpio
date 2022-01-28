i2c-star usb-to-i2c fork with additional usb to gpio firmware and kernel driver for stm32f411 blackpill


4 working gpio PC13 LED, PC14, PC15, and PA0 BTN.

PA0 reads fine (with realy good usb cable, usb ables apparently screw up the gpio state reading) the others have been tested without Pull UPs/DOWNs and didnt go great. Setting output/input wrks perect on all 4 pins but ovbiously the BTN-PA0 should be kept input and LED-C13 kept output. Ill mask their directions eventually.


I had to patch i2c-tiny-usb to keep it from attaching to interface 2, ill include the patch in folder. 


tested on kernel 5.16.1



