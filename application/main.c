#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#define DEVICE_NAME "MCXW71_TH"
#define DHT22_NODE DT_INST(0, aosong_dht22)

#define BT_UUID_TH_SERVICE  BT_UUID_DECLARE_16(0xFFF0)
#define BT_UUID_TH_CHAR     BT_UUID_DECLARE_16(0xFFF1)

static struct bt_conn *g_conn = NULL;
const struct device *dht22 = DEVICE_DT_GET(DHT22_NODE);

static bool notify_subscribed = false;

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_subscribed = (value == BT_GATT_CCC_NOTIFY);
    printk("📱 手机订阅状态: %s\n", notify_subscribed ? "已订阅" : "未订阅");
}

static ssize_t th_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, uint16_t len, uint16_t offset)
{
    uint8_t dummy[4] = {0};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, dummy, sizeof(dummy));
}

BT_GATT_SERVICE_DEFINE(th_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_TH_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_TH_CHAR,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        th_read_cb, NULL, NULL),
    BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, sizeof(DEVICE_NAME)-1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("连接失败\n");
        return;
    }
    g_conn = conn;
    printk("蓝牙连接成功！\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    g_conn = NULL;
    notify_subscribed = false;
    printk("蓝牙断开连接\n");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void read_dht22(void)
{
    struct sensor_value temp, humi;
    uint8_t th_data[4];

    if (!device_is_ready(dht22)) return;
    if (sensor_sample_fetch(dht22) != 0) return;

    sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humi);

    printk("温度: %d.%d°C | 湿度: %d.%d%%RH\n",
           temp.val1, (temp.val2 / 10000) % 100,
           humi.val1, (humi.val2 / 10000) % 100);

    if (g_conn && notify_subscribed) {
        th_data[0] = temp.val1;
        th_data[1] = (temp.val2 / 10000) % 100;
        th_data[2] = humi.val1;
        th_data[3] = (humi.val2 / 10000) % 100;
        bt_gatt_notify(g_conn, &th_service.attrs[1], th_data, sizeof(th_data));
    }
}

static void bt_ready(int err)
{
    if (err) {
        printk("蓝牙初始化失败\n");
        return;
    }

    printk("蓝牙初始化完成\n");
    printk("可连接广播运行中\n");

    const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_CONN,
        BT_GAP_ADV_FAST_INT_MIN_2,
        BT_GAP_ADV_FAST_INT_MAX_2,
        NULL
    );
    bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
}

static void bt_delayed_init(struct k_work *work)
{
    printk("初始化蓝牙中...\n");
    bt_enable(bt_ready);
}
K_WORK_DELAYABLE_DEFINE(bt_work, bt_delayed_init);

int main(void)
{
    printk("===================================\n");
    printk("*** Booting Zephyr OS v4.2.0 ***\n");
    printk("===================================\n");

    k_work_schedule(&bt_work, K_SECONDS(1));

    while (1) {
        printk("程序运行中...\n");
        read_dht22();
        k_sleep(K_SECONDS(2));
    }
}