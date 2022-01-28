/*
 * This file is part of the i2c-star project.
 *
 * Copyright (C) 2014 Daniel Thompson <daniel@redfelineninja.org.uk>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/usb/usbd.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/dwc/otg_common.h>
#include <libopencm3/usb/dwc/otg_fs.h>
#include <librfn/fibre.h>
#include <librfn/time.h>
#include <librfn/util.h>
#include <librfm3/i2c_ctx.h>

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0xff,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0403,
	.idProduct = 0xc631,
	.bcdDevice = 0x0205,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 0,
	.bNumConfigurations = 1,
};

const struct usb_endpoint_descriptor i2c_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x81,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 4,
	.bInterval = 0x20,
};

const struct usb_interface_descriptor i2c_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = 0,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = &i2c_endpoint,
};

const struct usb_endpoint_descriptor gpio_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 4,
	.bInterval = 0x20,
};

const struct usb_interface_descriptor gpio_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = 0,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 1,

	.endpoint = &gpio_endpoint,
};

const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &i2c_iface,
	}, {
	.num_altsetting = 1,
	.altsetting = &gpio_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0, /* ?automatically calculated? */
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"redfelineninja.org.uk",
	"i2c-stm32f4-usb",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

// gpio
#define USB_CMD_WRITE       0
#define USB_CMD_READ        1
#define USB_CMD_GPIO_OUTPUT 2
#define USB_CMD_GPIO_INPUT  3
#define USB_CMD_GPIO_SET    4
#define USB_CMD_GPIO_GET    5

/* commands from USB, must e.g. match command ids in kernel driver */
#define CMD_ECHO       0
#define CMD_GET_FUNC   1
#define CMD_SET_DELAY  2
#define CMD_GET_STATUS 3

#define CMD_I2C_IO     4
#define CMD_I2C_BEGIN  1  // flag fo I2C_IO
#define CMD_I2C_END    2  // flag fo I2C_IO

/* linux kernel flags */
#define I2C_M_TEN		0x10	/* we have a ten bit chip address */
#define I2C_M_RD		0x01
#define I2C_M_NOSTART		0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800

/* To determine what functionality is present */
#define I2C_FUNC_I2C			0x00000001
#define I2C_FUNC_10BIT_ADDR		0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING	0x00000004 /* I2C_M_{REV_DIR_ADDR,NOSTART,..} */
#define I2C_FUNC_SMBUS_HWPEC_CALC	0x00000008 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_READ_WORD_DATA_PEC  0x00000800 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA_PEC 0x00001000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_PROC_CALL_PEC	0x00002000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL_PEC 0x00004000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0x00008000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_QUICK		0x00010000
#define I2C_FUNC_SMBUS_READ_BYTE	0x00020000
#define I2C_FUNC_SMBUS_WRITE_BYTE	0x00040000
#define I2C_FUNC_SMBUS_READ_BYTE_DATA	0x00080000
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA	0x00100000
#define I2C_FUNC_SMBUS_READ_WORD_DATA	0x00200000
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA	0x00400000
#define I2C_FUNC_SMBUS_PROC_CALL	0x00800000
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0x01000000
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK	0x04000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK	0x08000000 /* w/ 1-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK_2	 0x10000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK_2 0x20000000 /* w/ 2-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA_PEC  0x40000000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC 0x80000000 /* SMBus 2.0 */

#define I2C_FUNC_SMBUS_BYTE I2C_FUNC_SMBUS_READ_BYTE | \
                            I2C_FUNC_SMBUS_WRITE_BYTE
#define I2C_FUNC_SMBUS_BYTE_DATA I2C_FUNC_SMBUS_READ_BYTE_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_BYTE_DATA
#define I2C_FUNC_SMBUS_WORD_DATA I2C_FUNC_SMBUS_READ_WORD_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_WORD_DATA
#define I2C_FUNC_SMBUS_BLOCK_DATA I2C_FUNC_SMBUS_READ_BLOCK_DATA | \
                                  I2C_FUNC_SMBUS_WRITE_BLOCK_DATA
#define I2C_FUNC_SMBUS_I2C_BLOCK I2C_FUNC_SMBUS_READ_I2C_BLOCK | \
                                  I2C_FUNC_SMBUS_WRITE_I2C_BLOCK

#define I2C_FUNC_SMBUS_EMUL I2C_FUNC_SMBUS_QUICK | \
                            I2C_FUNC_SMBUS_BYTE | \
                            I2C_FUNC_SMBUS_BYTE_DATA | \
                            I2C_FUNC_SMBUS_WORD_DATA | \
                            I2C_FUNC_SMBUS_PROC_CALL | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC | \
                            I2C_FUNC_SMBUS_I2C_BLOCK

/* the currently support capability is quite limited */
const unsigned long func = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;


#define STATUS_IDLE	   0
#define STATUS_ADDRESS_ACK 1
#define STATUS_ADDRESS_NACK 2

static uint8_t status = STATUS_IDLE;

uint32_t i2c = I2C1;

/*!
 * \brief Handle I2C I/O request.
 *
 * \todo There is no bus error checking at all...
 */
static int usb_i2c_io(struct usb_setup_data *req, uint8_t *buf, uint16_t *len)
{
	uint32_t reg32 __attribute__((unused));

	/* Interpret the request */
	uint8_t cmd = req->bRequest;
	uint8_t address = req->wIndex;
	uint8_t is_read = req->wValue & I2C_M_RD;
	uint8_t size = req->wLength;

	i2c_ctx_t ctx;

	i2c_ctx_init(&ctx, I2C1);

	/* We can ignore CMD_I2C_BEGIN, the hardware will work out which
	 * type of start condition to generate.
	 */
	PT_CALL(&ctx.leaf, i2c_ctx_start(&ctx));
	if (ctx.err)
		goto err;

	/* Send the address */
	PT_CALL(&ctx.leaf,
		i2c_ctx_sendaddr(&ctx, address, (is_read ? size : 0)));
	if (ctx.err)
		goto err;

	/* Perform the transaction */
	for (int i=0; i<size; i++) {
		PT_CALL(&ctx.leaf, is_read ? i2c_ctx_getdata(&ctx, buf + i)
					    : i2c_ctx_senddata(&ctx, buf[i]));
		if (ctx.err)
			goto err;
	}

	/* Stop the transaction if requested and this is a write transaction
	 * (reads are stopped automatically)
	 */
	if (cmd & CMD_I2C_END && !is_read) {
		PT_CALL(&ctx.leaf, i2c_ctx_stop(&ctx));
		if (ctx.err)
			goto err;
	}

	status = STATUS_ADDRESS_ACK;
	*len = (is_read ? size : 0);
	return USBD_REQ_HANDLED;

err:
	i2c_ctx_reset(&ctx);
	status = STATUS_ADDRESS_NACK;
	*len = 0;
	return USBD_REQ_HANDLED;
}

static void my_delay_1( void )
{
   for (unsigned i = 0; i < 800000; i++)
     {
        __asm__("nop");
     }
}

static void usbgpio_output(int gpio)
{
//	rcc_periph_clock_enable(RCC_GPIOC);
	if (gpio == 1) {
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO13);
    } else {
	if (gpio == 2) {
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO14);
		}
	}
    my_delay_1();
}

static void usbgpio_input(int gpio)
{

	if (gpio == 1) {
	gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO13);
	} else {
	if (gpio == 2) {
	gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO14);
	}
	}
	my_delay_1();
}

/*
uint8_t data[4 * 1024];
uint8_t dataGPIO[1];
uint8_t inData[64];
volatile int total = 0;
static enum usbd_request_return_codes usb_control_gpio_request(
    usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
    uint16_t *len,
    void (**complete)(usbd_device *, struct usb_setup_data *req))
{
   (void)complete;
	(void)usbd_dev;

	int gpio = req->wIndex;
	int val = req->wValue;

	switch (req->bRequest) {
	case USB_CMD_WRITE:
		memcpy(inData, &req->wValue, sizeof(req->wValue));
		*buf = inData;
		*len = sizeof(req->wValue);
		return USBD_REQ_HANDLED;
	case USB_CMD_READ:
		*buf = data;
		*len = sizeof(data);
		return USBD_REQ_HANDLED;
	case USB_CMD_GPIO_OUTPUT:
		if (gpio == 1) {
			gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
			gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO14);
    	} else {
		if (gpio == 2) {
			gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
			gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO15);
			}
		}
		return 0;
	case USB_CMD_GPIO_INPUT:
		if (gpio == 1) {
			gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO14);
		} // else {
		if (gpio == 2) {
			gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO15);
		  }
		// }
		return 0;
	case USB_CMD_GPIO_SET:
//		switch (gpio)
		if (gpio == 1) {
			if (val == 1) {
				gpio_set(GPIOC, GPIO14);
			} else {		 
			if (val == 0) {
			gpio_clear(GPIOC, GPIO14);
		}
	   }
	  } 
	if (gpio == 2) {
			if (val == 1) {
			gpio_set(GPIOC, GPIO15);
		} else {
		if (val == 0) {
			gpio_clear(GPIOC, GPIO15);
		}
	  }
    }
		return 0;
	case USB_CMD_GPIO_GET:
		switch (gpio)
		{
//		case 0: val = IO_IS_HIGH(LED); break;
		case 1: val = gpio_get(GPIOC, GPIO14); break;
		case 2: val = gpio_get(GPIOC, GPIO15); break;
		}
		memcpy(inData, &val, sizeof(val));
		*buf = inData;
		*len = sizeof(val);
		return USBD_REQ_HANDLED;
	
  }
   return 1;
}
*/

static enum usbd_request_return_codes usb_control_gpio_request(
    usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
    uint16_t *len,
    void (**complete)(usbd_device *, struct usb_setup_data *req))
{
    (void)complete;
	(void)usbd_dev;
    int getv;

   if ((req->bmRequestType & 0x7F) != USB_REQ_TYPE_VENDOR)
     return 0;

   (*len) = 1;
   (*buf)[0] = 1; //success

   if (req->wValue == 1)
     {
        if ( req->wIndex == 0 )
			{
				usbgpio_input(1);
			}
	    else if ( req->wIndex == 1 )
			{
				usbgpio_input(2);
			}
      }
   else if (req->wValue == 2)
     {
     if ( req->wIndex == 0 )
			{
				usbgpio_output(1);
			}
	    else if ( req->wIndex == 1 )
			{
				usbgpio_output(2);
			}
      }
   else if (req->wValue == 3)
     {
     if ( req->wIndex == 0 )
			{
			getv = gpio_get(GPIOA, GPIO0);
			if (getv == 0) {
		        (*buf)[0] = 1; 
		        (*buf)[1] = 2;
		        (*buf)[2] = 2;
		        (*buf)[3] = 2;
			    *len = 4;
			} else if (getv == 1) {
				(*buf)[0] = 1; 
		        (*buf)[1] = 3;
		        (*buf)[2] = 3;
		        (*buf)[3] = 3;
			    *len = 4;
			}
			return USBD_REQ_HANDLED;
			}
	    else if ( req->wIndex == 1 )
			{
			getv = gpio_get(GPIOA, GPIO0);
			if (getv == 0) {
		        (*buf)[0] = 1; 
		        (*buf)[1] = 2;
		        (*buf)[2] = 2;
		        (*buf)[3] = 2;
			    *len = 4;
			} else if (getv == 1) {
				(*buf)[0] = 1; 
		        (*buf)[1] = 3;
		        (*buf)[2] = 3;
		        (*buf)[3] = 3;
			    *len = 4;
			}
			return USBD_REQ_HANDLED;
			}
      }
  else if (req->wValue == 0)
     { 
	 if (req->bRequest == 1)
        {
        if ( req->wIndex == 0 )
			{
				gpio_clear(GPIOC, GPIO13);
			}
	    else if ( req->wIndex == 1 )
			{
				gpio_set(GPIOC, GPIO14);
			}
        }
   else if (req->bRequest == 0)
     {
     if (req->wIndex == 0)
			{
				gpio_set(GPIOC, GPIO13);
			}
	    else if ( req->wIndex == 1 )
			{
				gpio_clear(GPIOC, GPIO14);
			}		
	  }
    }
   else
     {
        (*buf)[0] = -1; // FAILURE
     }
 
   return 1;
}



static enum usbd_request_return_codes usb_control_request(
    usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
    uint16_t *len,
    void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	static uint8_t reply_buf[64];

	(void)usbd_dev;
	(void)complete;

	switch (req->bRequest) {
	case CMD_ECHO:
		memcpy(reply_buf, &req->wValue, sizeof(req->wValue));
		*buf = reply_buf;
		*len = sizeof(req->wValue);
		return USBD_REQ_HANDLED;

	case CMD_GET_FUNC:
		/* Report our capabilities */
		memcpy(reply_buf, &func, sizeof(func));
		*buf = reply_buf;
		*len = sizeof(func);
		return USBD_REQ_HANDLED;

	case CMD_SET_DELAY:
		/* This was used in i2c-tiny-usb to choose the clock
		 * frequency by specifying the shortest time between
		 * clock edges.
		 *
		 * This implementation silently ignores delay requests. We
		 * run the hardware as fast as we are permitted.
		 */
		*buf = reply_buf;
		*len = 0;
		return USBD_REQ_HANDLED;

	case CMD_I2C_IO:
	case CMD_I2C_IO | CMD_I2C_BEGIN:
	case CMD_I2C_IO | CMD_I2C_END:
	case CMD_I2C_IO | CMD_I2C_BEGIN | CMD_I2C_END:
		if (req->wValue & I2C_M_RD)
			*buf = reply_buf;
		return usb_i2c_io(req, *buf, len);
		break;

	case CMD_GET_STATUS:
		memcpy(reply_buf, &status, sizeof(status));
		*buf = reply_buf;
		*len = sizeof(status);
		return USBD_REQ_HANDLED;

	default:
		break;

	}

	return USBD_REQ_NEXT_CALLBACK;
}

static void usb_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 4, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				usb_control_request);

	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 9, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_VENDOR,
				USB_REQ_TYPE_TYPE,
				usb_control_gpio_request);
}

static usbd_device *usbd_dev;

static int usb_fibre(fibre_t *fibre)
{
	PT_BEGIN_FIBRE(fibre);

	rcc_periph_clock_enable(RCC_OTGFS);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
			GPIO9 | GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF10, GPIO9 | GPIO11 | GPIO12);

	usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config,
			usb_strings, 2,
			usbd_control_buffer, sizeof(usbd_control_buffer));
	OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS;
	usbd_register_set_config_callback(usbd_dev, usb_set_config);

	while (true) {
		usbd_poll(usbd_dev);
		PT_YIELD();
	}

	PT_END();
}
static fibre_t usb_task = FIBRE_VAR_INIT(usb_fibre);

static void i2c_init(void)
{
	/* clocks */
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_I2C1);

	/* initialize the peripheral */
	i2c_ctx_t ctx;
	i2c_ctx_init(&ctx, I2C1);
	i2c_ctx_reset(&ctx);

	/* GPIO for I2C1 */
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO6 | GPIO9);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_100MHZ,
				GPIO6 | GPIO9);
	gpio_set_af(GPIOB, GPIO_AF4, GPIO6 | GPIO9);

	/* take the DAC out of reset (so there is something in the bus) */
//	rcc_periph_clock_enable(RCC_GPIOD);
//	gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4);
//	gpio_set(GPIOD, GPIO4);
}



static void gpio_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO13);

	gpio_clear(GPIOC, GPIO13);

	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO14);

	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
							GPIO15);
							
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);
	
    my_delay_1();
	gpio_set(GPIOC, GPIO13);
	gpio_set(GPIOC, GPIO14);
	gpio_set(GPIOC, GPIO15);
//	gpio_set_af(GPIOA, GPIO_AF1, GPIO10);
}

int main(void)
{
	int i;

    rcc_clock_setup_pll(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_84MHZ]);

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	
	gpio_init();
	i2c_init();
	time_init();
//	gpio_clear(GPIOC, GPIO13);

	for (i = 0; i < 0x800000; i++)
		__asm__("nop");

	/* prepare the scheduler */
	fibre_run(&usb_task);

	fibre_scheduler_main_loop();
	return 0;
}
