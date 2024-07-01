#include <linux/kernel.h> /* We're doing kernel work */ 
#include <linux/uaccess.h> /* for copy_from/to_user*/ 
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/list.h>

#include "sort.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Sorting test driver");
MODULE_VERSION("0.1");

#define DEVICE_NAME "sort_test"

/* The function from xoroshiro128p */
extern void seed(uint64_t, uint64_t);
extern void jump(void);
extern uint64_t next(void);

/* The extern function from `sort_test_impl` */
extern void worst_case_generator(struct list_head *head);

/* The structure of the linked-list in this test */
typedef struct {
    int value;
    struct list_head list;
    int seq;
} element_t;

static dev_t dev = -1;
static struct cdev cdev;
static struct class *class;

/* The compare function for this linked-list structure */
static int list_cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
    element_t *element_a = list_entry(a, element_t, list);
    element_t *element_b = list_entry(b, element_t, list);

    /* `int` data type could know if the cmp is larger, equal, or less 
     */
    int res = element_a->value - element_b->value;

    if (!res)
        return 0;
    
    if (priv)
        *((size_t *) priv) += 1;

    return res;
}

static int create_samples(struct list_head *head,
                           int samples,
                           int case_id)
{
    /* Variables for random values */
    int random_section, random_index, random_count;
    /* The array for saving the duplicate values */
    int dup[4];
    /* defining the place to fill random values */
    switch (case_id) {
    case 1: /* Random 3 elements */
        random_count = 3;
        random_section = samples / 3;
        random_index = next() % random_section;
        break;
    case 3: /* Random 1% elements */
        random_count = samples / 100;
        random_section = 100;
        random_index = next() % random_section;
        break;
    case 4: /* Duplicate */
        for (int i = 0 ; i < 4 ; i++)
            dup[i] = i + 12300;
        break;
    default:
        break;
    }

    int cnt = 0;
    /* Start to create the samples for the testing list */
    for (int i = 0; i < samples; i++, cnt++) {
        element_t *sample = kmalloc(sizeof(element_t), GFP_KERNEL);
        if (!samples) {
            printk(KERN_ALERT "sort_test: kmalloc failed on `sample`\n");
            return -ENOMEM; // Return error if allocation fails
        }

        int value;
        switch (case_id) {
        case 0: /* Worst case of merge sort */
            value = i;
            break;
        case 1: /* Random 3 elements */
            if (cnt == random_index && random_count) {
                value = next() % MAX_LEN;
                random_index = next() % random_section;
                cnt = -1;
                random_count--;
            } else
                value = i;
            break;
        case 2: /* Random last 10 elements */
            if (i < samples - 10)
                value = i;
            else {
                value = next() % MAX_LEN;
            }
            break;
        case 3: /* Random 1% elements */
            if (cnt == random_index && random_count) {
                value = next() % MAX_LEN;
                random_index = next() % random_section;
                cnt = -1;
                random_count--;
            } else
                value = i;
            break;
        case 4: /* Duplicate */
            value = dup[next() % 4];
            break;
        default: /* Random elements */
            value = next() % MAX_LEN;
            break;
        }

        sample->value = value;
        sample->seq = i;
        list_add_tail(&sample->list, head);
    }

    /* Worst case scenario */
    if (!case_id)
        worst_case_generator(head);
    
    return 0;
}

static int copy_list(struct list_head *from, struct list_head *to)
{
    if (list_empty(from))
        return 0;

    element_t *entry;
    list_for_each_entry (entry, from, list) {
        element_t *copy = kmalloc(sizeof(element_t), GFP_KERNEL);
        if (!copy) {
            printk(KERN_ALERT "sort_test: kmalloc failed on `sample`\n");
            return -ENOMEM; // Return error if allocation fails
        }

        copy->value = entry->value;
        copy->seq = entry->seq;
        list_add_tail(&copy->list, to);
    }

    return 0;
}


static bool check_list(struct list_head *head, int count)
{
    if (list_empty(head))
        return 0 == count;

    element_t *entry, *safe;
    size_t ctr = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        ctr++;
    }

    int unstable = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->list.next != head) {
            if (entry->value > safe->value) {
                printk(KERN_ALERT "\nERROR: Wrong order\n");
                return false;
            }
            if (entry->value == safe->value && entry->seq > safe->seq)
                unstable++;
        }
    }

    if (unstable) {
        printk(KERN_ALERT "\nERROR: unstable %d\n", unstable);
        return false;
    }

    if (ctr < MIN_LEN && ctr > MAX_LEN) {
        printk(KERN_ALERT "\nERROR: Inconsistent number of elements: %ld\n", ctr);
        return false;
    }

    return true;
}

test_t tests[] = {
    {.name = "listsort", .impl = list_sort},
    {.name = "timsort_merge", .impl = timsort_merge},
    {.name = "timsort_linear", .impl = timsort_linear},
    {.name = "timsort_binary", .impl = timsort_binary},
    {.name = "timsort_gallop", .impl = timsort_l_gallop},
    {.name = "timsort_b_gallop", .impl = timsort_b_gallop},
    {.name = "adaptive_shiverssort", .impl = shiverssort},
    {.name = "adaptive_shiverssort_merge", .impl = shiverssort_merge},
    {NULL, NULL},
};
test_t test;

static ktime_t kt_sort;
int nodes, case_id;

static int sort_test_open(struct inode *inode, struct file *file)
{
    nodes = 0;
    case_id = 0;
    
    // printk(KERN_INFO "You have opened the `sort_test` device driver !");
    return 0;
}

static int sort_test_release(struct inode *inode, struct file *file)
{
    nodes = 0;
    case_id = 0;

    // printk(KERN_INFO "You have closed the `sort_test` device driver !");
    return 0;
}

/* When a process attempts to read this opened dev file, 
 * starting the test of the linked-list.
 */
static ssize_t sort_test_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    local_irq_disable(); /* disable interrupt */
    get_cpu(); /* disable preemption */

    size_t count = 0;
    struct list_head sample_head, warmup_head;

    /* Initialize the sample linked-list */
    INIT_LIST_HEAD(&sample_head);
    int chk = create_samples(&sample_head, nodes, case_id);
    if (chk)
        return chk;

    /* Initialize the warmup linked-list */
    INIT_LIST_HEAD(&warmup_head);
    chk = copy_list(&sample_head, &warmup_head);
    if (chk)
        return chk;

    /* Warmup */
    test.impl(&count, &warmup_head, list_cmp);

    count = 0;
    kt_sort = ktime_get();
    /* Start the sortings */
    test.impl(&count, &sample_head, list_cmp);
    kt_sort = ktime_sub(ktime_get(), kt_sort);
    ktime_to_us(kt_sort);

    /* Check if the list is sorted */
    if (!check_list(&sample_head, count)) {
        printk(KERN_ALERT "The list isn't sorted in the correct order\n");
        return 0;
    }

    /* Delete the list and free the current `element_t` structure */
    element_t *iterator, *next;
    list_for_each_entry_safe (iterator, next, &sample_head, list) {
        list_del(&iterator->list);
        kfree(iterator);
    }

    list_for_each_entry_safe (iterator, next, &warmup_head, list) {
        list_del(&iterator->list);
        kfree(iterator);
    }

    /* Return the result of the test to user space */
    char device_buf[512];
    snprintf(device_buf, 512, "%llu %lu", (unsigned long long int) kt_sort, count);
    unsigned long len = copy_to_user(buf, device_buf, 512);
    if (len != 0) {
        printk(KERN_ALERT "Failed to copy data to user\n");
        return 0;
    }

    local_irq_enable();
    put_cpu();

    return size;
}

static ssize_t sort_test_write(struct file *file, const char __user  *buf, size_t size, loff_t *offset)
{
    /* Get the test information from user space */
    char device_buf[512];
    unsigned long len = copy_from_user(device_buf, buf, size);
    if (len != 0) {
        printk(KERN_ALERT "Failed to copy data from user\n");
        return 0;
    }

    char *token;
    char *str = kstrdup(device_buf, GFP_KERNEL);

    int counter = 0;
    /* Update the information of current sort test */
    while ((token = strsep(&str, " ")) != NULL) {
        /* Convert the token to an unsigned long long int */
        long int number = simple_strtol(token, NULL, 10);
        if (counter == 0)
            nodes = (int) number;
        else if (counter == 1)
            case_id = (int) number;
        else if (counter == 2)
            test = tests[(int) number];
        else
            break;
        counter++;
    }

    // printk(KERN_INFO "The current test info: nodes = %d, case_id = %d, sort program = %s", 
    //        nodes, case_id, test.name);

    kfree(str);

    return size;
}

/* Set the file operations of the kernel module */
static const struct file_operations fops = {
    .read = sort_test_read,
    .write = sort_test_write,
    .open = sort_test_open,
    .release = sort_test_release,
    .owner = THIS_MODULE,
};

static int __init sort_test_init(void)
{
    seed(314159265, 1618033989);  // Initialize PRNG with pi and phi.

    struct device *device;

    printk(KERN_INFO DEVICE_NAME ": loaded\n");

    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0)
        return -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    class = class_create(THIS_MODULE, DEVICE_NAME);
#else
    class = class_create(DEVICE_NAME);
#endif
    if (IS_ERR(class)) {
        goto error_unregister_chrdev_region;
    }
   
    device = device_create(class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        goto error_class_destroy;
    }

    cdev_init(&cdev, &fops);
    if (cdev_add(&cdev, dev, 1) < 0)
        goto error_device_destroy;

    return 0;

error_device_destroy:
    device_destroy(class, dev);
error_class_destroy:
    class_destroy(class);
error_unregister_chrdev_region:
    unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit sort_test_exit(void)
{
    device_destroy(class, dev);
    class_destroy(class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);

    printk(KERN_INFO DEVICE_NAME ": unloaded\n");
}

module_init(sort_test_init);
module_exit(sort_test_exit);
