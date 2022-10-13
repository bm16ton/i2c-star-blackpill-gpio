// Author: (C) 2016 Amitesh Singh

#include <linux/init.h>// macros used to markup functions e.g. __init, __exit
#include <linux/module.h>// Core header for loading LKMs into the kernel
#include <linux/kernel.h>// Contains types, macros, functions for the kernel
#include <linux/device.h>// Header to support the kernel Driver Model
#include <linux/version.h>

#include <linux/usb.h> //for usb stuffs
#include <linux/slab.h> //for kzmalloc and kfree

#include <linux/workqueue.h> //for work_struct
#include <linux/gpio.h> //for led
#include <linux/gpio/driver.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>


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
     struct mutex		io_mutex;
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
     int               num;
     uint8_t           irq_num;                            // number of pins with IRQs
     int               irq_base;                           // base IRQ allocated
//     struct irq_desc*  irq_descs    [5]; // IRQ descriptors used (irq_num elements)
    const struct cpumask *aff_mask;
     int               irq_types    [5]; // IRQ types (irq_num elements)
     bool              irq_enabled  [5]; // IRQ enabled flag (irq_num elements)
     int               irq_gpio_map [5]; // IRQ to GPIO pin map (irq_num elements)
     int               irq_hw;                             // IRQ for GPIO with hardware IRQ (default -1)
    
     u8 bufr[4];
};

#define MY_USB_VENDOR_ID 0x0403
#define MY_USB_PRODUCT_ID 0xc631
static struct usb_device_id my_usb_table[] = {
       { USB_DEVICE(MY_USB_VENDOR_ID, MY_USB_PRODUCT_ID) },
       {},
};

MODULE_DEVICE_TABLE(usb, my_usb_table);

static int i2c_gpio_to_irq(struct gpio_chip *chip, unsigned offset);

unsigned int GPIO_irqNumber;

static uint8_t gpio_val = 0;      // brequest
static uint8_t offs = 0;          // windex?
static uint8_t usbval = 0;        // wvalue
int shutdval = 0;
int irqt = 2;
int irqon = 0;
static void
_gpio_work_job(struct work_struct *work)
{
   struct my_usb *sd = container_of(work, struct my_usb, work);
	mutex_lock(&sd->io_mutex);
	if (!sd->interface) {
		mutex_unlock(&sd->io_mutex);
    return;
	}
   printk(KERN_ALERT "gpioval i/o: %d \n", gpio_val);
   printk(KERN_ALERT "usbval i/o: %d \n", usbval);
   printk(KERN_ALERT "offset i/o: %d \n",offs);
   usb_control_msg(sd->udev,
                   usb_sndctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_OUT,
                   usbval, offs,
                   NULL, 0,
                   3000);
    mutex_unlock(&sd->io_mutex);
}

static void
_gpio_work_job2(struct work_struct *work2)
{
   struct my_usb *sd = container_of(work2, struct my_usb, work2);

	mutex_lock(&sd->io_mutex);
	if (!sd->interface) {
		mutex_unlock(&sd->io_mutex);
    return;
    }
   printk(KERN_ALERT "Read port i/o: %d \n", offs);
   usb_control_msg(sd->udev,
                   usb_rcvctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_IN,
                   usbval, offs,
                   (u8 *)sd->bufr, 4,
                   100);
    mutex_unlock(&sd->io_mutex);
}

static void
int_cb(struct urb *urb)
{
   struct my_usb *sd = urb->context;
   unsigned long flags;
   char *intrxbuf = kmalloc(4, GFP_KERNEL);
   if (!intrxbuf)
		printk(KERN_ALERT "Failed to create intrxbuf \n");
		
   printk(KERN_ALERT "urb interrupt is called \n");
   memcpy(sd->int_in_buf, intrxbuf, 4);
if (irqon == 1) {
   local_irq_save(flags);
   generic_handle_irq(GPIO_irqNumber);
   local_irq_restore(flags);
   printk(KERN_ALERT "irq fired, and received data: %s \n", intrxbuf);
} else {
   printk(KERN_ALERT "but irq currently disabled");
}
if (shutdval == 0) {
   usb_submit_urb(sd->int_in_urb, GFP_KERNEL);
}
   kfree(intrxbuf);
}

static void
_gpioa_set(struct gpio_chip *chip,
           unsigned offset, int value)
{
   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
   printk(KERN_INFO "GPIO SET INFO for pin: %d \n", offset);

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
   char *rxbuf = kmalloc(4, GFP_KERNEL);
   if (!rxbuf)
		return -ENOMEM;
		
    printk(KERN_INFO "GPIO GET INFO: %d \n", offset);

    usbval = 3;
	offs = offset;
    gpio_val = 1;


    usb_control_msg(data->udev,
                   usb_rcvctrlpipe(data->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_IN,
                   usbval, offs,
                   rxbuf, 4,
                   100);
              
    memcpy(data->bufr, rxbuf, 4);          
                                      
    retval = rxbuf[0];
    retval1 = rxbuf[1];
    retval2 = rxbuf[2];
    retval3 = rxbuf[3];
    printk("buf0 =  %d \n", retval);
    printk("buf1 =  %d \n", retval1);
    printk("buf2 =  %d \n", retval2);
    printk("buf3 =  %d \n", retval3);

    kfree(rxbuf);
 
    return retval1 - 3; 

}

static int
_direction_output(struct gpio_chip *chip,
                  unsigned offset, int value)
{
	   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
   printk("Setting pin to OUTPUT \n");
   
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
                                      
   printk("Setting pin to INPUT \n");


        usbval = 1;
		offs = offset;

        schedule_work(&data->work);

   return 0;
}

static int
i2c_gpio_to_irq(struct gpio_chip *chip,
                  unsigned offset)
{
   struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
   GPIO_irqNumber = irq_create_mapping(data->chip.irq.domain, offset);
   pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);

   return GPIO_irqNumber;
}

static void usb_gpio_irq_enable(struct irq_data *irqd)
{
	struct my_usb *dev = irq_data_get_irq_chip_data(irqd);
    irqon = 1;
	if (dev->irq_enabled[4])
		return;


    dev->irq_enabled[4] = true;

}

void set_irq_disabled(struct irq_data *irqd)
{
    struct my_usb *dev = irq_data_get_irq_chip_data(irqd);
    irqon = 0;
    if (!dev->irq_enabled[4])
        return;
        
    dev->irq_enabled[4] = false;
}

static void usb_gpio_irq_disable(struct irq_data *irqd)
{
    struct gpio_chip *chip = irq_data_get_irq_chip_data(irqd);
    struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
	/* Is that needed? */
//	if (!chip->irq_enabled[4])
//		return;

   usbval = 9;
   offs = 1;
   gpio_val = 9;
   schedule_work(&data->work);
   set_irq_disabled(irqd);
//   dev->irq_enabled[4] = false;
//	dev->irq.irq_enable = false;
//	usb_kill_urb(dev->int_in_urb);
}

static int usbirq_irq_set_type(struct irq_data *irqd, unsigned type)
{
    struct gpio_chip *chip = irq_data_get_irq_chip_data(irqd);
    struct my_usb *data = container_of(chip, struct my_usb,
                                      chip);
    int pin = irqd_to_hwirq(irqd);
    pr_info("irq pin = %d\n", pin);
//    GPIO_irqNumber = gpio_to_irq(pin);
    irqt = type;
    pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);
    	switch (type) {
    case IRQ_TYPE_NONE:
		   usbval = 9;
           offs = 1;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		   usbval = 9;
           offs = 2;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		   usbval = 9;
           offs = 3;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		   usbval = 9;
           offs = 4;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	case IRQ_TYPE_EDGE_RISING:
		   usbval = 9;
           offs = 5;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		   usbval = 9;
           offs = 6;
           gpio_val = 9;
           schedule_work(&data->work);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}    

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,18,0)    
static const struct irq_chip usb_gpio_irqchip = {
	.name = "usbgpio-irq",
	.irq_enable =  usb_gpio_irq_enable,
	.irq_disable = usb_gpio_irq_disable,
	.irq_set_type = usbirq_irq_set_type,
	.flags = IRQCHIP_IMMUTABLE, GPIOCHIP_IRQ_RESOURCE_HELPERS,
};
#endif

const char *gpio_names[] = { "LED", "usbGPIO2", "BTN", "usbGPIO4", "IRQpin" };

//called when a usb device is connected to PC
static int
my_usb_probe(struct usb_interface *interface,
             const struct usb_device_id *id)
{
   struct usb_device *udev = interface_to_usbdev(interface);
   struct usb_host_interface *iface_desc;
   struct usb_endpoint_descriptor *endpoint;
   struct my_usb *data;
   struct gpio_irq_chip *girq;
   int i;
   int inf;
   int err;
   int rc;
   
   printk(KERN_INFO "manufacturer: %s \n", udev->manufacturer);
   printk(KERN_INFO "product: %s \n", udev->product);

   iface_desc = interface->cur_altsetting;
   printk(KERN_INFO "vusb led %d probed: (%04X:%04X) \n",
          iface_desc->desc.bInterfaceNumber, id->idVendor, id->idProduct);
   printk(KERN_INFO "bNumEndpoints: %d \n", iface_desc->desc.bNumEndpoints);

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
   data->int_in_endpoint = endpoint;
   // allocate our urb for interrupt in 
   data->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
   //allocate the interrupt buffer to be used
   data->int_in_buf = kmalloc(le16_to_cpu(data->int_in_endpoint->wMaxPacketSize), GFP_KERNEL);

   //initialize our interrupt urb
   //notice the rcvintpippe -- it is for recieving data from device at interrupt endpoint
   usb_fill_int_urb(data->int_in_urb, udev,
                    usb_rcvintpipe(udev, data->int_in_endpoint->bEndpointAddress),
                    data->int_in_buf,
                    le16_to_cpu(data->int_in_endpoint->wMaxPacketSize),
                    int_cb, // this callback is called when we are done sending/recieving urb
                    data,
                    (data->int_in_endpoint->bInterval));

   usb_set_intfdata(interface, data);

   printk(KERN_INFO "usb gpio irq is connected \n");

   i = usb_submit_urb(data->int_in_urb, GFP_KERNEL);
   if (i)
     {
        printk(KERN_ALERT "Failed to submit urb \n");
     }
     


   /// gpio_chip struct info is inside KERNEL/include/linux/gpio/driver.h
   data->chip.label = "vusb-gpio"; //name for diagnostics
//   data->chip.dev = &data->udev->dev; // optional device providing the GPIOs
   data->chip.parent = &interface->dev;
   data->chip.owner = THIS_MODULE; // helps prevent removal of modules exporting active GPIOs, so this is required for proper cleanup
   data->chip.base = -1; // identifies the first GPIO number handled by this chip; 
   // or, if negative during registration, requests dynamic ID allocation.
   // i was getting 435 on -1.. nice. Although, it is deprecated to provide static/fixed base value. 

   data->chip.ngpio = 5; // the number of GPIOs handled by this controller; the last GPIO
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
   #if LINUX_VERSION_CODE <= KERNEL_VERSION(5,18,0)    
   data->irq.name = "usbgpio-irq";
   data->irq.irq_set_type = usbirq_irq_set_type;
   data->irq.irq_enable = usb_gpio_irq_enable;
   data->irq.irq_disable = usb_gpio_irq_disable;

	girq = &data->chip.irq;
	girq->chip = &data->irq;
	#else 
	girq = &data->chip.irq;
    gpio_irq_chip_set_chip(girq, &usb_gpio_irqchip);
    #endif
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_simple_irq;
	
	rc = irq_alloc_desc(0);
	if (rc < 0) {
		dev_err(&interface->dev, "Cannot allocate an IRQ desc \n");
		return rc;
	}
	
//   girq->irq_num = rc;
	
   if (gpiochip_add(&data->chip) < 0)
     {
        printk(KERN_ALERT "Failed to add gpio chip \n");
     }
   else
     {
        printk(KERN_INFO "Able to add gpiochip: %s \n", data->chip.label);
     }

//   gpio_direction_input(5);
//   gpio_export_link(data->chip, 3, BTN);
   i2c_gpio_to_irq(&data->chip, 4);
   usb_set_intfdata(interface, data);

   printk(KERN_INFO "usb device is connected \n");

   INIT_WORK(&data->work, _gpio_work_job);
   INIT_WORK(&data->work2, _gpio_work_job2);
   mutex_init(&data->io_mutex);
                   
   return 0;
}

//called when unplugging a USB device
static void
my_usb_disconnect(struct usb_interface *interface)
{
   struct my_usb *data;


   data = usb_get_intfdata(interface);

   shutdval = 1;

    if (&data->work) {
   cancel_work_sync(&data->work);
   }
   if (&data->work2) {
   cancel_work_sync(&data->work2);
   }
   if (&data->chip) {
   gpiochip_remove(&data->chip);
   }
   if (data->int_in_urb) {
   usb_kill_urb(data->int_in_urb);
   usb_free_urb(data->int_in_urb);
   }
   if (data->irq_enabled[4]) {
   data->irq_enabled[4] = false;
   }
   if (data->int_in_buf) {
   kfree(data->int_in_buf);
   }
   if (data->bufr) {
   kfree(data->bufr);
   }
   if (data->irq_base) {
   irq_free_descs(data->irq_base, data->irq_num);
   }
   
    mutex_lock(&data->io_mutex);
 	data->interface = NULL;
   usb_set_intfdata(interface, NULL);
   mutex_unlock(&data->io_mutex);
   //deref the count
   usb_put_dev(data->udev);

   kfree(data); //deallocate, allocated by kzmalloc()

   printk(KERN_INFO "usb device is disconnected \n");
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
   printk(KERN_INFO "usb driver is loaded \n");

   result = usb_register(&my_usb_driver);
   if (result)
     {
        printk(KERN_ALERT "device registeration failed!! \n");
     }
   else
     {
        printk(KERN_INFO "device registered\n");
     }

   return result;
}

//called on module unloading
static void __exit
_usb_exit(void)
{
   printk(KERN_INFO "usb driver is unloaded\n");
   usb_deregister(&my_usb_driver);
}

module_init(_usb_init);
module_exit(_usb_exit);
