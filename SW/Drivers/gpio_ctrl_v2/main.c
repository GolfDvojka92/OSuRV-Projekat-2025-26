// SPDX-License-Identifier: MIT

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h> // gpio_set_value(), gpio_get_value()
#include <linux/miscdevice.h> // misc_register()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()
#include <linux/delay.h> // udelay()


typedef struct gpio_desc* gpio_t;
gpio_t gpio[27];


static int gpio_stream_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int gpio_stream_release(struct inode *inode, struct file *filp) {
	return 0;
}

uint8_t rd_val = 0;

static ssize_t gpio_stream_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos
) {

	uint8_t pkg[3];
	uint8_t op;
	uint8_t gpio_num;
	uint8_t wr_val;
	int i;
	(void)i;

	if(len != 3 && len != 2){
		return -EINVAL;
	}

	if(copy_from_user(pkg, buf, len) != 0){
		return -EFAULT;
	}

#if 0
	printk(KERN_INFO "gpio_ctrl: %s() len = %d\n", __func__, (int)len);
	for(i = 0; i < len; i++){
		printk(KERN_INFO "gpio_ctrl: %s() buf[%d] = %d 0x%02x %c\n", __func__, i, (int)pkg[i], (int)pkg[i], pkg[i]);
	}
#endif

	op = pkg[0];
	//printk(KERN_INFO "gpio_ctrl: %s() op = %c\n", __func__, op);
	gpio_num = pkg[1];
	//printk(KERN_INFO "gpio_ctrl: %s() gpio_num = %d\n", __func__, gpio_num);
	
	if(gpio_num > 26){
		return -EINVAL;
	}

	if(len == 3 && op == 'w'){
		wr_val = pkg[2];
		//printk(KERN_INFO "gpio_ctrl: %s() wr_val = %d\n", __func__, wr_val);

		gpiod_direction_output(gpio[gpio_num], 0);

		gpiod_set_value(gpio[gpio_num], wr_val);

	}else if(len == 2 && op == 'r'){
		gpiod_direction_output(gpio[gpio_num], 1);

		rd_val = gpiod_get_value(gpio[gpio_num]);
		//printk(KERN_INFO "gpio_ctrl: %s() rd_val = %d\n", __func__, rd_val);
	}else{
		return -EINVAL;
	}

	// Move position in file.
	*f_pos += len;

	return len;
}


static ssize_t gpio_stream_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {

	if(len != 1){
		return -EINVAL;
	}

	if(copy_to_user(buf, &rd_val, len) != 0){
		return -EFAULT;
	}else{
		return len;
	}
}

static loff_t gpio_stream_llseek(
	struct file* filp,
	loff_t offset,
	int whence
) {
	switch(whence){
		case SEEK_SET:
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			filp->f_pos += offset;
			break;
		case SEEK_END:
			return -ENOSYS; // Function not implemented.
		default:
			return -EINVAL;
		}
	return filp->f_pos;
}

static struct file_operations gpio_stream_fops = {
	open           : gpio_stream_open,
	release        : gpio_stream_release,
	read           : gpio_stream_read,
	write          : gpio_stream_write,
	llseek         : gpio_stream_llseek
};

static struct miscdevice gpio_stream_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "gpio_stream",
	.fops  = &gpio_stream_fops,
};

static int gpio_ctrl_probe(struct platform_device *pdev) {
	int i;
	int ret;
	dev_info(&pdev->dev, "gpio_ctrl probe\n");

	for(i = 0; i < sizeof(gpio)/sizeof(gpio_t); i++){
		static char name[8];
		snprintf(name, sizeof(name), "gpio%02d", i);
		gpio[i] = devm_gpiod_get(&pdev->dev, name, GPIOD_OUT_LOW);
		if(IS_ERR(gpio[i])){
			if(i == 0){
				dev_err(&pdev->dev, "Failed to get gpio %d\n", i);
			}
			gpio[i] = gpio[0];
		}else{
			dev_info(&pdev->dev, "Obtained gpio %d\n", i);
		}
	}

	// Setup device
	ret = misc_register(&gpio_stream_miscdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register misc device!\n");
		return ret;
	}

	return 0;
}

static int gpio_ctrl_remove(struct platform_device *pdev) {
	dev_info(&pdev->dev, "gpio_ctrl remove\n");
	misc_deregister(&gpio_stream_miscdev);
	return 0;
}

static const struct of_device_id gpio_ctrl_of_match[] = {
	{ .compatible = "osurv,gpio_ctrl" },
	{}
};
MODULE_DEVICE_TABLE(of, gpio_ctrl_of_match);

static struct platform_driver gpio_ctrl_driver = {
	.probe  = gpio_ctrl_probe,
	.remove = gpio_ctrl_remove,
	.driver = {
		.name           = "gpio_ctrl",
		.of_match_table = gpio_ctrl_of_match,
	},
};
module_platform_driver(gpio_ctrl_driver);

MODULE_AUTHOR("OSuRV");
MODULE_DESCRIPTION("GPIO ctrl");
MODULE_LICENSE("Dual BSD/GPL");
