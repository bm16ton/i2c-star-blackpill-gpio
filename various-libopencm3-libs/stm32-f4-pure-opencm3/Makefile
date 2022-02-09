#
# Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018
#
.SECONDARY:

CFLAGS+= -I. -Os  -DSTM32F4 -std=c99
CFLAGS+= -mthumb -march=armv7e-m
CFLAGS+= -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS+= -mcpu=cortex-m4
CFLAGS+= -fno-common -ffunction-sections -fdata-sections
CFLAGS+= -g -gdwarf-2
#CFLAGS+= -save-temps


LDFLAGS+= ${CFLAGS}
LDFLAGS+= --static
#LDFLAGS+= -nostartfiles
LDFLAGS+= -T master.ld

LDFLAGS+= -Wl,-Map=master.map
LDFLAGS+= -Wl,--cref -Wl,--gc-sections
LDFLAGS+= -lopencm3_stm32f4
#LDFLAGS+= -lc -lgcc
LDFLAGS+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

all: master.bin

MASTER_OBJS+= master.o 
MASTER_OBJS+= syscall.o
MASTER_OBJS+= buffer.o
MASTER_OBJS+= uastdio.o

MASTER_OBJS+= fsmcwr.o
MASTER_OBJS+= ili9341.o
MASTER_OBJS+= console.o
MASTER_OBJS+= random.o
MASTER_OBJS+= rtc4xx.o
MASTER_OBJS+= xpt2046.o


master.elf: $(MASTER_OBJS)
	arm-eabi-gcc $(^F) $(LDFLAGS) -o $@ 
	arm-eabi-size --format=berkeley $@

%.o: %.c
	arm-eabi-gcc $(CFLAGS) -c -o $@ $<

%.o: %.S
	arm-eabi-as $(ASFLAGS) -o $@ $<

%.bin: %.elf
	arm-eabi-objcopy -O binary $< $@

%.elf: %.o
	arm-eabi-gcc $(^F) $(LDFLAGS) -o $@ 
	arm-eabi-size --format=berkeley $@

clean:
	rm -f *.i *.o *.elf *.bin *.map *~ *.hex *.d *.s

upload: master.upl

%.upl: %.bin
	@openocd \
	    -c 'puts "--- START --------------------"' \
	    -f 'interface/stlink-v2.cfg' \
	    -f 'target/stm32f4x.cfg'  \
	    -c 'puts "--- INIT --------------------"' \
	    -c "init" \
	    -c "reset halt" \
	    -c 'puts "--- WRITE --------------------"' \
	    -c "flash write_image erase $< 0x08000000"\
	    -c 'puts "--- VERIFY --------------------"' \
	    -c "verify_image $<" \
	    -c 'puts "--- RESET --------------------"' \
	    -c "reset" \
	    -c 'puts "--- DONE --------------------"' \
	    -c "shutdown"



debug:
	@openocd \
	    -c 'puts "--- START --------------------"' \
	    -f 'interface/stlink-v2.cfg' \
	    -f 'target/stm32f4x.cfg'  \
	    -c 'puts "--- INIT --------------------"' \
	    -c "init" \
	    -c "halt" \
	    -c "poll"


#EOF
