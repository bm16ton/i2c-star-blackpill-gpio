i2c-star usb-to-i2c fork with additional usb to gpio firmware and kernel driver for stm32f411 blackpill


4 working gpio PC13 LED, PC14, PC15, and PA0 BTN. (pin numbers subject to change wo notice)

gpio read now works fine (with realy good usb cable, usb cables apparently screw up the gpio state reading) Setting output/input wrks perfect on all 4 pins but obviously the BTN-PA0 should be kept input and LED-C13 kept output. Ill mask their directions eventually.

Basic PWM now implimented on blackpill but I just realized im using the old PWM FOPS "configure" instead of "apply" etc. Also neither seem to supply an offset for the gpio pin number so currently dunno how to tell the usb device which pin to configure! ill get it eventually. 

IRQ basic IRQ stuff has been added to driver and blackpill 411 but not everything and most of what added needs to be edited/changed. My next step will be to add a pin/button and have it be irq enabled and do something stupid, hen have it send an interrupt urb that the driver turns into irq via driver

Finally ADC pins I havent started at all!

I had to patch i2c-tiny-usb to keep it from attaching to interface 2, ill include the patch in folder. 


tested on kernel 5.16.1 - 5.17-rc3



