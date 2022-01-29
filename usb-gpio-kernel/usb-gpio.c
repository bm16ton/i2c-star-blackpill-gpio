// Author: (C) 2016 Amitesh Singh

#include <linux/init.h>// macros used to markup functions e.g. __init, __exit
#include <linux/module.h>// Core header for loading LKMs into the kernel
#include <linux/kernel.h>// Contains types, macros, functions for the kernel
#include <linux/device.h>// Header to support the kernel Driver Model

#include <linux/usb.h> //for usb stuffs
#include <linux/slab.h> //for kzmalloc and kfree

#include <linux/workqueue.h> //for work_struct
#include <linux/gpio.h> //for led
#include <linux/gpio/driver.h>
#include <linux/irq.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ben Maddocks");
MODULE_DESCRIPTION("usb gpio stm32f411"); //sysfs
MODULE_VERSION("0.1"); 

//one structure for each connected device
struct my_usb {
     struct usb_device *udev;
     struct usb_interface *interface;	/* the interface for this device */
     struct usb_endpoint_descriptor *int_out_endpoint;
     //this gonna hold the data which we send to device
     uint8_t *int_out_buf;
     struct urb *int_out_urb;

     //ENPOINT IN - 1 - interrupt
     struct usb_endpoint_descriptor *int_in_endpoint;
     uint8_t *int_in_buf;
     struct urb *int_in_urb;

     struct work_struct work;
     struct work_struct work2;
     struct gpio_chip chip; //this is our GPIO chip
     bool    hwirq;
     int                      gpio_irq_map[4]; // GPIO to IRQ map (gpio_num elements)
     
         struct irq_chip   irq;                                // chip descriptor for IRQs
    uint8_t           irq_num;                            // number of pins with IRQs
    int               irq_base;                           // base IRQ allocated
    struct irq_desc*  irq_descs    [4]; // IRQ descriptors used (irq_num elements)
    int               irq_types    [4]; // IRQ types (irq_num elements)
    bool              irq_enabled  [4]; // IRQ enabled flag (irq_num elements)
    int               irq_gpio_map [4]; // IRQ to GPIO pin map (irq_num elements)
    int               irq_hw;                             // IRQ for GPIO with hardware IRQ (default -1)
    
     u8 buf[4];
};

#define MY_USB_VENDOR_ID 0x0403
#define MY_USB_PRODUCT_ID 0xc631
static struct usb_device_id my_usb_table[] = {
       { USB_DEVICE(MY_USB_VENDOR_ID, MY_USB_PRODUCT_ID) },
       {},
};

MODULE_DEVICE_TABLE(usb, my_usb_table);

unsigned int GPIO_irqNumber;

static uint8_t gpio_val = 0;
static uint8_t offs = 0;
static uint8_t usbval = 0;

static void
_gpio_work_job(struct work_struct *work)
{
   struct my_usb *sd = container_of(work, struct my_usb, work);

   printk(KERN_ALERT "gpioval i/o: %d", gpio_val);
   printk(KERN_ALERT "usbval i/o: %d", usbval);
   printk(KERN_ALERT "offset i/o: %d",offs);
   usb_control_msg(sd->udev,
                   usb_sndctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_OUT,
                   usbval, offs,
                   NULL, 0,
                   1000);
}

static void
_gpio_work_job2(struct work_struct *work2)
{
   struct my_usb *sd = container_of(work2, struct my_usb, work2);

   printk(KERN_ALERT "Read port i/o: %d", offs);
   usb_control_msg(sd->udev,
                   usb_rcvctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_IN,
                   usbval, offs,
                   (u8 *)sd->buf, 4,
                   1000);
}

//this is called when you do echo 1 > value
static void
_gpioa_set(struct gpio_chip *chip,
           unsigned offset, int value)
{
   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
   printk(KERN_INFO "GPIO SET INFO for pin: %d", offset);

   usbval = 0;

		offs = offset;
        gpio_val = value;
        schedule_work(&data->work);
}

static int
_gpioa_get(struct gpio_chip *chip,
           unsigned offset)
{
   struct my_usb *data = container_of(chip, struct my_usb,
                                     chip);

   int retval, retval1, retval2, retval3;

    printk(KERN_INFO "GPIO GET INFO: %d", offset);

    usbval = 3;
	offs = offset;
    gpio_val = 1;

    usleep_range(5000, 5200);
    schedule_work(&data->work2);
    usleep_range(5000, 5200);
    retval = data->buf[0];
    retval1 = data->buf[1];
    retval2 = data->buf[2];
    retval3 = data->buf[3];
    printk("buf0 =  %d", retval);
    printk("buf1 =  %d", retval1);
    printk("buf2 =  %d", retval2);
    printk("buf3 =  %d", retval3);

 
    return retval1 - 2; 

}

static int
_direction_output(struct gpio_chip *chip,
                  unsigned offset, int value)
{
	   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
   printk("Setting pin to OUTPUT");
   
        usbval = 2;
		offs = offset;

        schedule_work(&data->work);

   return 0;
}

static int
_direction_input(struct gpio_chip *chip,
                  unsigned offset)
{
   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
                                      
   printk("Setting pin to INPUT");


        usbval = 1;
		offs = offset;

        schedule_work(&data->work);

   return 0;
}

static int
i2c_gpio_to_irq(struct gpio_chip *chip,
                  unsigned offset)
{
//   struct my_usb *data = container_of(chip, struct my_usb,
//                                      chip);
   GPIO_irqNumber = gpio_to_irq(offset);
   pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);

   return GPIO_irqNumber;
}

const char *gpio_names[] = { "LED-PC13", "PC14", "BTN-PA0", "PC15" };

//called when a usb device is connected to PC
static int
my_usb_probe(struct usb_interface *interface,
             const struct usb_device_id *id)
{
   struct usb_device *udev = interface_to_usbdev(interface);
   struct usb_host_interface *iface_desc;
   struct usb_endpoint_descriptor *endpoint;
   struct my_usb *data;
   int i;
   int inf;
   
   printk(KERN_INFO "manufacturer: %s", udev->manufacturer);
   printk(KERN_INFO "product: %s", udev->product);

   iface_desc = interface->cur_altsetting;
   printk(KERN_INFO "vusb led %d probed: (%04X:%04X)",
          iface_desc->desc.bInterfaceNumber, id->idVendor, id->idProduct);
   printk(KERN_INFO "bNumEndpoints: %d", iface_desc->desc.bNumEndpoints);

   for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
     {
        endpoint = &iface_desc->endpoint[i].desc;

        printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n",
               i, endpoint->bEndpointAddress);
        printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n",
               i, endpoint->bmAttributes);
        printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n",
               i, endpoint->wMaxPacketSize, endpoint->wMaxPacketSize);
     }

   data = kzalloc(sizeof(struct my_usb), GFP_KERNEL);
   if (data == NULL)
     {
        //handle error
     }

	data->udev = usb_get_dev(interface_to_usbdev(interface));
	data->interface = interface;

	inf = data->interface->cur_altsetting->desc.bInterfaceNumber;
	if (inf > 1) {
		dev_info(&interface->dev, "Ignoring Interface\n");
		return -ENODEV;
		}
	if (inf < 1) {
		dev_info(&interface->dev, "Ignoring Interface\n");
		return -ENODEV;
		}

   //increase ref count, make sure u call usb_put_dev() in disconnect()
   data->udev = usb_get_dev(udev);

   /// gpio_chip struct info is inside KERNEL/include/linux/gpio/driver.h
   data->chip.label = "vusb-gpio"; //name for diagnostics
//   data->chip.dev = &data->udev->dev; // optional device providing the GPIOs
   data->chip.parent = &interface->dev;
   data->chip.owner = THIS_MODULE; // helps prevent removal of modules exporting active GPIOs, so this is required for proper cleanup
   data->chip.base = -1; // identifies the first GPIO number handled by this chip; 
   // or, if negative during registration, requests dynamic ID allocation.
   // i was getting 435 on -1.. nice. Although, it is deprecated to provide static/fixed base value. 

   data->chip.ngpio = 4; // the number of GPIOs handled by this controller; the last GPIO
   data->chip.can_sleep = true; // 
   /*
      flag must be set iff get()/set() methods sleep, as they
    * must while accessing GPIO expander chips over I2C or SPI. This
    * implies that if the chip supports IRQs, these IRQs need to be threaded
    * as the chip access may sleep when e.g. reading out the IRQ status
    * registers.
    */
   data->chip.set = _gpioa_set;
   data->chip.get = _gpioa_get;
   //TODO  implement it later in firmware
   data->chip.direction_input = _direction_input;
   data->chip.direction_output = _direction_output;
   data->chip.to_irq = i2c_gpio_to_irq;
   data->chip.names = gpio_names;

   if (gpiochip_add(&data->chip) < 0)
     {
        printk(KERN_ALERT "Failed to add gpio chip");
     }
   else
     {
        printk(KERN_INFO "Able to add gpiochip: %s", data->chip.label);
     }

   gpio_direction_input(3);
   i2c_gpio_to_irq(&data->chip, 3);
  
   usb_set_intfdata(interface, data);

   printk(KERN_INFO "usb device is connected");

   INIT_WORK(&data->work, _gpio_work_job);
   INIT_WORK(&data->work2, _gpio_work_job2);

   //swith off the led
/*   usb_control_msg(data->udev,
                   usb_sndctrlpipe(data->udev, 0),
                   0, USB_TYPE_VENDOR | USB_DIR_OUT,
                   0, 0,
                   NULL, 0,
                   1000);

   usb_control_msg(data->udev,
                   usb_sndctrlpipe(data->udev, 0),
                   1, USB_TYPE_VENDOR | USB_DIR_OUT,
                   0, 1,
                   NULL, 0,
                   1000);
*/                   
   return 0;
}

//called when unplugging a USB device
static void
my_usb_disconnect(struct usb_interface *interface)
{
   struct my_usb *data;

   data = usb_get_intfdata(interface);

   cancel_work_sync(&data->work);
   gpiochip_remove(&data->chip);

   usb_set_intfdata(interface, NULL);

   //deref the count
   usb_put_dev(data->udev);

   kfree(data); //deallocate, allocated by kzmalloc()

   printk(KERN_INFO "usb device is disconnected");
}

static struct usb_driver my_usb_driver = {
     .name = "my first usb driver",
     .id_table = my_usb_table,
     .probe = my_usb_probe,
     .disconnect = my_usb_disconnect,
};


//we could use module_usb_driver(my_usb_driver); instead of 
// init and exit functions
//called on module loading
static int __init
_usb_init(void)
{
   int result;
   printk(KERN_INFO "usb driver is loaded");

   result = usb_register(&my_usb_driver);
   if (result)
     {
        printk(KERN_ALERT "device registeration failed!!");
     }
   else
     {
        printk(KERN_INFO "device registered");
     }

   return result;
}

//called on module unloading
static void __exit
_usb_exit(void)
{
   printk(KERN_INFO "usb driver is unloaded");
   usb_deregister(&my_usb_driver);
}

module_init(_usb_init);
module_exit(_usb_exit);
