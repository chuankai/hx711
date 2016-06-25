#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

enum {
	CONFIG_CHANNEL_A_GAIN_128 = 1,
	CONFIG_CHNNNEL_B_GAIN_32,
	CONFIG_CHANNEL_A_GAIN_64,
};

struct raw_gram_map {
	int raw;
	int gram;
};

#define NUM_OF_CALIB_POINTS	5

static struct raw_gram_map raw_gram_maps[NUM_OF_CALIB_POINTS];
static int power, config, raw;
// Hardcode the gpio pins for now...
static unsigned int dout_pin = 7;
static unsigned int pd_sck_pin = 8;
static int irq;


static void hx711_power(int);

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
	if (!ret) {
		ret = gpio_request_one(dout_pin, GPIOF_DIR_IN, "hx711_data") || gpio_request_one(pd_sck_pin, GPIOF_OUT_INIT_HIGH, "hx711_clk");
		if (ret) {
			printk(KERN_INFO "GPIO request failed\n");
			gpio_free(dout_pin);
			gpio_free(pd_sck_pin);
			
			return -EINVAL;
		}
	}
	
	return ret;
}

static int hx711_remove(struct platform_device *pdev)
{
	gpio_free(dout_pin);
	gpio_free(pd_sck_pin);
	free_irq(irq, NULL);

	return 0;
}

#define DATA_BIT_LENGTH 24
static irqreturn_t dout_irq_handler(int irq, void *dev)
{
	int _raw, pulses, i;

	pulses = DATA_BIT_LENGTH + config;
	_raw = 0;

	for (i = 0; i < pulses * 2; i++) {
		udelay(1);
		if ( i & 1) {
			if (i < 48)
				_raw =  (_raw << 1) + gpio_get_value(dout_pin);
			gpio_set_value(pd_sck_pin, 0);
		} else {
			gpio_set_value(pd_sck_pin, 1);
		}
	}
	raw = _raw ^ (1 << 23);

	if (!power)
		hx711_power(0);

	return IRQ_HANDLED;
}

static void hx711_power(int onoff)
{
	if (!!onoff != power) {
		if (onoff) {
			enable_irq(irq);
			gpio_set_value(pd_sck_pin, 0);
		} else {
			disable_irq(irq);
			gpio_set_value(pd_sck_pin, 1);
		}
		power = !!onoff;
	}
}

// Skip the driver and device binding for now. Do everything here.
static int __init hx711_init(void)
{
	int ret;

	raw = 0;
	config = CONFIG_CHANNEL_A_GAIN_64;

	ret = gpio_request_one(dout_pin, GPIOF_DIR_IN, "hx711_data") || gpio_request_one(pd_sck_pin, GPIOF_OUT_INIT_HIGH, "hx711_clk");
	if (ret) {
		printk(KERN_INFO "GPIO request failed\n");
		ret = -EINVAL;
		goto EXIT;
	}

	irq = gpio_to_irq(dout_pin);
	if (irq < 0) {
		printk(KERN_INFO "IRQ number no available\n");
		ret = -EINVAL;
		goto CLEANUP;
	}

	ret = request_threaded_irq(irq, NULL, dout_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_FALLING, "hx711", NULL);
	if (ret) {
		printk(KERN_INFO "IRQ request failed\n");
		goto CLEANUP;
	}
	disable_irq(irq);
	hx711_power(0);

	goto EXIT;

CLEANUP:
	gpio_free(dout_pin);
	gpio_free(pd_sck_pin);
EXIT:
	return ret;
}

static ssize_t config_show(struct device_driver *drv, char *buf)
{
        return sprintf(buf, "%d\n", config);
}

static ssize_t config_store(struct device_driver *drv, const char *buf, size_t count)
{
        sscanf(buf, "%d", &config);
        return count;
}

static DRIVER_ATTR_RW(config);

static ssize_t calib_store(struct device_driver *drv, const char *buf, size_t count)
{
        sscanf(buf, "%d %d %d %d %d %d %d %d %d %d", 	&raw_gram_maps[0].raw, &raw_gram_maps[0].gram,\
							&raw_gram_maps[1].raw, &raw_gram_maps[1].gram,\
							&raw_gram_maps[2].raw, &raw_gram_maps[2].gram,\
							&raw_gram_maps[3].raw, &raw_gram_maps[3].gram,\
							&raw_gram_maps[4].raw, &raw_gram_maps[4].gram);

        return count;
}

static ssize_t calib_show(struct device_driver *drv, char *buf)
{
        return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d\n", 	raw_gram_maps[0].raw, raw_gram_maps[0].gram,\
								raw_gram_maps[1].raw, raw_gram_maps[1].gram,\
								raw_gram_maps[2].raw, raw_gram_maps[2].gram,\
								raw_gram_maps[3].raw, raw_gram_maps[3].gram,\
								raw_gram_maps[4].raw, raw_gram_maps[4].gram);
}

static DRIVER_ATTR_RW(calib);

static ssize_t power_show(struct device_driver *drv, char *buf)
{
        return sprintf(buf, "%d\n", power);
}

static ssize_t power_store(struct device_driver *drv, const char *buf, size_t count)
{
	int p;

        sscanf(buf, "%d", &p);
	hx711_power(!!p);

        return count;
}

static DRIVER_ATTR_RW(power);	//struct driver_attribute driver_attr_power

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if 0	// Do the interpolation in user space
	int l, h, key, value;

	l = 0;
	h = NUM_OF_CALIB_POINTS - 1; 
	key = raw;

	// binary search
	while (h - l > 1) {
		if (key <= raw_gram_maps[h].raw)
			h = (l + h) / 2;
		else 
			l = (l + h) / 2;
	}

	value = raw_gram_maps(l).gram + (raw_gram_maps(h).gram - raw_gram_maps(l).gram) / (  
#endif
        return sprintf(buf, "%d\n", 0);
}

static DRIVER_ATTR_RO(value);

static ssize_t raw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", raw);
}

static DRIVER_ATTR_RO(raw);

static struct attribute *hx711_attrs[] = {
	&driver_attr_power.attr,
	&driver_attr_value.attr,
	&driver_attr_raw.attr,
	&driver_attr_config.attr,
	&driver_attr_calib.attr,
	NULL
};

ATTRIBUTE_GROUPS(hx711);	//contruct attribute_groups *hx711_groups[]

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
		.groups = hx711_groups,
	},
	.probe = hx711_probe,
	.remove = hx711_remove,
};

static int __init mod_init(void)
{
	int ret;

	printk(KERN_INFO "hx711 module being loaded\n");

	ret = hx711_init();
	if (ret) {
		printk(KERN_INFO "hx711 init failed\n");
		goto EXIT;
	}
	ret = platform_driver_register(&hx711_driver);
	if (ret)
		printk(KERN_INFO "hx711 driver registration failed\n");

EXIT:
	return ret;
}  

static void __exit mod_exit(void) 
{
	printk(KERN_INFO "hx7111 module being unloaded\n");

	gpio_free(dout_pin);
	gpio_free(pd_sck_pin);
	free_irq(irq, NULL);
	platform_driver_unregister(&hx711_driver);
}  

module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR("Chuankai Lin <chuankai@kai.idv.tw>"); 
MODULE_LICENSE("Dual BSD/GPL"); 
MODULE_DESCRIPTION("hx711 driver");

