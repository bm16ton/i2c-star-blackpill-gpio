i2c-star usb-to-i2c fork with additional usb to gpio firmware and kernel driver for stm32f411 blackpill

fixed system freezing on removal by removing the unused pwm part

4 working gpio 

gpio read now works fine (with realy good usb cable, usb cables apparently screw up the gpio state reading) Setting output/input wrks perfect on all 4 pins but obviously the BTN-PA0 should be kept input and LED-C13 kept output. Ill mask their directions eventually.

Currently IRQ support works! one pin only, all edges supported. Need to check why using libgpio can set the initial edge value and work but the one driver ive tried mcp251x needs me to export the gpio set the edge ad unexport it before it will work and fire irqs. Probly having irq enabled by default on pin will fix this.

Finally ADC pins I havent started at all! (see kitched sink repo for working usbadc)

I had to patch i2c-tiny-usb to keep it from attaching to interface 2, ill include the patch in folder. 


tested on kernel 5.16.1 - 5.17-rc3  thru 6.0.0

This is an old project ill try and keep up with from time to time, the newer project with all this and more is in the kitchensink repo but requires a toucscreen and possibly a adafruit seesaw. 



