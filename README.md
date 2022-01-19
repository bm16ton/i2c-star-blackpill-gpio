i2c-star usb-to-i2c fork with additional usb to gpio firmware and kernel driver for stm32f411 blackpill


First night hacking away only 2 gpios pc13 (led) and pc14. Iused pc13 to make testing ez. currently setting high/low an direction in/out all work but reading current gpio state illudes me. once i conquer the usb read ill add irq generated from interrupt endpoint.


I had to patcg i2c-tiny-usb to keep it from attaching to interface 2, ill include the patch in folder. 


tested on kernel 5.16.1



