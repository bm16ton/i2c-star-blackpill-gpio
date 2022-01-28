// SPDX-License-Identifier: GPL-2.0-only
/*
 * driver for the i2c-tiny-usb adapter - 1.0
 * http://www.harbaum.org/till/i2c_tiny_usb
 *
 * Copyright (C) 2006-2007 Till Harbaum (Till@Harbaum.org)
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>

/* include interfaces to usb layer */
#include <linux/usb.h>

/* include interface to i2c layer */
#include <linux/i2c.h>

#include <linux/workqueue.h> //for work_struct
#include <linux/gpio.h> //for led
#include <linux/device.h>// Header to support the kernel Driver Model
#include <linux/gpio/driver.h>
/* commands via USB, must match command ids in the firmware */
#define USB_CMD_WRITE       12
#define USB_CMD_READ        11
#define USB_CMD_GPIO_OUTPUT 8
#define USB_CMD_GPIO_INPUT  13
#define USB_CMD_GPIO_SET    10
#define USB_CMD_GPIO_GET    9

#define CMD_ECHO		0
#define CMD_GET_FUNC		1
#define CMD_SET_DELAY		2
#define CMD_GET_STATUS		3

#define CMD_I2C_IO		4
#define CMD_I2C_IO_BEGIN	(1<<0)
#define CMD_I2C_IO_END		(1<<1)

/* i2c bit delay, default is 10us -> 100kHz max
   (in practice, due to additional delays in the i2c bitbanging
   code this results in a i2c clock of about 50kHz) */
   
static uint8_t gpio_val = 0;
static uint8_t offs = 0;
static uint8_t usbval = 0;
   
static unsigned short delay = 10;
module_param(delay, ushort, 0);
MODULE_PARM_DESC(delay, "bit delay in microseconds "
		 "(default is 10us for 100kHz max)");

static int usb_read(struct i2c_adapter *adapter, int cmd,
		    int value, int index, void *data, int len);

static int usb_write(struct i2c_adapter *adapter, int cmd,
		     int value, int index, void *data, int len);

/* ----- begin of i2c layer ---------------------------------------------- */

#define STATUS_IDLE		0
#define STATUS_ADDRESS_ACK	1
#define STATUS_ADDRESS_NAK	2


struct i2c_tiny_usb {
	struct usb_device *usb_dev; /* the usb device for this device */
	struct usb_interface *interface; /* the interface for this device */
	struct i2c_adapter adapter; /* i2c related things */
	struct gpio_chip chip; //this is our GPIO chip
//	unsigned char open_gpios;
//	unsigned int bitmode;
	unsigned char gpio_direction; /* data direction in bitbang */
	unsigned char gpio_values; /* GPIO output values */

     struct work_struct work;
     struct work_struct work2;
     
     u8 buf[3];
};

static void
_gpio_work_job(struct work_struct *work)
{
   struct i2c_tiny_usb *sd = container_of(work, struct i2c_tiny_usb, work);

   printk(KERN_ALERT "gpioval i/o: %d", gpio_val);
   printk(KERN_ALERT "usbval i/o: %d", usbval);
   printk(KERN_ALERT "offset i/o: %d",offs);
   usb_control_msg(sd->usb_dev,
                   usb_sndctrlpipe(sd->usb_dev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_OUT,
                   usbval, offs,
                   NULL, 0,
                   1000);
}

static void
_gpio_work_job2(struct work_struct *work2)
{
   struct i2c_tiny_usb *sd = container_of(work2, struct i2c_tiny_usb, work2);

   printk(KERN_ALERT "Read port i/o: %d", offs);
   usb_control_msg(sd->usb_dev,
                   usb_rcvctrlpipe(sd->usb_dev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_IN,
                   usbval, offs,
                   (u8 *)sd->buf, 3,
                   1000);
}

static int usb_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int num)
{
	unsigned char *pstatus;
	struct i2c_msg *pmsg;
	int i, ret;

	dev_dbg(&adapter->dev, "master xfer %d messages:\n", num);

	pstatus = kmalloc(sizeof(*pstatus), GFP_KERNEL);
	if (!pstatus)
		return -ENOMEM;

	for (i = 0 ; i < num ; i++) {
		int cmd = CMD_I2C_IO;

		if (i == 0)
			cmd |= CMD_I2C_IO_BEGIN;

		if (i == num-1)
			cmd |= CMD_I2C_IO_END;

		pmsg = &msgs[i];

		dev_dbg(&adapter->dev,
			"  %d: %s (flags %d) %d bytes to 0x%02x\n",
			i, pmsg->flags & I2C_M_RD ? "read" : "write",
			pmsg->flags, pmsg->len, pmsg->addr);

		/* and directly send the message */
		if (pmsg->flags & I2C_M_RD) {
			/* read data */
			if (usb_read(adapter, cmd,
				     pmsg->flags, pmsg->addr,
				     pmsg->buf, pmsg->len) != pmsg->len) {
				dev_err(&adapter->dev,
					"failure reading data\n");
				ret = -EIO;
				goto out;
			}
		} else {
			/* write data */
			if (usb_write(adapter, cmd,
				      pmsg->flags, pmsg->addr,
				      pmsg->buf, pmsg->len) != pmsg->len) {
				dev_err(&adapter->dev,
					"failure writing data\n");
				ret = -EIO;
				goto out;
			}
		}

		/* read status */
		if (usb_read(adapter, CMD_GET_STATUS, 0, 0, pstatus, 1) != 1) {
			dev_err(&adapter->dev, "failure reading status\n");
			ret = -EIO;
			goto out;
		}

		dev_dbg(&adapter->dev, "  status = %d\n", *pstatus);
		if (*pstatus == STATUS_ADDRESS_NAK) {
			ret = -ENXIO;
			goto out;
		}
	}

	ret = i;
out:
	kfree(pstatus);
	return ret;
}

static u32 usb_func(struct i2c_adapter *adapter)
{
	__le32 *pfunc;
	u32 ret;

	pfunc = kmalloc(sizeof(*pfunc), GFP_KERNEL);

	/* get functionality from adapter */
	if (!pfunc || usb_read(adapter, CMD_GET_FUNC, 0, 0, pfunc,
			       sizeof(*pfunc)) != sizeof(*pfunc)) {
		dev_err(&adapter->dev, "failure reading functionality\n");
		ret = 0;
		goto out;
	}

	ret = le32_to_cpup(pfunc);
out:
	kfree(pfunc);
	return ret;
}

static void
_gpioa_set(struct gpio_chip *chip,
           unsigned offset, int value)
{
   struct i2c_tiny_usb *data = container_of(chip, struct i2c_tiny_usb,
                                  chip);
   printk(KERN_INFO "GPIO SET INFO for pin: %d", offset);

   usbval = 10;

   if (offset == 0)
     {
		offs = offset;
        gpio_val = value;
        schedule_work(&data->work);
     }
   if (offset == 1)
     {
		offs = offset;
        gpio_val = value;
        schedule_work(&data->work);
     }
}

static int
_gpioa_get(struct gpio_chip *chip,
           unsigned offset)
{
    struct i2c_tiny_usb *data = container_of(chip, struct i2c_tiny_usb,
                                  chip);
//   struct usb_device *udev = interface_to_usbdev(data->interface);
   int ret = -1;
   printk(KERN_INFO "GPIO GET INFO: %d", offset);


	offs = offset;
    gpio_val = 1;
//	schedule_work(&data->work);
    usbval = 9;
//    ret = schedule_work(&data->work2);
    //ret = gpio_val;

	schedule_work(&data->work);
    usbval = 9;
    ret = schedule_work(&data->work2);
    return ret;
}

static int
_direction_output(struct gpio_chip *chip,
                  unsigned offset, int value)
{
    struct i2c_tiny_usb *data = container_of(chip, struct i2c_tiny_usb,
                                  chip);
   printk("Setting pin to OUTPUT");
   if (offset == 0)
     {
        usbval = USB_CMD_GPIO_OUTPUT;
		offs = offset;
//        gpio_val = value;
        schedule_work(&data->work);
     }
   if (offset == 1)
     {
        usbval = USB_CMD_GPIO_OUTPUT;
		offs = offset;
 //       gpio_val = value;
        schedule_work(&data->work);
     }
   return 0;
}

static int
_direction_input(struct gpio_chip *chip,
                  unsigned offset)
{
    struct i2c_tiny_usb *data = container_of(chip, struct i2c_tiny_usb,
                                  chip);
                                      
   printk("Setting pin to INPUT");
   if (offset == 0)
     {
        usbval = USB_CMD_GPIO_INPUT;
		offs = offset;
//        gpio_val = value;
        schedule_work(&data->work);
     }
   if (offset == 1)
     {
        usbval = USB_CMD_GPIO_INPUT;
		offs = offset;
 //       gpio_val = value;
        schedule_work(&data->work);
     }
   return 0;
}


/* This is the actual algorithm we define */
static const struct i2c_algorithm usb_algorithm = {
	.master_xfer	= usb_xfer,
	.functionality	= usb_func,
};

/* ----- end of i2c layer ------------------------------------------------ */

/* ----- begin of usb layer ---------------------------------------------- */

/*
 * Initially the usb i2c interface uses a vid/pid pair donated by
 * Future Technology Devices International Ltd., later a pair was
 * bought from EZPrototypes
 */
static const struct usb_device_id i2c_tiny_usb_table[] = {
	{ USB_DEVICE(0x0403, 0xc631) },   /* FTDI */
	{ USB_DEVICE(0x1c40, 0x0534) },   /* EZPrototypes */
	{ }                               /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, i2c_tiny_usb_table);

/* Structure to hold all of our device specific stuff */


static int usb_read(struct i2c_adapter *adapter, int cmd,
		    int value, int index, void *data, int len)
{
	struct i2c_tiny_usb *dev = (struct i2c_tiny_usb *)adapter->algo_data;
	void *dmadata = kmalloc(len, GFP_KERNEL);
	int ret;

	if (!dmadata)
		return -ENOMEM;

	/* do control transfer */
	ret = usb_control_msg(dev->usb_dev, usb_rcvctrlpipe(dev->usb_dev, 0),
			       cmd, USB_TYPE_VENDOR | USB_RECIP_INTERFACE |
			       USB_DIR_IN, value, index, dmadata, len, 2000);

	memcpy(data, dmadata, len);
	kfree(dmadata);
	return ret;
}

static int usb_write(struct i2c_adapter *adapter, int cmd,
		     int value, int index, void *data, int len)
{
	struct i2c_tiny_usb *dev = (struct i2c_tiny_usb *)adapter->algo_data;
	void *dmadata = kmemdup(data, len, GFP_KERNEL);
	int ret;

	if (!dmadata)
		return -ENOMEM;

	/* do control transfer */
	ret = usb_control_msg(dev->usb_dev, usb_sndctrlpipe(dev->usb_dev, 0),
			       cmd, USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			       value, index, dmadata, len, 2000);

	kfree(dmadata);
	return ret;
}

static void i2c_tiny_usb_free(struct i2c_tiny_usb *dev)
{
	usb_put_dev(dev->usb_dev);
	kfree(dev);
}

static int i2c_gpio_probe(struct usb_interface *interface)
{
//	int ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	int inf;
	inf = interface->cur_altsetting->desc.bInterfaceNumber;

	if (inf > 0) {
		dev_info(&interface->dev, "Ignoring Interface\n");
		return -ENODEV;
		}
	if (inf < 0) {
		dev_info(&interface->dev, "Ignoring Interface\n");
		return -ENODEV;
		}

	return 0;
}

static int i2c_tiny_usb_probe(struct usb_interface *interface,
			      const struct usb_device_id *id)
{
	struct i2c_tiny_usb *dev;
	struct usb_device *udev = interface_to_usbdev(interface);
//	struct my_usb *data;
	int retval = -ENOMEM;
	u16 version;
	int ret;

	dev_dbg(&interface->dev, "probing usb device\n");

	/* allocate memory for our device state and initialize it */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	dev->usb_dev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

     if (dev->usb_dev->product && !strcmp(dev->usb_dev->product, "i2c-stm32f4-usb")) {
	 	ret = i2c_gpio_probe(interface);
		if (ret < 0) {
		    	return -ENODEV;
     	}
	 }

	
	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	version = le16_to_cpu(dev->usb_dev->descriptor.bcdDevice);
	dev_info(&interface->dev,
		 "version %x.%02x found at bus %03d address %03d\n",
		 version >> 8, version & 0xff,
		 dev->usb_dev->bus->busnum, dev->usb_dev->devnum);

	/* setup i2c adapter description */
	dev->adapter.owner = THIS_MODULE;
	dev->adapter.class = I2C_CLASS_HWMON;
	dev->adapter.algo = &usb_algorithm;
	dev->adapter.algo_data = dev;
	snprintf(dev->adapter.name, sizeof(dev->adapter.name),
		 "i2c-tiny-usb at bus %03d device %03d",
		 dev->usb_dev->bus->busnum, dev->usb_dev->devnum);

	if (usb_write(&dev->adapter, CMD_SET_DELAY, delay, 0, NULL, 0) != 0) {
		dev_err(&dev->adapter.dev,
			"failure setting delay to %dus\n", delay);
		retval = -EIO;
		goto error;
	}

/*   data = kzalloc(sizeof(struct my_usb), GFP_KERNEL);
   if (data == NULL)
     {
        //handle error
     }

	data->udev = usb_get_dev(interface_to_usbdev(interface));
	data->interface = interface;
*/	
	dev->adapter.dev.parent = &dev->interface->dev;

	/* and finally attach to i2c layer */
	i2c_add_adapter(&dev->adapter);

	/* inform user about successful attachment to i2c layer */
	dev_info(&dev->adapter.dev, "connected i2c-tiny-usb device\n");


   dev->usb_dev = usb_get_dev(udev);

   /// gpio_chip struct info is inside KERNEL/include/linux/gpio/driver.h
   dev->chip.label = "vusb-gpio"; //name for diagnostics
//   data->chip.dev = &data->udev->dev; // optional device providing the GPIOs
   dev->chip.parent = &interface->dev;
   dev->chip.owner = THIS_MODULE; // helps prevent removal of modules exporting active GPIOs, so this is required for proper cleanup
   dev->chip.base = -1; // identifies the first GPIO number handled by this chip; 
   // or, if negative during registration, requests dynamic ID allocation.
   // i was getting 435 on -1.. nice. Although, it is deprecated to provide static/fixed base value. 

   dev->chip.ngpio = 2; // the number of GPIOs handled by this controller; the last GPIO
   dev->chip.can_sleep = true; // 
   /*
      flag must be set iff get()/set() methods sleep, as they
    * must while accessing GPIO expander chips over I2C or SPI. This
    * implies that if the chip supports IRQs, these IRQs need to be threaded
    * as the chip access may sleep when e.g. reading out the IRQ status
    * registers.
    */
   dev->chip.set = _gpioa_set;
   dev->chip.get = _gpioa_get;
   //TODO  implement it later in firmware
   dev->chip.direction_input = _direction_input;
   dev->chip.direction_output = _direction_output;

   if (gpiochip_add(&dev->chip) < 0)
     {
        printk(KERN_ALERT "Failed to add gpio chip");
     }
   else
     {
        printk(KERN_INFO "Able to add gpiochip: %s", dev->chip.label);
     }

   usb_set_intfdata(interface, dev);
   
   INIT_WORK(&dev->work, _gpio_work_job);
   INIT_WORK(&dev->work2, _gpio_work_job2);
   
   
	return 0;

 error:
	if (dev)
		i2c_tiny_usb_free(dev);

	return retval;
}

static void i2c_tiny_usb_disconnect(struct usb_interface *interface)
{
	struct i2c_tiny_usb *dev = usb_get_intfdata(interface);
    gpiochip_remove(&dev->chip);
	i2c_del_adapter(&dev->adapter);
	usb_set_intfdata(interface, NULL);
	i2c_tiny_usb_free(dev);



   //deref the count
    usb_put_dev(dev->usb_dev);

    kfree(dev); //deallocate, allocated by kzmalloc()
	dev_dbg(&interface->dev, "disconnected\n");
}

static struct usb_driver i2c_tiny_usb_driver = {
	.name		= "i2c-tiny-usb",
	.probe		= i2c_tiny_usb_probe,
	.disconnect	= i2c_tiny_usb_disconnect,
	.id_table	= i2c_tiny_usb_table,
};

module_usb_driver(i2c_tiny_usb_driver);

/* ----- end of usb layer ------------------------------------------------ */

MODULE_AUTHOR("Till Harbaum <Till@Harbaum.org>");
MODULE_DESCRIPTION("i2c-tiny-usb driver v1.0");
MODULE_LICENSE("GPL");
