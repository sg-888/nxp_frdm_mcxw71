#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#define DHT22_NODE DT_INST(0, aosong_dht22)
const struct device *dht22 = DEVICE_DT_GET(DHT22_NODE);

int main(void)
{
	struct sensor_value temp, humi;

	printk("*** Booting Zephyr OS build v4.2.0 ***\n");
	printk("DHT22 设备初始化成功\n");

	while (1) {
		int ret = sensor_sample_fetch(dht22);
		if (ret == 0) {
			sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humi);

			printk("温度: %d.%d°C | 湿度: %d.%d%%RH\n",
			       temp.val1, temp.val2 / 100000,
			       humi.val1, humi.val2 / 100000);
		}
		else {
			printk("读取失败，错误码: %d\n", ret);
		}

		k_sleep(K_SECONDS(2));
	}
}