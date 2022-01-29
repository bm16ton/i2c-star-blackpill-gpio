

LIBPWM_DIR = ../../libpwm

OBJS += \
	pwm.o

vpath %.c $(LIBPWM_DIR)

CPPFLAGS += -I$(LIBPWM_DIR)
