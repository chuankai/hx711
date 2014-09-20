#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <kobject.h>
#include <sysfs.h>

#define DOUT_GPIO_NUM		7
#define PD_SCK_GPIO_NUM		8

static int power;
static int value;
static int calib;

static ssize_t power_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", foo);
}

static ssize_t power_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
        sscanf(buf, "%du", &foo);
        return count;
}

static struct kobj_attribute power_attr = __ATTR_RW(power);

ATTRIBUTE_GROUPS(hx711);

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *kobj;

static int __init mod_init(void)
{
	int err;

	struct gpio gs[] = {	{DOUT_GPIO_NUM, GPIOF_DIR_IN, 'hx711_data'},
				(PD_SCK_GPIO_NUM, GPIOF_OUT_INIT_HIGH, 'hx711_clk'} };

	printk(KERN_INFO "hx711 module being loaded\n");

	err = gpio_request_array(gs, 2);
	if (err) {
		printk(KERN_INFO "GPIO request failed\n");
		return -EBUSY;
	}

	kobj = kobject_create_and_add("hx711", kernel_kobj);
	if (!kobj) {
		printk(KERN_INFO "kobject creation failed\n");
		return -ENOMEM;
	}

	return 0;
}  

static void __exit mod_exit(void) 
{
	printk(KERN_INFO "hx7111 module being unloaded\n"); 
}  

module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR("Chuankai Lin <chuankai@kai.idv.tw>"); 
MODULE_LICENSE("Dual BSD/GPL"); 
MODULE_DESCRIPTION("hx711 driver");
