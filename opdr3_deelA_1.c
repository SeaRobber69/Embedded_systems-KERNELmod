
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michiel Vervenne");

static int time = 1;
static int outputs[2] = { -1, -1 };
static long data=0;
static struct timer_list blink_timer;
static int arr_argc = 0;
/*
* module_param(foo, int, 0000)
* The first param is the parameters name
* The second param is it's data type
* The final argument is the permissions bits,
* for exposing parameters in sysfs (if non−zero) at a later stage.
*/

module_param(time, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(time, "(int) Time to toggle on (S)");

module_param_array(outputs, int, &arr_argc, 0000);
MODULE_PARM_DESC(outputs, "(int[]) Output numbers. max 2");

/*
* module_param_array(name, type, num, perm);
* The first param is the parameter's (in this case the array's) name
* The second param is the data type of the elements of the array
* The third argument is a pointer to the variable that will store the number
* of elements of the array initialized by the user at module loading time
* The fourth argument is the permission bits
*/
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
        blink_timer.expires = jiffies + (time*HZ);                 // 1 sec.
        add_timer(&blink_timer);
}

static int __init toggleIO_init(void)
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
    blink_timer.expires = jiffies + (time*HZ);                 // Default: 1 sec.
    add_timer(&blink_timer);

    return ret;
}

static void __exit toggleIO_exit(void)
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

module_init(toggleIO_init);
module_exit(toggleIO_exit);
