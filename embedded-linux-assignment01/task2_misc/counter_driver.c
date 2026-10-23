#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEVICE_NAME "counter"
#define PROC_NAME   "counter_info"
#define INPUT_SIZE  64

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Le Tien Thanh - SE201799");
MODULE_DESCRIPTION("Assignment 01 - Misc Counter Device");
MODULE_VERSION("1.0");

/* Shared device state. */
static atomic_t counter_value = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);
static DEFINE_MUTEX(counter_mutex);
static struct proc_dir_entry *counter_proc;

/* Preserve one value across partial reads of a file descriptor. */
struct counter_file_data {
    char text[32];
    size_t length;
    bool initialized;
};

/*
 * counter_open: allocate per-open read state.
 * file: opened device descriptor.
 * Return: 0 or -ENOMEM.
 */
static int counter_open(struct inode *inode, struct file *file)
{
    struct counter_file_data *data;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    file->private_data = data;
    return 0;
}

/*
 * counter_release: free per-open read state.
 * file: descriptor being closed.
 * Return: 0.
 */
static int counter_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    file->private_data = NULL;
    return 0;
}

/*
 * counter_read: return the current counter as decimal text.
 * On the first successful read for this open, increment once.
 * Offset and partial reads are handled correctly.
 * Return: bytes copied, EOF (0), or negative errno.
 */
static ssize_t counter_read(struct file *file,
                            char __user *buf,
                            size_t count,
                            loff_t *ppos)
{
    struct counter_file_data *data = file->private_data;
    ssize_t ret;
    int value;

    if (!data)
        return -EINVAL;

    if (count == 0)
        return 0;

    mutex_lock(&counter_mutex);

    if (!data->initialized) {
        value = atomic_read(&counter_value);
        data->length = scnprintf(data->text,
                                 sizeof(data->text),
                                 "%d\n", value);
    }

    ret = simple_read_from_buffer(buf, count, ppos,
                                  data->text, data->length);

    if (ret > 0 && !data->initialized) {
        atomic_inc(&counter_value);
        atomic_inc(&total_reads);
        data->initialized = true;
        pr_info("counter_driver: read value=%d\n",
                atomic_read(&counter_value) - 1);
    }

    mutex_unlock(&counter_mutex);
    return ret;
}

/*
 * counter_write: set/reset counter using ASCII integer input.
 * Negative and invalid values are rejected with -EINVAL.
 * Return: bytes accepted or negative errno.
 */
static ssize_t counter_write(struct file *file,
                             const char __user *buf,
                             size_t count,
                             loff_t *ppos)
{
    struct counter_file_data *data = file->private_data;
    char input[INPUT_SIZE];
    int value;
    int ret;

    if (count == 0 || count >= sizeof(input))
        return -EINVAL;

    if (copy_from_user(input, buf, count))
        return -EFAULT;

    input[count] = '\0';

    ret = kstrtoint(input, 10, &value);
    if (ret || value < 0)
        return -EINVAL;

    mutex_lock(&counter_mutex);

    atomic_set(&counter_value, value);
    atomic_inc(&total_writes);

    /* A subsequent read on this descriptor starts afresh. */
    if (data)
        data->initialized = false;

    *ppos = 0;

    mutex_unlock(&counter_mutex);

    pr_info("counter_driver: set counter=%d\n", value);
    return count;
}

/*
 * counter_proc_show: publish current counter and operation totals.
 * seq: procfs output context.
 * Return: 0.
 */
static int counter_proc_show(struct seq_file *seq, void *v)
{
    mutex_lock(&counter_mutex);

    seq_printf(seq, "counter_value: %d\n",
               atomic_read(&counter_value));
    seq_printf(seq, "total_reads: %d\n",
               atomic_read(&total_reads));
    seq_printf(seq, "total_writes: %d\n",
               atomic_read(&total_writes));

    mutex_unlock(&counter_mutex);
    return 0;
}

/*
 * counter_proc_open: open procfs seq_file interface.
 * Return: single_open result.
 */
static int counter_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open    = counter_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static const struct file_operations counter_fops = {
    .owner   = THIS_MODULE,
    .open    = counter_open,
    .release = counter_release,
    .read    = counter_read,
    .write   = counter_write,
    .llseek  = no_llseek,
};

static struct miscdevice counter_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &counter_fops,
    .mode  = 0666,
};

/*
 * counter_init: register misc device and create procfs entry.
 * Return: 0 on success or negative errno.
 */
static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter_value, 0);
    atomic_set(&total_reads, 0);
    atomic_set(&total_writes, 0);

    ret = misc_register(&counter_misc);
    if (ret) {
        pr_err("counter_driver: misc_register failed=%d\n", ret);
        return ret;
    }

    counter_proc = proc_create(PROC_NAME, 0444, NULL,
                               &counter_proc_ops);
    if (!counter_proc) {
        misc_deregister(&counter_misc);
        pr_err("counter_driver: proc_create failed\n");
        return -ENOMEM;
    }

    pr_info("counter_driver: loaded major=10 minor=%d\n",
            counter_misc.minor);

    return 0;
}

/*
 * counter_exit: remove procfs entry and unregister misc device.
 */
static void __exit counter_exit(void)
{
    proc_remove(counter_proc);
    misc_deregister(&counter_misc);
    pr_info("counter_driver: unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);
