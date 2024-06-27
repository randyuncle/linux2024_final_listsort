#include <linux/kernel.h> /* We're doing kernel work */ 
#include <linux/uaccess.h> /* for copy_from_user */ 
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "sort.h"
#include "list.h"

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

/* The array buffer that saves the place to fill the random variable
 * or the saves the value for the duplicate case */
int exch[MAX_LEN] = {0};
static void create_samples(struct list_head *head,
                           element_t *space,
                           int samples,
                           int case_id)
{
    memset(exch, 0, sizeof(exch));
    int cnt = 0;
    /* defining the place to fill random values */
    switch (case_id) {
    case 1: /* Random 3 elements */
        for (int i = 0; i < 3; i++) {
            uint64_t temp = next() % MAX_NUM;
            exch[i] = temp % samples;
            for (int j = 0; j < i; j++) {
                if (exch[i] == exch[j])
                    i--;
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < i; j++) {
                if (exch[i] < exch[j]) {
                    int temp = exch[i];
                    exch[i] = exch[j];
                    exch[j] = temp;
                }
            }
        }
        break;
    case 3: /* Random 1% elements */
        int num = samples / 100;

        for (int i = 0; i < num; i++) {
            uint64_t temp = next() % MAX_NUM;
            exch[i] = temp % samples;
            for (int j = 0; j < i; j++) {
                if (exch[i] == exch[j]) {
                    i--;
                    break;
                }
            }
        }

        for (int i = 0; i < num; i++) {
            for (int j = 0; j < i; j++) {
                if (exch[i] < exch[j]) {
                    int temp = exch[i];
                    exch[i] = exch[j];
                    exch[j] = temp;
                }
            }
        }
        break;
    case 4: /* Duplicate */
        for (int i = 0 ; i < 4 ; i++)
            exch[i] = i + 12300;
        break;
    default:
        break;
    }

    /* Start to create the samples for the testing list */
    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;

        int value;
        uint64_t temp = next() % MAX_NUM;
        switch (case_id) {
        case 0: /* Worst case of merge sort */
            value = i;
            break;
        case 1: /* Random 3 elements */
            if (i == exch[cnt]) {
                value = temp;
                cnt++;
            } else
                value = i;
            break;
        case 2: /* Random last 10 elements */
            if (i < samples - 10)
                value = i;
            else {
                value = temp;
            }
            break;
        case 3: /* Random 1% elements */
            if (i == exch[cnt]) {
                value = temp;
                cnt++;
            } else
                value = i;
            break;
        case 4: /* Duplicate */
            value = exch[temp % 4];
            break;
        default: /* Random elements */
            value = temp;
            break;
        }

        elem->value = value;
        elem->seq = i;
        list_add_tail(&elem->list, head);
    }

    /* Worst case scenario */
    if (!case_id)
        worst_case_generator(head);
}

static void copy_list(struct list_head *from, struct list_head *to, element_t *space)
{
    if (list_empty(from))
        return;

    element_t *entry;
    list_for_each_entry (entry, from, list) {
        element_t *copy = space++;
        copy->value = entry->value;
        copy->seq = entry->seq;
        list_add_tail(&copy->list, to);
    }
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
st_dev dptr;
st_usr uptr;
/* When a process attempts to read this opened dev file, 
 * starting the test of the linked-list.
 */
static ssize_t sort_test_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    if (size != sizeof(st_usr)) {
        printk(KERN_ALERT "Invalid data size\n");
        return -EFAULT;
    }

    size_t count = 0;
    struct list_head sample_head, warm_head;

    /* The test samples */
    element_t *samples = kmalloc(dptr.nodes * sizeof(element_t *), GFP_KERNEL);
    if (!samples) {
        printk(KERN_ALERT "sort_test: kmalloc failed on `samples`\n");
        return -ENOMEM; // Return error if allocation fails
    }

    element_t *warmdatas = kmalloc(dptr.nodes * sizeof(element_t *), GFP_KERNEL);
    if (!warmdatas) {
        printk(KERN_ALERT "sort_test: kmalloc failed on `samples`\n");
        return -ENOMEM; // Return error if allocation fails
    }

    INIT_LIST_HEAD(&sample_head);
    create_samples(&sample_head, samples, dptr.nodes, dptr.case_id);

    INIT_LIST_HEAD(&warm_head);
    copy_list(&sample_head, &warm_head, warmdatas);

    /* Warmup */
    test.impl(&count, &warm_head, list_cmp);

    count = 0;
    kt_sort = ktime_get();
    /* Start the sortings */
    test.impl(&count, &sample_head, list_cmp);
    kt_sort = ktime_sub(ktime_get(), kt_sort);
    ktime_to_us(kt_sort);

    if (!check_list(&sample_head, count)) {
        printk(KERN_ALERT "The list isn't sorted in the correct order\n");
        return -EFAULT;
    }

    uptr.time = kt_sort;
    uptr.count = count;

    /* Clean the value and list in the current `element_t` structure */
    element_t *iterator, *next;
    list_for_each_entry_safe (iterator, next, &sample_head, list)
        list_del(&iterator->list);
    list_for_each_entry_safe (iterator, next, &warm_head, list)
        list_del(&iterator->list);

    kfree(samples);
    kfree(warmdatas);
    
    if (copy_to_user(buf, &uptr, size) < 0) {
        printk(KERN_ALERT "Failed to copy data to user\n");
        return -EFAULT;
    }

    return size;
}

static ssize_t sort_test_write(struct file *file, const char *buf, size_t size, loff_t *offset)
{
    if (size != sizeof(st_dev)) {
        printk(KERN_ALERT "Invalid data size\n");
        return -EFAULT;
    }

    if (copy_from_user(&dptr, buf, size) < 0) {
        printk(KERN_ALERT "Failed to copy data from user\n");
        return -EFAULT;
    }

    /* Update the sort test functions */
    test = tests[dptr.test_case];

    return size;
}

/* Set the file operations of the kernel module */
static const struct file_operations fops = {
    .read = sort_test_read,
    .write = sort_test_write,
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