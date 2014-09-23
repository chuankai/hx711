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
static int power, value, config, raw;
// Hardcode the gpio pins for now...
static unsigned int dout_pin = 7;
static unsigned int pd_sck_pin = 8;
static int irq;
static DEFINE_MUTEX(data_retrieve_mutex);


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
		ret = gpio_request_one(dout_pin, GPIOF_DIR_IN, "hx711_data") || gpio_request_one(pd_sck_pin, GPIOF_DIR_IN, "hx711_clk");
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

	return 0;
}

#define DATA_BIT_LENGTH 24
static irqreturn_t dout_irq_handler(int irq, void *dev)
{
	int ret, val, pulses, i;

	mutex_lock(&data_retrieve_mutex);

	pulses = DATA_BIT_LENGTH + config;
	val = 0;

	for (i = 0; i < pulses * 2; i++) {
		usleep_range(1, 2);
		if ( i & 1) {
			if (i < 48)
				val =  (val << 1) + gpio_get_value(dout_pin);
			gpio_set_value(dout_pin, 0);
		} else {
			gpio_set_value(dout_pin, 1);
		}
	}
	value = val;

	printk(KERN_INFO "Value: %06x\n", val);
	ret = gpio_request_one(dout_pin, GPIOF_DIR_IN, "hx711_data") || gpio_request_one(pd_sck_pin, GPIOF_OUT_INIT_HIGH, "hx711_clk");
	if (ret)
		printk(KERN_INFO "GPIO request failed\n");

	mutex_unlock(&data_retrieve_mutex);

	return IRQ_HANDLED;
}

static int hx711_power(int on)
{
	int ret;

	mutex_lock(&data_retrieve_mutex);
	if (power == on)
		return 0;


	if (on) {
		ret = request_threaded_irq(irq, NULL, dout_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_FALLING, "hx711", NULL);
		if (ret) {
			printk(KERN_INFO "IRQ request failed\n");
			goto EXIT;
		}
	} else {
		free_irq(irq, NULL);
	}

	gpio_set_value(pd_sck_pin, !on);
	mutex_unlock(&data_retrieve_mutex);

EXIT:
	return ret;
}

// Skip the driver and device binding for now. Do everything here.
static int hx711_init(void)
{
	int ret;

	value = 0;
	power = 0;
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
		goto EXIT;
	}

	hx711_power(power);

EXIT:
	gpio_free(dout_pin);
	gpio_free(pd_sck_pin);
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
	int r, g;

        sscanf(buf, "%d %d", &r, &g);

        return count;
}

static DRIVER_ATTR_WO(calib);

static ssize_t power_show(struct device_driver *drv, char *buf)
{
        return sprintf(buf, "%d\n", power);
}

static ssize_t power_store(struct device_driver *drv, const char *buf, size_t count)
{
        sscanf(buf, "%d", &power);
	hx711_power(power);
        return count;
}

static DRIVER_ATTR_RW(power);	//struct driver_attribute driver_attr_power

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", value);
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

	//platform_driver_unregister(&hx711_driver);	TODO:kernel complains about unexpected unload
}  

module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR("Chuankai Lin <chuankai@kai.idv.tw>"); 
MODULE_LICENSE("Dual BSD/GPL"); 
MODULE_DESCRIPTION("hx711 driver");

