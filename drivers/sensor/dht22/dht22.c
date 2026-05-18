#define DT_DRV_COMPAT aosong_dht22

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>


#define DHT22_RST_LOW_DELAY_US   18000
#define DHT22_RST_HIGH_DELAY_US  40
#define DHT22_BIT_DELAY_US       28
#define DHT22_RETRY_MAX          200

struct dht22_config {
	struct gpio_dt_spec gpio;
};

struct dht22_data {
	float temp;
	float humi;
};

static void dht22_set_pin_output(const struct gpio_dt_spec *gpio)
{
	gpio_pin_configure_dt(gpio, GPIO_OUTPUT);
}

static void dht22_set_pin_input(const struct gpio_dt_spec *gpio)
{
	gpio_pin_configure_dt(gpio, GPIO_INPUT);
}

static void dht22_idle(const struct gpio_dt_spec *gpio)
{
	dht22_set_pin_output(gpio);
	gpio_pin_set_dt(gpio, 1);
	k_busy_wait(1000);
}

static void dht22_reset(const struct gpio_dt_spec *gpio)
{
	dht22_idle(gpio);
	dht22_set_pin_output(gpio);
	gpio_pin_set_dt(gpio, 0);
	k_busy_wait(DHT22_RST_LOW_DELAY_US);
	gpio_pin_set_dt(gpio, 1);
	k_busy_wait(DHT22_RST_HIGH_DELAY_US);
}

static uint8_t dht22_check_ack(const struct gpio_dt_spec *gpio)
{
	uint8_t retry = 0;
	dht22_set_pin_input(gpio);

	while (gpio_pin_get_dt(gpio) && retry < DHT22_RETRY_MAX) {
		retry++;
		k_busy_wait(1);
	}
	if (retry >= DHT22_RETRY_MAX) return 1;

	retry = 0;
	while (!gpio_pin_get_dt(gpio) && retry < DHT22_RETRY_MAX) {
		retry++;
		k_busy_wait(1);
	}
	if (retry >= DHT22_RETRY_MAX) return 1;

	return 0;
}

static uint8_t dht22_read_bit(const struct gpio_dt_spec *gpio)
{
	uint8_t retry = 0;

	while (gpio_pin_get_dt(gpio) && retry < DHT22_RETRY_MAX) {
		retry++;
		k_busy_wait(1);
	}
	retry = 0;

	while (!gpio_pin_get_dt(gpio) && retry < DHT22_RETRY_MAX) {
		retry++;
		k_busy_wait(1);
	}
	k_busy_wait(DHT22_BIT_DELAY_US);

	return gpio_pin_get_dt(gpio) ? 1 : 0;
}

static uint8_t dht22_read_byte(const struct gpio_dt_spec *gpio)
{
	uint8_t byte_val = 0;
	for (int i = 0; i < 8; i++) {
		byte_val <<= 1;
		byte_val |= dht22_read_bit(gpio);
	}
	return byte_val;
}

static int dht22_read(const struct device *dev)
{
	const struct dht22_config *cfg = dev->config;
	struct dht22_data *data = dev->data;
	uint8_t buf[5] = {0};

	dht22_reset(&cfg->gpio);
	if (dht22_check_ack(&cfg->gpio) != 0) {
		return -EIO;
	}

	for (int i = 0; i < 5; i++) {
		buf[i] = dht22_read_byte(&cfg->gpio);
	}

	data->humi = (float)((buf[0] << 8) + buf[1]) / 10.0f;
	data->temp = (buf[2] & 0x80) ? 
		-1.0f * (float)(((buf[2] & 0x7F) << 8) + buf[3]) / 10.0f :
		(float)((buf[2] << 8) + buf[3]) / 10.0f;

	return 0;
}

static int dht22_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	ARG_UNUSED(chan);
	return dht22_read(dev);
}

static int dht22_channel_get(const struct device *dev,
			     enum sensor_channel chan,
			     struct sensor_value *val)
{
	struct dht22_data *data = dev->data;

	if (chan == SENSOR_CHAN_HUMIDITY) {
		val->val1 = (int32_t)data->humi;
		val->val2 = (int32_t)(data->humi * 100000) % 100000;
	} else if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
		val->val1 = (int32_t)data->temp;
		val->val2 = (int32_t)(data->temp * 100000) % 100000;
	}
	return 0;
}

static const struct sensor_driver_api dht22_api = {
	.sample_fetch = dht22_sample_fetch,
	.channel_get = dht22_channel_get,
};

static int dht22_init(const struct device *dev)
{
	const struct dht22_config *cfg = dev->config;
	if (!device_is_ready(cfg->gpio.port)) return -ENODEV;
	dht22_idle(&cfg->gpio);
	return 0;
}

#define DHT22_INIT(inst) \
	static struct dht22_data data_##inst; \
	static struct dht22_config config_##inst = { \
		.gpio = GPIO_DT_SPEC_INST_GET(inst, gpios), \
	}; \
	DEVICE_DT_INST_DEFINE(inst, \
			      dht22_init, \
			      NULL, \
			      &data_##inst, \
			      &config_##inst, \
			      POST_KERNEL, \
			      CONFIG_SENSOR_INIT_PRIORITY, \
			      &dht22_api);

DT_INST_FOREACH_STATUS_OKAY(DHT22_INIT)