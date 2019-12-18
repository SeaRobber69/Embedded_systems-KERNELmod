#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h> 
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "query_ioctl.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static int freq = 1, counter = 0;
 

 // Relay 
static int outputs[2] = { 3, 4 }; // output 3 en 4
static long data=0;
static struct timer_list blink_timer;
static int arr_argc = 0;

static void blink_timer_func(struct timer_list* t)
{
        printk(KERN_INFO "%s\n", __func__);

        // Set LED GPIOs output to data: 1|0
        int i = 0;
        for(i = 0; i < ARRAY_SIZE(outputs); i++) {
            if(outputs[i] > 0)
                gpio_set_value(outputs[i], data);
        }
        data=!data;

        /* schedule next execution */
        //blink_timer.data = !data;                                             // makes the LED toggle
        blink_timer.expires = jiffies + (freq*HZ);                 // 1 sec.
        add_timer(&blink_timer);
}

static int toggleIO_init(void)
{
    int ret = 0;

    printk(KERN_INFO "%s\n", __func__);

    // register LED GPIOs, turn LEDs off

    int i = 0;
    for(i = 0; i < ARRAY_SIZE(outputs); i++) {
        if(outputs[i] > 0)
            ret = gpio_request_one(outputs[i], GPIOF_OUT_INIT_LOW, "led");
    }

    if (ret) {
            printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
            return ret;
    }

    /* init timer, add timer function */
    //init_timer(&blink_timer);
        timer_setup(&blink_timer, blink_timer_func, 0);

    blink_timer.function = blink_timer_func;
    //blink_timer.data = 1L;                                                        // initially turn LED on
    blink_timer.expires = jiffies + (freq*HZ);                 // Default: 1 sec.
    add_timer(&blink_timer);

    return ret;
}

static void toggleIO_exit(void)
{
    // deactivate timer if running
    del_timer_sync(&blink_timer);

    // turn LEDs off
    int i = 0;
    for(i = 0; i < ARRAY_SIZE(outputs); i++) {
        if(outputs[i] > 0)
            gpio_set_value(outputs[i], 0);
    }
    
    // unregister GPIOs
    for(i = 0; i < ARRAY_SIZE(outputs); i++) {
        if(outputs[i] > 0)
            gpio_free(outputs[i]);
    }
    
    printk(KERN_INFO "Releasing light!\n");
}

// End Relay

// Button
static int inputBtn = 14; //Button input default 14
static int button_irq = -1;

static irqreturn_t button_isr(int irq, void *data)
{
    // Counter optellen als er een interupt op de knop binnenkomt
    if(irq == button_irq) {
        counter++;
    }

    printk(KERN_INFO "Current counter value: %d\n", counter);
    return IRQ_HANDLED;
}


static int btnCounter_init(void)
{
    int ret = 0;

    printk(KERN_INFO "%s\n", __func__);

    
    if(inputBtn > 0){
        // Register button input
        ret = gpio_request_one(inputBtn, GPIOF_IN, "Input button");

        if (ret) {
                printk(KERN_ERR "Unable to request GPIO: %d\n", ret);
                return ret;
        }

        // Register IRQ on button
        printk(KERN_INFO "Current button value: %d\n", gpio_get_value(inputBtn));

        ret = gpio_to_irq(inputBtn);

        if(ret < 0) {
                printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
                goto fail1;
        }

        button_irq = ret;

        printk(KERN_INFO "Successfully requested BUTTON1 IRQ # %d\n", button_irq);

        // Set up IRQ method for input button
        ret = request_irq(button_irq, button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#Inputbutton", NULL);

        if(ret) {
                printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
                goto fail1;
        }

        // cleanup what has been setup so far
        fail1:
                gpio_free(inputBtn);
    }
    return ret;
}

static void btnCounter_exit(void)
{
    int i;

    printk(KERN_INFO "%s\n", __func__);

    // free irqs
    free_irq(button_irq, NULL);

    // unregister
    gpio_free(inputBtn);

    printk(KERN_INFO "Giving button freedom back!\n");
}

// End button

// ioctl
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    query_arg_t q;
 
    switch (cmd)
    {
        case QUERY_GET_VARIABLES:
            q.freq = freq;
            q.counter = counter;
            if (copy_to_user((query_arg_t *)arg, &q, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            break;
        case QUERY_CLR_VARIABLES:
            counter = 0;
            break;
        case QUERY_SET_VARIABLES:
            if (copy_from_user(&q, (query_arg_t *)arg, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            freq = q.freq;
            //counter = q.counter;
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};
 
static int __init query_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
 
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "query_ioctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &query_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "query")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

    // Relay
    toggleIO_init();

    // Button
    btnCounter_init();
 
    return 0;
}
 
static void __exit query_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);

    // Relay 
    toggleIO_exit();

    // Button
    btnCounter_exit();
}
 
module_init(query_ioctl_init);
module_exit(query_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email_at_sarika-pugs_dot_com> & Michiel Vervenne");
MODULE_DESCRIPTION("Query ioctl() Driver for relay,button counter");

// end ioctl

