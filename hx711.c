#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>


static int power, value;
static unsigned int dout_pin, pd_sck_pin;

#if 0
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
#endif

static int of_get_gpio_pins(struct device_node *np, unsigned int *_dout_pin, unsigned int *_pd_sck_pin)
{
        if (of_gpio_count(np) < 2)
		return -ENODEV;
 
        *_dout_pin = of_get_gpio(np, 0);
        *_pd_sck_pin = of_get_gpio(np, 1);
 
	if (!gpio_is_valid(*_dout_pin) || !gpio_is_valid(*_pd_sck_pin)) {
		printk(KERN_INFO "Invalid GPIO pins %d and %d\n", *_dout_pin, *_pd_sck_pin);
		return -ENODEV;
	}
 
        return 0;
}

static int hx711_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_get_gpio_pins(pdev->dev.of_node, &dout_pin, &pd_sck_pin);
	if (ret)
		return ret;
	
	ret = gpio_request_one(dout_pin, GPIOF_DIR_IN, "hx711_data") || gpio_request_one(pd_sck_pin, GPIOF_DIR_IN, "hx711_clk");
	if (ret) {
		printk(KERN_INFO "GPIO request failed\n");
		gpio_free(dout_pin);
		gpio_free(pd_sck_pin);
		return -EINVAL;
	}
}

static int hx711_remove(struct platform_device *pdev)
{
	gpio_free(dout_pin);
	gpio_free(pd_sck_pin);

	return 0;
}

static const struct of_device_id hx711_dt_ids[] = {
	{ .compatible = "hx711", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hx711_dt_ids);

static struct platform_driver hx711_driver = {
	.driver = {
		.name = "hx711",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hx711_dt_ids),
	},
	.probe = hx711_probe,
	.remove = hx711_remove,
};

static int __init mod_init(void)
{
	printk(KERN_INFO "hx711 module being loaded\n");

#if 0
	kobj = kobject_create_and_add("hx711", kernel_kobj);
	if (!kobj) {
		printk(KERN_INFO "kobject creation failed\n");
		return -ENOMEM;
	}
#endif

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
