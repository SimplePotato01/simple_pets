#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>

#define DEVICE_NAME "mychardev"
#define CLASS_NAME "mychar"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("My name");
MODULE_DESCRIPTION("Symbol driver");

static int major_number;
static struct class *chardev_class = NULL;
static struct device *chardev_device = NULL;
static struct cdev my_cdev;
static char *device_buffer;
static int buffer_pointer = 0;
static int open_count = 0;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int dev_open(struct inode *inodep, struct file *filep) {
	open_count++;
	printk(KERN_INFO "mychardev: device openned %d times\n", open_count);
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
	printk(KERN_INFO "mychardev: device closed\n");
	return 0;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
	int bytes_to_read;
	int bytes_read;
    
	printk(KERN_INFO "mychardev: trying to read %zu bytes position=%lld\n", len, *offset);
    
	if (*offset >= buffer_pointer) {
		printk(KERN_INFO "mychardev: EOF reached\n");
		return 0;
	}
    
	bytes_to_read = min(len, (size_t)(buffer_pointer - *offset));
    
	if (copy_to_user(buffer, device_buffer + *offset, bytes_to_read)) {
		printk(KERN_ALERT "mychardev: error during copy to the user\n");
		return -EFAULT;
	}
    
	*offset += bytes_to_read;
	bytes_read = bytes_to_read;
    
	printk(KERN_INFO "mychardev: read %d bytes, new position=%lld\n", bytes_read, *offset);
    
	return bytes_read;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
	int bytes_to_write;
	int bytes_written;
    
	printk(KERN_INFO "mychardev: trying to write %zu bytes, position=%lld\n", len, *offset);
    
	if (*offset >= BUFFER_SIZE) {
		printk(KERN_WARNING "mychardev: error out of bound of buffer\n");
		return -ENOSPC;
	}
    
	bytes_to_write = min(len, (size_t)(BUFFER_SIZE - *offset));
    
	if (copy_from_user(device_buffer + *offset, buffer, bytes_to_write)) {
		printk(KERN_ALERT "mychardev: error during copy from the user\n");
		return -EFAULT;
	}
    
	*offset += bytes_to_write;
	bytes_written = bytes_to_write;
    
	if (*offset > buffer_pointer) buffer_pointer = *offset;
    
	printk(KERN_INFO "mychardev: wrote %d bytes, new position=%lld\n", bytes_written, *offset);
	printk(KERN_DEBUG "mychardev: buffer now: \"%s\"\n", device_buffer);
    
	return bytes_written;
}

static int __init chardev_init(void) {
	dev_t dev_num;
	int result;
    
	printk(KERN_INFO "mychardev: loading module\n");
    
	device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!device_buffer) {
		printk(KERN_ALERT "mychardev: error on kmalloc():\n");
		return -ENOMEM;
	}
	memset(device_buffer, 0, BUFFER_SIZE);
    
	result = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (result < 0) {
		printk(KERN_ALERT "mychardev: error on getting major num\n");
		kfree(device_buffer);
		return result;
	}
	major_number = MAJOR(dev_num);
	printk(KERN_INFO "mychardev: registered with major=%d\n", major_number);
    
	cdev_init(&my_cdev, &fops);
	my_cdev.owner = THIS_MODULE;
	result = cdev_add(&my_cdev, dev_num, 1);
	if (result < 0) {
		printk(KERN_ALERT "mychardev: cannot add cdev\n");
		unregister_chrdev_region(dev_num, 1);
		kfree(device_buffer);
		return result;
	}
    
	chardev_class = class_create(CLASS_NAME);
	if (IS_ERR(chardev_class)) {
		printk(KERN_ALERT "mychardev: cannot create class\n");
		cdev_del(&my_cdev);
		unregister_chrdev_region(dev_num, 1);
		kfree(device_buffer);
		return PTR_ERR(chardev_class);
	}
    
	chardev_device = device_create(chardev_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(chardev_device)) {
		printk(KERN_ALERT "mychardev: cannot create the device\n");
		class_destroy(chardev_class);
		cdev_del(&my_cdev);
		unregister_chrdev_region(dev_num, 1);
		kfree(device_buffer);
		return PTR_ERR(chardev_device);
	}
    
	printk(KERN_INFO "mychardev: DONE! Device /dev/%s created\n", DEVICE_NAME);
	return 0;
}

static void __exit chardev_exit(void) {
	dev_t dev_num = MKDEV(major_number, 0);
    
	device_destroy(chardev_class, dev_num);
	class_destroy(chardev_class);
    
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, 1);
    
	kfree(device_buffer);
    
	printk(KERN_INFO "mychardev: module unloaded\n");
}

module_init(chardev_init);
module_exit(chardev_exit);
