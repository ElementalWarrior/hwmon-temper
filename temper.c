#include <linux/debugfs.h>
#include <linux/hid.h>
#include <linux/hwmon.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/unaligned.h>


/*
root@omv:/home/james# cat /sys/kernel/debug/usb/usbmon/1u
ffff8e3674fd03c0 1557669628 S Ii:1:011:2 -115:8 8 <
ffff8e3674f420c0 1557728285 S Io:1:011:2 -115:8 8 = 0186ff01 00000000
ffff8e3674f420c0 1557736022 C Io:1:011:2 0:8 8 >
ffff8e3674fd03c0 1557744049 C Ii:1:011:2 0:8 8 = 54454d50 6572476f
ffff8e3674fd03c0 1557744076 S Ii:1:011:2 -115:8 8 <
ffff8e3674fd03c0 1557808250 C Ii:1:011:2 0:8 8 = 6c645f56 332e3520
ffff8e3674fd03c0 1557808287 S Ii:1:011:2 -115:8 8 <
ffff8e37c6a1b3c0 1558008490 S Io:1:011:2 -115:8 8 = 01803301 00000000
ffff8e37c6a1b3c0 1558016073 C Io:1:011:2 0:8 8 >
ffff8e3674fd03c0 1558024070 C Ii:1:011:2 0:8 8 = 80800a3a 4e200000
ffff8e3674fd03c0 1558024108 S Ii:1:011:2 -115:8 8 <
ffff8e3674fd03c0 1558125338 C Ii:1:011:2 -2:8 0

above is the output from usbmon for the statement: `$ temper --force 3553:A001`

the response for the temper command was: `26.18C`


my code hid_hw_output_report3:
ffff93d302ee7f00 71967030 S Io:1:002:2 -115:8 8 = 01803301 00000000
ffff93d302ee7f00 71981245 C Io:1:002:2 0:8 8 >
ffff93d302ee76c0 71989522 C Ii:1:002:2 0:8 8 = 80800b86 4e200000
ffff93d302ee76c0 71989530 S Ii:1:002:2 -115:8 8 <
ffff93d302ee7f00 74100406 S Ci:1:002:0 s a1 01 0301 0001 0008 8 <
ffff93d302ee7f00 74103785 C Ci:1:002:0 0 0
ffff93d302ee76c0 74104851 C Ii:1:002:2 -2:8 0

my code hid_hw_raw_request:
ffff9b7d049bc3c0 177140343 S Co:1:002:0 s 21 09 0301 0001 0008 8 = 01803301 00000000
ffff9b7d049bc3c0 177143505 C Co:1:002:0 0 8 >
ffff9b7d049bc3c0 179273506 S Ci:1:002:0 s a1 01 0301 0001 0008 8 <
ffff9b7d049bc3c0 179276789 C Ci:1:002:0 0 0
ffff9b7d1538dd80 179277815 C Ii:1:002:2 -2:8 0
*/

#define DRIVER_NAME "temper"
#define STATUS_REPORT_ID		0x01
#define CTRL_REPORT_SIZE	0x08
#define STATUS_UPDATE_INTERVAL		(2 * HZ)	/* In seconds */
#define REQ_TIMEOUT		300

struct pcs_temper_data {
	struct hid_device *hdev;
	int buffer_size;
	u8 *buffer;
	unsigned int updated;
	struct completion wait_input_report;
	struct device *hwmon_dev;
};

static int pcs_temper_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
	struct pcs_temper_data *priv = hid_get_drvdata(hdev);
	
	/* only copy buffer when requested */
	if (completion_done(&priv->wait_input_report))
		return 0;

	printk("received buffer %llx %llx %llx %llx\n", (long long unsigned int)*data, (long long unsigned int)*(data+1), (long long unsigned int)*(data+2), (long long unsigned int)*(data+3));
	short temp = (data[2]<<8) | data[3];
	printk("temp %d\n", temp);
	// this was originally  * 175.72 / 65536 - 46.85, but we can't do floating point math
	printk("temp %d\n", (int)((temp /100)));

	complete(&priv->wait_input_report);
    return 0;
}

static bool should_load(struct hid_device *hdev) {

	// borrowed from hid_connect to determine type
	static const char *types[] = { "Device", "Pointer", "Mouse", "Device",
		"Joystick", "Gamepad", "Keyboard", "Keypad",
		"Multi-Axis Controller"
	};
	
	const char* type = "Device";
	int i;
	for (i = 0; i < hdev->maxcollection; i++) {
		struct hid_collection *col = &hdev->collection[i];
		if (col->type == HID_COLLECTION_APPLICATION &&
		   (col->usage & HID_USAGE_PAGE) == HID_UP_GENDESK &&
		   (col->usage & 0xffff) < ARRAY_SIZE(types)) {
			type = types[col->usage & 0xffff];
			break;
		}
	}

	return strcmp(type, "Keyboard") != 0;
}

static umode_t pcs_temper_is_visible(const void *data, enum hwmon_sensor_types type,
			      u32 attr, int channel) {
	return 0444;
}

static int ccp_read(struct device *dev, enum hwmon_sensor_types type,
		    u32 attr, int channel, long *val)
{
	struct rog_ryujin_data *priv = dev_get_drvdata(dev);
	printk("ccp_read called.\n");
	return 0;
}

static int ccp_write(struct device *dev, enum hwmon_sensor_types type,
		    u32 attr, int channel, long *val) {
	struct rog_ryujin_data *priv = dev_get_drvdata(dev);
	printk("ccp_write called.\n");
	return 0;
}

static const struct hwmon_ops temper_hwmon_ops = {
	.is_visible = pcs_temper_is_visible,
	.read = ccp_read,
	// .write = ccp_write,
};

static const struct hwmon_channel_info *temper_info[] = {
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT),
	NULL
};

static const struct hwmon_chip_info temper_chip_info = {
	.ops = &temper_hwmon_ops,
	.info = temper_info,
};

static int pcs_temper_probe(struct hid_device *hdev, const struct hid_device_id *id) {

	struct pcs_temper_data *priv;
	int ret;

	// if (!should_load(hdev))
	// 	goto fail_and_close;

	priv = devm_kzalloc(&hdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->hdev = hdev;
	hid_set_drvdata(hdev, priv);

	priv->updated = jiffies - STATUS_UPDATE_INTERVAL;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto fail_and_stop;
	
	hid_device_io_start(hdev);
	init_completion(&priv->wait_input_report);


	priv->hwmon_dev = hwmon_device_register_with_info(&hdev->dev, "temper",
							  priv, &temper_chip_info, NULL);


    priv->buffer_size = CTRL_REPORT_SIZE;
	int i = 0;
	priv->buffer = devm_kzalloc(&hdev->dev, priv->buffer_size, GFP_KERNEL);

	priv->buffer[i++] = STATUS_REPORT_ID;
	priv->buffer[i++] = 0x80;
	priv->buffer[i++] = 0x33;
	priv->buffer[i++] = 0x1;
	priv->buffer[i++] = 0x0;
	priv->buffer[i++] = 0x0;
	priv->buffer[i++] = 0x0;
	priv->buffer[i++] = 0x0;
	reinit_completion(&priv->wait_input_report);

	hid_hw_output_report(priv->hdev, priv->buffer, priv->buffer_size);

	ret = wait_for_completion_timeout(&priv->wait_input_report, msecs_to_jiffies(REQ_TIMEOUT));
	if (!ret)
		goto fail_and_close;

	hid_info(hdev, "finished\n");
	goto fail_and_close;

fail_and_close:
	hid_hw_close(hdev);
fail_and_stop:
	hid_hw_stop(hdev);
	printk("end of probe\n");
	return ret;
}

static void pcs_temper_remove(struct hid_device *hdev)
{
	// struct pcs_temper_data *priv = hid_get_drvdata(hdev);

	// debugfs_remove_recursive(priv->debugfs);
	// hwmon_device_unregister(priv->hwmon_dev);

	// if (!should_load(hdev))
	// 	return;
	// printk("remove temper\n");
	hid_device_io_stop(hdev);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id pcs_temper_table[] = {
	{ HID_USB_DEVICE(0x3553, 0xA001) },
	{ }
};

MODULE_DEVICE_TABLE(hid, pcs_temper_table);

static struct hid_driver pcs_temper_driver = {
	.name = DRIVER_NAME,
	.id_table = pcs_temper_table,
	.probe = pcs_temper_probe,
	.remove = pcs_temper_remove,
	.raw_event = pcs_temper_raw_event,
};

static int __init pcs_temper_init(void)
{
	return hid_register_driver(&pcs_temper_driver);
}

static void __exit pcs_temper_exit(void)
{
	// printk("unload temper\n");
	hid_unregister_driver(&pcs_temper_driver);
}

/* Request to initialize after the HID bus to ensure it's not being loaded before */
late_initcall(pcs_temper_init);
module_exit(pcs_temper_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James McDonnell <topgamer7@gmail.com>");
MODULE_DESCRIPTION("Hwmon driver PCsensor TEMPer Gold 3.5");
