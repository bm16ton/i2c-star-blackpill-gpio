

LIBPWM_DIR = ../../libpwm

OBJS += \
	pwmf1.o

vpath %.c $(LIBPWM_DIR)

CPPFLAGS += -I$(LIBPWM_DIR)
