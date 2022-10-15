// Author: (C) 2016 Amitesh Singh

#include <linux/init.h>// macros used to markup functions e.g. __init, __exit
#include <linux/module.h>// Core header for loading LKMs into the kernel
#include <linux/kernel.h>// Contains types, macros, functions for the kernel
#include <linux/device.h>// Header to support the kernel Driver Model
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/uaccess.h>

#include <linux/mutex.h>
#include <linux/usb.h> //for usb stuffs
#include <linux/slab.h> //for kzmalloc and kfree

#include <linux/workqueue.h> //for work_struct
#include <linux/gpio.h> //for led
#include <linux/gpio/driver.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/pwm.h>

#define MAX_TRANSFER		(PAGE_SIZE - 512)

#define WRITES_IN_FLIGHT	8

#define SKEL_VENDOR_ID 0x1d50
#define SKEL_PRODUCT_ID 0x6018

static struct usb_device_id skel_table[] = {
        { USB_DEVICE_AND_INTERFACE_INFO(0x1d50, 0x6018, 0xEF, 0x00, 0x00) }, 
       {},
};
MODULE_DEVICE_TABLE(usb, skel_table);

#define SKEL_MINOR_BASE	192

void askforadc(struct device *dev);
static void skel_write_bulk_callback(struct urb *urb);
static void int_cb(struct urb *urb);
static void int_cb2(struct urb *urb);

//one structure for each connected device
struct skel {
     struct usb_device *udev;
     struct usb_interface *interface;	/* the interface for this device */
     struct semaphore	limit_sem;		/* limiting the number of writes in progress */
     struct usb_anchor	submitted;		/* in case we need to retract our submissions */
     struct usb_endpoint_descriptor *int_out;
     __u8			int_out_endpointAddr;
     //this gonna hold the data which we send to device
     uint8_t *int_out_buf;
     struct urb *int_out_urb;

     //ENPOINT IN - 1 - interrupt
//     struct usb_endpoint_descriptor *int_in;
//     uint8_t *int_in_buf;
 //    struct urb *int_in_urb;
    struct usb_endpoint_descriptor *int_in;
	struct urb		*int_in_urb;		/* the urb to read data with */
	unsigned char           *int_in_buf;	/* the buffer to receive data */
	size_t			int_in_size;		/* the size of the receive buffer */
	size_t			int_in_filled;		/* number of bytes in the buffer */
	size_t			int_in_copied;		/* already copied to user space */
	__u8			int_in_endpointAddr;	/* the address of the bulk in endpoint */
	int			errors;			/* the last request tanked */
	bool			ongoing_read;		/* a read is going on */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
	unsigned long		disconnected:1;
	wait_queue_head_t	int_in_wait;		/* to wait for an ongoing read */
//	 struct usb_endpoint_descriptor *int_in;
//     uint8_t *int_in_buf;
//     struct urb *int_in_urb;


//     struct urb *ack;
//	 uint8_t *ack_buf;
	 
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
    
     struct pwm_chip pwmchip;
     int               duty_cycle;
     int               period;
     int               polarity;
     int duty_ns, period_ns;
    
     u8 bufr[4];

     

};
#define to_skel_dev(d) container_of(d, struct skel, kref)

int adcred;

static struct usb_driver skel_driver;
static void skel_read_bulk_callback(struct urb *urb);
static int skel_suspend(struct usb_interface *intf, pm_message_t message);
static void skel_draw_down(struct skel *dev);




static void skel_delete(struct kref *kref)
{
	struct skel *dev = to_skel_dev(kref);

	usb_free_urb(dev->int_in_urb);
	usb_put_intf(dev->interface);
	usb_put_dev(dev->udev);
	kfree(dev->int_in_buf);
	kfree(dev);
}



static int skel_open(struct inode *inode, struct file *file)
{
	struct skel *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);

	interface = usb_find_interface(&skel_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
			__func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}

	retval = usb_autopm_get_interface(interface);
	if (retval)
		goto exit;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;

exit:
	return retval;
}

static int skel_do_read_io(struct skel *dev, size_t count)
{
	int rv;

	/* prepare a read */

/*			
       usb_fill_int_urb(dev->int_in_urb, dev->udev,
                    usb_rcvintpipe(dev->udev, 0x85),
                    dev->int_in_buf,
                    32,
                    int_cb, // this callback is called when we are done sending/recieving urb
                    dev,
                    3000);
    */                
	/* tell everybody to leave the URB alone */
	spin_lock_irq(&dev->err_lock);
	dev->ongoing_read = 1;
	spin_unlock_irq(&dev->err_lock);

	/* submit bulk in urb, which means no data to deliver */
	dev->int_in_filled = 0;
	dev->int_in_copied = 0;

	/* do it */
	rv = usb_submit_urb(dev->int_in_urb, GFP_KERNEL);
	if (rv < 0) {
		dev_err(&dev->interface->dev,
			"%s - failed submitting read urb, error %d\n",
			__func__, rv);
		rv = (rv == -ENOMEM) ? rv : -EIO;
		spin_lock_irq(&dev->err_lock);
		dev->ongoing_read = 0;
		spin_unlock_irq(&dev->err_lock);
	}

	return rv;
}

static ssize_t skel_read(struct file *file, char *buffer, size_t count,
			 loff_t *ppos)
{
	struct skel *dev;
	int rv;
	bool ongoing_io;
//    struct skel *data = dev_get_drvdata(dev);
    
	dev = file->private_data;
    
	if (!count)
		return 0;

	/* no concurrent readers */
	rv = mutex_lock_interruptible(&dev->io_mutex);
	if (rv < 0)
		return rv;

	if (dev->disconnected) {		/* disconnect() was called */
		rv = -ENODEV;
		goto exit;
	}

	/* if IO is under way, we must not touch things */
retry:
	spin_lock_irq(&dev->err_lock);
	ongoing_io = dev->ongoing_read;
	spin_unlock_irq(&dev->err_lock);

	if (ongoing_io) {
		/* nonblocking IO shall not wait */
		if (file->f_flags & O_NONBLOCK) {
			rv = -EAGAIN;
			goto exit;
		}
		/*
		 * IO may take forever
		 * hence wait in an interruptible state
		 */
		rv = wait_event_interruptible(dev->int_in_wait, (!dev->ongoing_read));
		if (rv < 0)
			goto exit;
	}

	/* errors must be reported */
	rv = dev->errors;
	if (rv < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		rv = (rv == -EPIPE) ? rv : -EIO;
		/* report it */
		goto exit;
	}

	/*
	 * if the buffer is filled we may satisfy the read
	 * else we need to start IO
	 */

	if (dev->int_in_filled) {
		/* we had read data */
		size_t available = dev->int_in_filled - dev->int_in_copied;
		size_t chunk = min(available, count);

		if (!available) {
			/*
			 * all data has been used
			 * actual IO needs to be done
			 */
//			askforadc(dev->device);
			rv = skel_do_read_io(dev, count);
			if (rv < 0)
				goto exit;
			else
				goto retry;
		}
		/*
		 * data is available
		 * chunk tells us how much shall be copied
		 */

		if (copy_to_user(buffer,
				 dev->int_in_buf + dev->int_in_copied,
				 chunk))
			rv = -EFAULT;
		else
			rv = chunk;

		dev->int_in_copied += chunk;

		/*
		 * if we are asked for more than we have,
		 * we start IO but don't wait
		 */
		if (available < count)
			skel_do_read_io(dev, count - chunk);
	} else {
		/* no data in the buffer */
		rv = skel_do_read_io(dev, count);
		if (rv < 0)
			goto exit;
		else
			goto retry;
	}
exit:
	mutex_unlock(&dev->io_mutex);
	return rv;
}

static ssize_t skel_write(struct file *file, const char *user_buffer,
			  size_t count, loff_t *ppos)
{
	struct skel *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;
	size_t writesize = min(count, (size_t)MAX_TRANSFER);

	dev = file->private_data;

	/* verify that we actually have some data to write */
	if (count == 0)
		goto exit;

	/*
	 * limit the number of URBs in flight to stop a user from using up all
	 * RAM
	 */
	if (!(file->f_flags & O_NONBLOCK)) {
		if (down_interruptible(&dev->limit_sem)) {
			retval = -ERESTARTSYS;
			goto exit;
		}
	} else {
		if (down_trylock(&dev->limit_sem)) {
			retval = -EAGAIN;
			goto exit;
		}
	}

	spin_lock_irq(&dev->err_lock);
	retval = dev->errors;
	if (retval < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
	}
	spin_unlock_irq(&dev->err_lock);
	if (retval < 0)
		goto error;

	/* create a urb, and a buffer for it, and copy the data to the urb */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}


//	buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
//				 &urb->transfer_dma);
	buf = kmalloc(64, GFP_KERNEL);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}

	if (copy_from_user(buf, user_buffer, writesize)) {
		retval = -EFAULT;
		goto error;
	}

	/* this lock makes sure we don't submit URBs to gone devices */
	mutex_lock(&dev->io_mutex);
	if (dev->disconnected) {		/* disconnect() was called */
		mutex_unlock(&dev->io_mutex);
		retval = -ENODEV;
		goto error;
	}

	/* initialize the urb properly */
	usb_fill_int_urb(urb, dev->udev,
			  usb_sndintpipe(dev->udev, dev->int_out->bEndpointAddress),
			  buf, writesize, int_cb2, dev, 3000);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb(urb, &dev->submitted);

	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&dev->io_mutex);
	if (retval) {
		dev_err(&dev->interface->dev,
			"%s - failed submitting write urb, error %d\n",
			__func__, retval);
		goto error_unanchor;
	}

	/*
	 * release our reference to this urb, the USB core will eventually free
	 * it entirely
	 */
	usb_free_urb(urb);


	return writesize;

error_unanchor:
	usb_unanchor_urb(urb);
error:
	if (urb) {
		usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	up(&dev->limit_sem);

exit:
	return retval;
}


static int skel_release(struct inode *inode, struct file *file)
{
	struct skel *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* allow the device to be autosuspended */
	usb_autopm_put_interface(dev->interface);

	/* decrement the count on our device */
	kref_put(&dev->kref, skel_delete);
	return 0;
}

static int skel_flush(struct file *file, fl_owner_t id)
{
	struct skel *dev;
	int res;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* wait for io to stop */
	mutex_lock(&dev->io_mutex);
	skel_draw_down(dev);

	/* read out errors, leave subsequent opens a clean slate */
	spin_lock_irq(&dev->err_lock);
	res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
	dev->errors = 0;
	spin_unlock_irq(&dev->err_lock);

	mutex_unlock(&dev->io_mutex);

	return res;
}

static const struct file_operations skel_fops = {
	.owner =	THIS_MODULE,
	.read =		skel_read,
	.write =	skel_write,
	.open =		skel_open,
	.release =	skel_release,
	.flush =	skel_flush,
	.llseek =	noop_llseek,
};

/*
static void skel_read_bulk_callback(struct urb *urb)
{
	struct skel *dev;
	unsigned long flags;

	dev = urb->context;

	spin_lock_irqsave(&dev->err_lock, flags);
	
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);

		dev->errors = urb->status;
	} else {
	
		dev->int_in_filled = urb->actual_length;
	}
//    usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
	dev->ongoing_read = 0;
	spin_unlock_irqrestore(&dev->err_lock, flags);

	wake_up_interruptible(&dev->int_in_wait);
}
*/
static void skel_read_bulk_callback(struct urb *urb)
{
;
}

static void skel_write_bulk_callback(struct urb *urb)
{
;
}
/*
static void skel_write_bulk_callback(struct urb *urb)
{
	struct skel *dev;
	unsigned long flags;

	dev = urb->context;


	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - write callback nonzero write bulk status received: %d\n",
				__func__, urb->status);

		spin_lock_irqsave(&dev->err_lock, flags);
		dev->errors = urb->status;
		spin_unlock_irqrestore(&dev->err_lock, flags);
	}


	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	up(&dev->limit_sem);
}

*/

unsigned int GPIO_irqNumber;

static uint8_t gpio_val = 0;      // brequest
static uint8_t offs = 0;          // windex?
static uint8_t usbval = 0;        // wvalue
int pdown = 0;
int powdn = 0;
int irqt = 2;
int irqyup = 0;
static void
_gpio_work_job(struct work_struct *work)
{
   struct skel *sd = container_of(work, struct skel, work);

   printk(KERN_ALERT "gpioval i/o: %d \n", gpio_val);
   printk(KERN_ALERT "usbval i/o: %d \n", usbval);
   printk(KERN_ALERT "offset i/o: %d \n",offs);
   usb_control_msg(sd->udev,
                   usb_sndctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_OUT,
                   usbval, offs,
                   NULL, 0,
                   3000);
}

static void
_gpio_work_job2(struct work_struct *work2)
{
   struct skel *sd = container_of(work2, struct skel, work2);

   printk(KERN_ALERT "Read port i/o: %d \n", offs);
   usb_control_msg(sd->udev,
                   usb_rcvctrlpipe(sd->udev, 0),
                   gpio_val, USB_TYPE_VENDOR | USB_DIR_IN,
                   usbval, offs,
                   (u8 *)sd->bufr, 4,
                   100);
}


static void
int_cb(struct urb *urb)
{
   struct skel *sd = urb->context;
   unsigned long flags;
   char *intrxbuf = kmalloc(32, GFP_KERNEL);
   if (!intrxbuf)
		printk(KERN_ALERT "Failed to create intrxbuf \n");
		
 //  		spin_lock_irqsave(&sd->err_lock, flags);
//		urb->status = 0;
	//	spin_unlock_irqrestore(&sd->err_lock, flags);
   
   printk(KERN_ALERT "urb interrupt is called \n");
   memcpy(intrxbuf, sd->int_in_buf, 32);
//   i2c_gpio_to_irq(&sd->chip, 3);
//   GPIO_irqNumber = gpio_to_irq(2);
//   pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);
//   generic_handle_domain_irq(sd->chip.irq.domain, 2);
//    handle_simple_irq (sd->irq_descs[3]);
//   local_irq_save(flags);
//   generic_handle_irq(GPIO_irqNumber);

//   local_irq_restore(flags);
   printk(KERN_ALERT "received data: %s \n", sd->int_in_buf);
   printk(KERN_ALERT "received data intrxbuf: %x \n", intrxbuf);
//   usb_submit_urb(sd->int_in_urb, GFP_KERNEL);
   kfree(intrxbuf);
   
}

static void
int_cb2(struct urb *urb)
{
   struct skel *sd = urb->context;
   unsigned long flags;
   char *intrxbuf = kmalloc(32, GFP_KERNEL);
   if (!intrxbuf)
		printk(KERN_ALERT "Failed to create intrxbuf \n");
		
 //  		spin_lock_irqsave(&sd->err_lock, flags);
//		urb->status = 0;
	//	spin_unlock_irqrestore(&sd->err_lock, flags);
   
   printk(KERN_ALERT "urb interrupt is called \n");
   memcpy(intrxbuf, sd->int_out_buf, 32);
//   i2c_gpio_to_irq(&sd->chip, 3);
//   GPIO_irqNumber = gpio_to_irq(2);
//   pr_info("GPIO_irqNumber = %d\n", GPIO_irqNumber);
//   generic_handle_domain_irq(sd->chip.irq.domain, 2);
//    handle_simple_irq (sd->irq_descs[3]);
//   local_irq_save(flags);
//   generic_handle_irq(GPIO_irqNumber);

//   local_irq_restore(flags);
   printk(KERN_ALERT "received data: %s \n", sd->int_out_buf);
   printk(KERN_ALERT "received data intrxbuf: %x \n", intrxbuf);
//   usb_submit_urb(sd->int_in_urb, GFP_KERNEL);
   kfree(intrxbuf);
   
}

static void
_gpioa_set(struct gpio_chip *chip,
           unsigned offset, int value)
{
   struct skel *data = container_of(chip, struct skel,
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
   struct skel *data = container_of(chip, struct skel,
                                     chip);

   int retval, retval1, retval2, retval3;
   char *rxbuf = kmalloc(4, GFP_KERNEL);
   if (!rxbuf)
		return -ENOMEM;
		
    printk(KERN_INFO "GPIO GET INFO: %d \n", offset);

    usbval = 3;
	offs = offset;
    gpio_val = 1;

//    usleep_range(1000, 1200);
//    schedule_work(&data->work2);
//    usleep_range(1000, 1200);
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
//    kfree(data->bufr);
 
    return retval1 - 3; 

}

static int
_direction_output(struct gpio_chip *chip,
                  unsigned offset, int value)
{
	   struct skel *data = container_of(chip, struct skel,
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
   struct skel *data = container_of(chip, struct skel,
                                      chip);
                                      
   printk("Setting pin to INPUT \n");


        usbval = 1;
		offs = offset;

        schedule_work(&data->work);

   return 0;
}




void askforadc(struct device *dev)
{
struct skel *data = dev_get_drvdata(dev);

 //   int actual_len;
 /*   usb_control_msg(data->udev,
                   usb_sndctrlpipe(data->udev, 0),
                   99, USB_TYPE_VENDOR | USB_DIR_IN,
                   99, 99,
                   NULL, 0,
                   3000);
                   */
   usbval = 99;
   offs = 99;
   gpio_val = 99;
   schedule_work(&data->work);

}


static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
   struct skel *data = dev_get_drvdata(dev);
   usbval = 93;
   offs = 93;
   gpio_val = 93;
   schedule_work(&data->work);
   return 0;
}

static ssize_t adc_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
    struct skel *data = dev_get_drvdata(dev);

   pdown = 0;
   if (powdn == 0) {

    
   usb_control_msg(data->udev,
                   usb_sndctrlpipe(data->udev, 0),
                   99, USB_TYPE_VENDOR | USB_DIR_IN,
                   99, 99,
                   NULL, 0,
                   1000);



    pdown = 0;
    return sprintf(buf, "sprint %d\n", data->int_in_buf);
//return 0;
    }
    return 0;
}

static ssize_t adcnc_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
struct skel *data = dev_get_drvdata(dev);

return sprintf(buf, "sprint %d\n", data->int_in_buf);
}

//static unsigned char gpio_ack_pkt[2] = { 0xaa, 0x02 };

static ssize_t bulktest_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
    
    struct skel *data = dev_get_drvdata(dev);
    struct urb *urb = NULL;
    int rv;
    int i;
    int volts;
	bool ongoing_io;
	unsigned char *bufr2;
	int actual_len;
	int ret;
	int ret2;
	int ret3;
	char *buf2 = NULL;
	int retval;
	size_t writesize = min(32, (size_t)MAX_TRANSFER);
//    int count = 6;
   	bufr2 = kmalloc(32, GFP_KERNEL);
	if (!bufr2) {
	    printk("failed 2 make bufr2\n");
    }
/*    
    	// create a urb, and a buffer for it, and copy the data to the urb 
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}
	
	buf2 = usb_alloc_coherent(data->udev, writesize, GFP_KERNEL,
				&urb->transfer_dma);
				
	mutex_lock(&data->io_mutex);
	if (data->disconnected) {		// disconnect() was called 
		mutex_unlock(&data->io_mutex);
		retval = -ENODEV;
		goto error;
	}
/*
	usb_fill_int_urb(urb, data->udev,
			  usb_sndintpipe(data->udev, data->int_out_endpointAddr),
			  buf2, writesize, int_cb2, data, 3000);
	urb->transfer_flags;
	usb_anchor_urb(urb, &data->submitted);

	// send the data out the bulk port 
	retval = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&data->io_mutex);
	if (retval) {
		dev_err(&data->interface->dev,
			"%s - failed submitting write urb, error %d\n",
			__func__, retval);
		goto error_unanchor;
	}
	*/
//   usbval = 99;
//   offs = 99;
//   gpio_val = 99;
//   schedule_work(&data->work);
//  askforadc(dev);
/*
  ret3 = usb_control_msg(data->udev,
                   usb_sndctrlpipe(data->udev, 0),
                   99, USB_TYPE_VENDOR | USB_DIR_IN,
                   99, 99,
                   NULL, 0,
                   1000);

   if (ret3)
    printk("sent control =%d\n", ret3);



    i = 0;
        for (i = 0; i < 63; i++) {
            bufr2[i] = 0;
        }

        for (i = 0; i < 63; i++) {
            buf[i] = 0;
        }

        for (i = 0; i < 63; i++) {
            data->bulk_in_buf[i] = 0;
        }
 */       
//		memset(bufr2, 0, 32);
		
//		usb_bulk_msg(data->udev, usb_sndbulkpipe(data->udev, 6),
//				   bufr2, 0, &actual_len,
//				   1000);
//        usb_submit_urb(data->bulk_in_urb, GFP_KERNEL);		

//    mutex_lock(&data->io_mutex);

/*
   usb_fill_int_urb(data->int_in_urb, data->udev,
                    usb_rcvintpipe(data->udev, 0x85),
                    bufr2,
                    63,
                    int_cb, // this callback is called when we are done sending/recieving urb
                    data,
                    3000);
*/
//    usb_submit_urb(data->int_in_urb, GFP_KERNEL);
    
//    printk(KERN_INFO "raw bufr2: %d \n", bufr2);
 //   rv = mutex_lock_interruptible(&data->io_mutex);
//	if (rv < 0) {
//	    printk("mutex-failed\n");
//		return rv;
//    }
//	if (data->disconnected) {		/* disconnect() was called */
//	    printk("disconnected\n");
//		rv = -ENODEV;
//		goto exit;
//	}
//    data->errors = 0;
 /*   
       usb_fill_int_urb(data->int_in_urb, data->udev,
                    usb_rcvintpipe(data->udev, 0x85),
                    bufr2,
                    32,
                    int_cb, // this callback is called when we are done sending/recieving urb
                    data,
                    3000);
  
    
   i = usb_submit_urb(data->int_in_urb, GFP_KERNEL);
   if (i)
     {
        printk(KERN_ALERT "bulktest Failed to submit urb \n");
     }
     */
    memset(bufr2, 0, 32);
     
    ret2 = usb_interrupt_msg(data->udev, usb_sndintpipe(data->udev, 5),
		bufr2, 32, &actual_len,
			3000);
	   if (ret2)
     {
        printk(KERN_ALERT "bulktest Failed to submit usb_interrupt_msg \n");
     }
    printk(KERN_INFO "actual legth %d\n", actual_len);
    printk(KERN_INFO "return code  %d\n", ret2);
    printk(KERN_INFO "bufr2  %d\n", bufr2);
    
   ret3 = usb_interrupt_msg(data->udev, usb_rcvintpipe(data->udev, 85),
		bufr2, 32, &actual_len,
			3000);
	   if (ret2)
     {
        printk(KERN_ALERT "bulktest Failed to submit usb_interrupt_msg \n");
     }
    printk(KERN_INFO "actual legth %d\n", actual_len);
    printk(KERN_INFO "return code  %d\n", ret3);
    printk(KERN_INFO "bufr2  %d\n", bufr2);
//        for (i = 0; i < 3; i++) {
//         ret = usb_interrupt_msg(data->udev, usb_rcvintpipe(data->udev, 0x85),
// 				   bufr2, 32, &actual_len,
// 				   1000);
//	    printk("loop buffer = %x\n");
//         if (ret < 0)
//            printk("int msg returned = %s\n", ret);

//        printk(KERN_INFO "bufr2: %d \n", bufr2);
//        if (ret < 0 || actual_len < 1 || bufr2[1] != actual_len)
//			continue;
//	}
//        usb_submit_urb(data->bulk_in_urb, GFP_KERNEL);
//    	for (i = 0; i < 3; i++) {
//		ret = usb_bulk_msg(data->udev, usb_rcvbulkpipe(data->udev, 86),
//				   bufr2, NULL, &actual_len,
//				   1000);
//		printk(KERN_INFO "looped raw bufr2: %d \n", bufr2);
//		if (ret < 0 || actual_len < 1 || bufr2[1] != actual_len)
//			continue;
//	}


//	printk(KERN_INFO "b4 do_read bulkin buf: %d \n", data->bulk_in_buf);
			
/*    adcred = skel_do_read_io(data, 64);
    if (adcred < 0)
        printk(KERN_INFO "adcread failed: %d \n", adcred);
        
    printk(KERN_INFO "after do_read bulkin buf: %d \n", data->bulk_in_buf);
    */
 //   printk(KERN_INFO "raw user buf: %d \n", buf);
//    printk(KERN_INFO "raw bufr2: %d \n", bufr2);
//		goto exit;
//    struct skel *dev;

//	dev = file->private_data;
//		    if (copy_to_user(buf, bufr2, actual_len)) {
//            printk("failed 2 copy to user\n");
//	}

//printk(KERN_INFO "raw buf: %d \n", bufr2);

	/* no concurrent readers */
//     printk(KERN_INFO "data->int_in_buf: %x \n", data->int_in_buf);
	
/*    adcred = skel_do_read_io(data, 64);
    if (adcred < 0)
        printk(KERN_INFO "adcread failed: %d \n", adcred);
//		goto exit;
    
//   if (copy_to_user(buf, data->bulk_in_buf + data->int_in_copied, sizeof(data->bulk_in_buf + data->int_in_copied)));
        printk(KERN_INFO "copy to user failed: %d \n", adcred);
//    kstrtouint(data->bulk_in_buf, 0, volts);
    
*///    printk(KERN_INFO "adcread: %d \n", adcred);
//    printk(KERN_INFO "volts: %d \n", volts);
 //   printk(KERN_INFO "raw user buf: %x \n", buf);
//    printk(KERN_INFO "raw buf: %d \n", bufr2);
	/* if IO is under way, we must not touch things */


		/*
		 * IO may take forever
		 * hence wait in an interruptible state
		 */
//		rv = wait_event_interruptible(data->int_in_wait, (!data->ongoing_read));
//		if (rv < 0)
//			goto exit;
	

	/* errors must be reported */
//	rv = data->errors;
//	if (rv < 0) {
		/* any error is reported once */
//		data->errors = 0;
		/* to preserve notifications about reset */
//		rv = (rv == -EPIPE) ? rv : -EIO;
		/* report it */
//		goto exit;
//	}

	/*
	 * if the buffer is filled we may satisfy the read
	 * else we need to start IO
	 */

//	if (data->int_in_filled) {
		/* we had read data */
//		size_t available = data->int_in_filled - data->int_in_copied;
//		size_t chunk = min(available, count);

//		if (!available) {
			/*
			 * all data has been used
			 * actual IO needs to be done
			 */

		
//usb_free_urb(urb);
//	}
return scnprintf(buf, PAGE_SIZE, "scnprint %d\n", data->int_in_buf);
	
error_unanchor:
	usb_unanchor_urb(urb);
	
error:
	if (urb) {
		usb_free_coherent(data->udev, writesize, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	up(&data->limit_sem);
		
exit:
	mutex_unlock(&data->io_mutex);
	return 1;
}

    


static ssize_t adctest_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
struct skel *data = dev_get_drvdata(dev);


   int retval, retval1, retval2, retval3;
   char *rxbuf = kmalloc(6, GFP_KERNEL);
   if (!rxbuf)
		return -ENOMEM;
		

    usb_control_msg(data->udev,
                   usb_rcvctrlpipe(data->udev, 0),
                  69, USB_TYPE_VENDOR | USB_DIR_IN,
                   69, 69,
                   rxbuf, 6,
                   100);
              
    memcpy(data->int_in_buf, rxbuf, 6);          
                                      
    retval = rxbuf[0];
    retval1 = rxbuf[1];
    retval2 = rxbuf[2];
    retval3 = rxbuf[3];
    printk("buf0 =  %d \n", retval);
    printk("buf1 =  %d \n", retval1);
    printk("buf2 =  %d \n", retval2);
    printk("buf3 =  %d \n", retval3);
    
    kfree(rxbuf);
//    kfree(data->bufr);
 
//return 0;
return sprintf(buf, "sprint %d\n", data->int_in_buf);
}


//static DEVICE_ATTR_RO(adc);
//static DEVICE_ATTR_RO(adcnc);
//static DEVICE_ATTR_RO(adctest);
static DEVICE_ATTR_RO(bulktest);
//static DEVICE_ATTR_RO(reset);


static void remove_sysfs_attrs(struct usb_interface *interface)
{

//			device_remove_file(&interface->dev, &dev_attr_adc);
//			device_remove_file(&interface->dev, &dev_attr_adcnc);
//			device_remove_file(&interface->dev, &dev_attr_adctest);
			device_remove_file(&interface->dev, &dev_attr_bulktest);
//			device_remove_file(&interface->dev, &dev_attr_reset);
}


static struct usb_class_driver skel_class = {
	.name =		"skel%d",
	.fops =		&skel_fops,
	.minor_base =	SKEL_MINOR_BASE,
};


const char *gpio_names[] = { "LED", "usbGPIO2", "BTN", "usbGPIO4", "IRQpin" };

int runm = 0;
//called when a usb device is connected to PC
static int
skel_probe(struct usb_interface *interface,
             const struct usb_device_id *id)
{
   struct usb_device *udev = interface_to_usbdev(interface);
   struct usb_host_interface *iface_desc;
   struct usb_endpoint_descriptor *endpoint, *int_in, *int_out;
   struct skel *data;
//   struct skel *pwmd;
//   struct gpio_irq_chip *girq;
   int i;
   int inf;
//   int err;
   int rc;
//   int ret;
//   int actual_len;
   int retval;
//   unsigned char *buf;
   
   printk(KERN_INFO "manufacturer: %s \n", udev->manufacturer);
   printk(KERN_INFO "product: %s \n", udev->product);

   iface_desc = interface->cur_altsetting;
   printk(KERN_INFO "vusb led %d probed: (%04X:%04X) \n",
          iface_desc->desc.bInterfaceNumber, id->idVendor, id->idProduct);
   printk(KERN_INFO "bNumEndpoints: %d \n", iface_desc->desc.bNumEndpoints);

   data = kzalloc(sizeof(struct skel), GFP_KERNEL);
   if (data == NULL)
     {
        dev_info(&interface->dev, "error kzalloc\n");
     }

    dev_info(&interface->dev, "after kzalloc\n");


   for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
     {
     endpoint = &iface_desc->endpoint[i].desc;
     if (i == 0) {
        int_out = &iface_desc->endpoint[i].desc;
        data->int_out = int_out;
//        data->int_out->bEndpointAddress = int_out->bEndpointAddress;
        data->int_out_endpointAddr = endpoint->bEndpointAddress;
     }
     if (i == 1) {
        int_in = &iface_desc->endpoint[i].desc;
        data->int_in = int_in;
        data->int_in_endpointAddr = endpoint->bEndpointAddress;
        data->int_in->wMaxPacketSize = endpoint->wMaxPacketSize;
        data->int_in->bmAttributes = endpoint->bmAttributes;
         }
//     endpoint = &iface_desc->endpoint[i].desc;
        printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n",
               i, endpoint->bEndpointAddress);
        printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n",
               i, endpoint->bmAttributes);
        printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n",
               i, endpoint->wMaxPacketSize, endpoint->wMaxPacketSize);
     }

    kref_init(&data->kref);
	sema_init(&data->limit_sem, WRITES_IN_FLIGHT);
	mutex_init(&data->io_mutex);
	spin_lock_init(&data->err_lock);
	init_usb_anchor(&data->submitted);
	init_waitqueue_head(&data->int_in_wait);
	
   dev_info(&interface->dev, "if5 start\n");
   
   	data->int_in_size = usb_endpoint_maxp(int_in);
   	dev_info(&interface->dev, "after size\n");
//	data->int_in_endpointAddr = int_in->bEndpointAddress;
	dev_info(&interface->dev, "after ep addr\n");
	data->int_in_buf = kmalloc(data->int_in_size, GFP_KERNEL);
	if (!data->int_in_buf) {
		retval = -ENOMEM;
		goto error;
	}
	dev_info(&interface->dev, "after buf kmalloc\n");
//   data->int_in = endpoint;
//   data->int_out = int_out;
 //  data->int_in->bEndpointAddress = int_in->bEndpointAddress;
//   data->int_out->bEndpointAddress = int_out->bEndpointAddress;
   data->udev = usb_get_dev(udev);
//   data->udev = usb_get_dev(interface_to_usbdev(interface));
   dev_info(&interface->dev, "after udev\n");
   //int_out_endpointAddr
   // allocate our urb for interrupt in 
   data->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
   dev_info(&interface->dev, "after urb  malloc\n");
   //allocate the interrupt buffer to be used
   data->int_in_buf = kmalloc(le16_to_cpu(data->int_in->wMaxPacketSize), GFP_KERNEL);
   dev_info(&interface->dev, "after buf  kmalloc\n");
//   data->int_in_buf = usb_alloc_coherent(udev, le16_to_cpu(data->int_in->wMaxPacketSize), GFP_KERNEL,
	//			 &data->int_in_urb->transfer_dma);
//   data->int_in_buf = "A1, 00";
//usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
//				 &urb->transfer_dma);
   //initialize our interrupt urb
   //notice the rcvintpippe -- it is for recieving data from device at interrupt endpoint
   printk(KERN_INFO "int in ->bEndpointAddress: 0x%02X\n",
               data->int_in_endpointAddr);
    printk(KERN_INFO "int in ->bEndpointAddress: 0x%02X\n",
               data->int_in->bEndpointAddress);
   usb_fill_int_urb(data->int_in_urb, data->udev,
                    usb_rcvintpipe(data->udev, 85),
                    data->int_in_buf,
                    le16_to_cpu(data->int_in->wMaxPacketSize),
                    int_cb, // this callback is called when we are done sending/recieving urb
                    data,
                    3000);
   data->int_in_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
   usb_set_intfdata(interface, data);

   
   printk(KERN_INFO "sending gpio irq urb \n");
   i = usb_submit_urb(data->int_in_urb, GFP_KERNEL);
   if (i)
     {
        printk(KERN_ALERT "Failed to submit urb \n");
     }

//    usb_reset_endpoint(udev, data->int_in->bEndpointAddress + USB_DIR_IN);

    printk(KERN_INFO "usb gpio irq is connected \n");
//    bulk_in->bEndpointAddress = 86;


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
//   data->chip.to_irq = i2c_gpio_to_irq;
   data->chip.names = gpio_names;
     
      if (gpiochip_add(&data->chip) < 0)
     {
        printk(KERN_ALERT "Failed to add gpio chip \n");
     }
   else
     {
        printk(KERN_INFO "Able to add gpiochip: %s \n", data->chip.label);
     }
     
//   retval = device_create_file(&interface->dev, &dev_attr_adc);
//   if (retval) {
//     goto error_create_file;
//  }   

//   retval = device_create_file(&interface->dev, &dev_attr_adcnc);
//   if (retval) {
//    goto error_create_file;
 // }

   retval = device_create_file(&interface->dev, &dev_attr_bulktest);
   if (retval) {
     goto error_create_file;
  }  
  
//   retval = device_create_file(&interface->dev, &dev_attr_reset);
//   if (retval) {
//     goto error_create_file;
//  }
  
 //  retval = device_create_file(&interface->dev, &dev_attr_adctest);
 //  if (retval) {
//     goto error_create_file;
//    }   
//  
//   printk(KERN_INFO "usb gpio irq is connected \n");
usb_set_intfdata(interface, data);



	data->udev = usb_get_dev(interface_to_usbdev(interface));
	data->interface = interface;

/*
 	retval = usb_find_common_endpoints(interface->cur_altsetting,
			&int_in, &int_out, NULL, NULL);
	if (retval) {
		dev_err(&interface->dev,
			"Could not find both bulk-in and bulk-out endpoints\n");
		goto error;
	}
	*/
    inf = data->interface->cur_altsetting->desc.bInterfaceNumber;
    
    dev_info(&interface->dev, "after get usbdev\n");



   

	retval = usb_register_dev(interface, &skel_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev,
		 "USB skel device now attached to skel-%d \n",
		 interface->minor);
	

//    custset(&interface->dev);

   printk(KERN_INFO "usb device is connected \n");

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
/*	buf = kmalloc(64, GFP_NOIO);
	if (!buf)
		goto out_buf;

    i = 0;
     
   	for (i = 0; i < 2; i++) {
		ret = usb_bulk_msg(udev, usb_rcvbulkpipe(udev, data->int_in2->bEndpointAddress),
				   buf, 64, &actual_len,
				   1000);
		if (ret < 0)
			goto out_buf;
	}  
    */ 
   
   return 0;
   
error_create_file:
 //  kfree(buf);
   usb_put_dev(udev);
   usb_set_intfdata(interface, NULL);
   return -1; 
 
error:
	/* this frees allocated memory */
	kref_put(&data->kref, skel_delete);

	return retval;  //out_buf:
//	kfree(buf);
 //   return ret;
}

//called when unplugging a USB device
static void
skel_disconnect(struct usb_interface *interface)
{
   struct skel *data;
//   struct skel *pwmd;

   data = usb_get_intfdata(interface);
   usb_set_intfdata(interface, NULL);

   cancel_work_sync(&data->work);
   cancel_work_sync(&data->work2);
	/* give back our minor */
	usb_deregister_dev(interface, &skel_class);
	
   	mutex_lock(&data->io_mutex);
	data->disconnected = 1;
	mutex_unlock(&data->io_mutex);

//   usb_kill_urb(data->int_in_urb);
//   usb_kill_urb(data->int_in_urb);
   usb_kill_urb(data->int_out_urb);

   remove_sysfs_attrs(interface);
   gpiochip_remove(&data->chip);

   kref_put(&data->kref, skel_delete);



   
//   usb_free_urb(data->int_in_urb);
//   kfree(data->int_in_buf);
//   usb_free_urb(data->bulk_in_urb);
   kfree(data->int_out_buf);
    kfree(data->int_in_buf);
//   kfree(data->bufr);
//   irq_free_descs(data->irq_base, data->irq_num);

//   module_put(THIS_MODULE);

   usb_put_intf(data->interface);
   usb_put_dev(data->udev);
//   usb_put_dev(pwmd->udev);
   kfree(data); //deallocate, allocated by kzmalloc()

   printk(KERN_INFO "usb device is disconnected \n");
}


//we could use module_usb_driver(skel_driver); instead of 
// init and exit functions
//called on module loading
static int __init
_usb_init(void)
{
   int result;
   printk(KERN_INFO "usb driver is loaded \n");

   result = usb_register(&skel_driver);
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

static void skel_draw_down(struct skel *dev)
{
	int time;

	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&dev->submitted);
	usb_kill_urb(dev->int_in_urb);
}

static int skel_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct skel *dev = usb_get_intfdata(intf);

	if (!dev)
		return 0;
	skel_draw_down(dev);
	return 0;
}

static int skel_resume(struct usb_interface *intf)
{
	return 0;
}

static int skel_pre_reset(struct usb_interface *intf)
{
	struct skel *dev = usb_get_intfdata(intf);

	mutex_lock(&dev->io_mutex);
	skel_draw_down(dev);

	return 0;
}

static int skel_post_reset(struct usb_interface *intf)
{
	struct skel *dev = usb_get_intfdata(intf);

	/* we are sure no URBs are active - no locking needed */
	dev->errors = -EPIPE;
	mutex_unlock(&dev->io_mutex);

	return 0;
}
static struct usb_driver skel_driver = {
     .name = "my first usb driver",
     .id_table = skel_table,
     .probe = skel_probe,
     .disconnect = skel_disconnect,
     .suspend =	skel_suspend,
	 .resume =	skel_resume,
	 .pre_reset =	skel_pre_reset,
	 .post_reset =	skel_post_reset,
	 .supports_autosuspend = 1,
};



module_usb_driver(skel_driver);

MODULE_LICENSE("GPL v2");
