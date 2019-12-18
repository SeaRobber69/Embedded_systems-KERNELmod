
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michiel Vervenne");

static int inputBtn = 14; //Button input default 14
static int button_irq = -1;
static int arr_argc = 0;
int counter = 0;
/*
* module_param(foo, int, 0000)
* The first param is the parameters name
* The second param is it's data type
* The final argument is the permissions bits,
* for exposing parameters in sysfs (if non−zero) at a later stage.
*/

module_param(inputBtn, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(inputBtn, "(int) Input button");

/*
* module_param_array(name, type, num, perm);
* The first param is the parameter's (in this case the array's) name
* The second param is the data type of the elements of the array
* The third argument is a pointer to the variable that will store the number
* of elements of the array initialized by the user at module loading time
* The fourth argument is the permission bits
*/

static irqreturn_t button_isr(int irq, void *data)
{
    // Counter optellen als er een interupt op de knop binnenkomt
    if(irq == button_irq) {
        counter++;
    }

    printk(KERN_INFO "Current counter value: %d\n", counter);
    return IRQ_HANDLED;
}


static int __init btnCounter_init(void)
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

static void __exit btnCounter_exit(void)
{
    int i;

    printk(KERN_INFO "%s\n", __func__);

    // free irqs
    free_irq(button_irq, NULL);

    // unregister
    gpio_free(inputBtn);

    printk(KERN_INFO "Giving button freedom back!\n");
}

module_init(btnCounter_init);
module_exit(btnCounter_exit);
