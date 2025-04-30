#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include "disk_wrapper_ioctl.h"
#include "bio_alias.h"

#define KERNEL_SECTOR_SIZE 512

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ashmrtn");
MODULE_DESCRIPTION("test hello world");

static char* target_device_path = "";
module_param(target_device_path, charp, 0);

static char* flags_device_path = "";
module_param(flags_device_path, charp, 0);

const char* const flag_names[] = {
  "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
  "meta", "prio", "discard", "secure", "write same", "no idle", "fua", "flush",
  "read ahead", "throttled", "sorted", "soft barrier", "no merge", "started",
  "don't prep", "queued", "elv priv", "failed", "quiet", "preempt", "alloced",
  "copy user", "flush seq", "io stat", "mixed merge", "kernel", "pm", "end",
  "nr bits"
};

struct disk_write_op {
    struct disk_write_op_meta metadata;
    void* data;
    struct disk_write_op* next;
};

static int major_num = 0;

static struct hwm_device {
    unsigned long size;
    spinlock_t lock;
    u8* data;
    struct gendisk* gd;
    bool log_on;

    struct gendisk* target_dev;
    u8 target_partno;
    struct block_device* target_bd;
    struct file *bdev_file;
    
    // Pointer to first write op in the chain.
    struct disk_write_op* writes;
    // Pointer to last write op in the chain.
    struct disk_write_op* current_write;
    // Pointer to log entry to be sent to user-land next.
    struct disk_write_op* current_log_write;
    unsigned long current_checkpoint;
} Device;

static bool should_log(struct bio *bio);

static void free_logs(void) 
{
    // Remove all writes.
    ktime_t curr_time;
    struct disk_write_op *first = NULL;
    struct disk_write_op* w = Device.writes;
    struct disk_write_op* tmp_w;

    while (w != NULL) {
      kfree(w->data);
      tmp_w = w;
      w = w->next;
      kfree(tmp_w);
    }

    // Create a fresh checkpoint as the starting point
    first = kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
    if (first == NULL) {
      printk(KERN_WARNING "hwm: error allocating default checkpoint\n");
      return;
    }
    curr_time = ktime_get();

    first->metadata.bi_flags = HWM_CHECKPOINT_FLAG;
    first->metadata.bi_rw = HWM_CHECKPOINT_FLAG;
    first->metadata.time_ns = ktime_to_ns(curr_time);
    Device.current_write = first;
    Device.writes = first;
    Device.current_log_write = first;
    Device.current_checkpoint = 1;
}


static int disk_wrapper_ioctl(struct block_device* bdev, fmode_t mode,
    unsigned int cmd, unsigned long arg) 
{
    int ret = 0;
    unsigned int not_copied;
    struct disk_write_op *checkpoint = NULL;
    ktime_t curr_time;

    switch (cmd) {
      case HWM_LOG_OFF:
          printk(KERN_INFO "hwm: turning off data logging\n");
          Device.log_on = false;
          break;

      case HWM_LOG_ON:
          printk(KERN_INFO "hwm: turning on data logging\n");
          Device.log_on = true;
          break;

      case HWM_GET_LOG_META:
          //printk(KERN_INFO "hwm: getting next log entry meta\n");
          if (Device.current_log_write == NULL) {
            printk(KERN_WARNING "hwm: no log entry here \n");
            return -ENODATA;
          }

          if (!access_ok((void*) arg, sizeof(struct disk_write_op_meta))) {
            // TODO(ashmrtn): Find right error code.
            printk(KERN_WARNING "hwm: bad user land memory pointer in log entry"
                " size\n");
            return -EFAULT;
          }

          // Copy metadata.
          not_copied = sizeof(struct disk_write_op_meta);
          while (not_copied != 0) {
            unsigned int offset = sizeof(struct disk_write_op_meta) - not_copied;
            not_copied = copy_to_user((void*) (arg + offset),
                &(Device.current_log_write->metadata) + offset, not_copied);
          }
          break;
          
      case HWM_GET_LOG_DATA:
          //printk(KERN_INFO "hwm: getting log entry data\n");
          if (Device.current_log_write == NULL) {
              printk(KERN_WARNING "hwm: no log entries to report data for\n");
              return -ENODATA;
          }

          if (!access_ok((void*) arg, Device.current_log_write->metadata.size)) {
              return -EFAULT;
          }

          // Copy written data.
          not_copied = Device.current_log_write->metadata.size;
          while (not_copied != 0) {
              unsigned int offset = Device.current_log_write->metadata.size - not_copied;
              not_copied = copy_to_user((void*) (arg + offset),
                  Device.current_log_write->data + offset, not_copied);
          }
          break;

      case HWM_NEXT_ENT:
          //printk(KERN_INFO "hwm: moving to next log entry\n");
          if (Device.current_log_write == NULL) {
              printk(KERN_WARNING "hwm: no next log entry\n");
              return -ENODATA;
          }
          Device.current_log_write = Device.current_log_write->next;
          break;

      case HWM_CLR_LOG:
          printk(KERN_INFO "hwm: clearing data logs\n");
          free_logs();
          break;

      case HWM_CHECKPOINT:
          curr_time = ktime_get();
          printk(KERN_INFO "hwm: making checkpoint in log\n");

          // Create a new log entry that just says we got a checkpoint.
          checkpoint = kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
          if (checkpoint == NULL) {
              printk(KERN_WARNING "hwm: error allocating checkpoint\n");
              return -ENOMEM;
          }

          checkpoint->metadata.bi_rw = HWM_CHECKPOINT_FLAG;
          checkpoint->metadata.bi_flags = HWM_CHECKPOINT_FLAG;
          checkpoint->metadata.time_ns = ktime_to_ns(curr_time);

          // Aquire lock and add the new entry to the end of the list.
          spin_lock(&Device.lock);
          // Assuming spinlock keeps the compiler from reordering this before the
          // lock is aquired...
          checkpoint->metadata.write_sector = Device.current_checkpoint;
          ++Device.current_checkpoint;
          Device.current_write->next = checkpoint;
          Device.current_write = checkpoint;
          // Drop lock and return success.
          spin_unlock(&Device.lock);
          break;

      default:
        ret = -EINVAL;
    }
    
    return ret;
}


/*
 * Converts from kernel specific flags to flags that CrashMonkey uses.
 */
static unsigned long long convert_flags(unsigned long long flags) 
{
    unsigned long long res = 0;

    if ((flags & REQ_OP_MASK) == REQ_OP_WRITE) {
        res |= HWM_WRITE_FLAG;
    }

    if ((flags & REQ_OP_MASK) == REQ_OP_DISCARD) {
        res |= HWM_DISCARD_FLAG;
    }

    if ((flags & REQ_OP_MASK) == REQ_OP_SECURE_ERASE) {
        res |= HWM_SECURE_FLAG;
    }
    // REQ_OP_WRITE_SAME flag is absent in 6.12
    // if ((flags & REQ_OP_MASK) == REQ_OP_WRITE_SAME) {
    //   res |= HWM_WRITE_SAME_FLAG;
    // }
    if ((flags & REQ_OP_MASK) == REQ_OP_WRITE_ZEROES) {
        res |= HWM_WRITE_ZEROES_FLAG;
    }

    if (flags & REQ_FAILFAST_DEV) {
        res |= HWM_FAILFAST_DEV_FLAG;
    }

    if (flags & REQ_FAILFAST_TRANSPORT) {
        res |= HWM_FAILFAST_TRANSPORT_FLAG;
    }

    if (flags & REQ_FAILFAST_DRIVER) {
        res |= HWM_FAILFAST_DRIVER_FLAG;
    }

    if (flags & REQ_SYNC) {
        res |= HWM_SYNC_FLAG;
    }

    if (flags & REQ_META) {
        res |= HWM_META_FLAG;
    }

    if (flags & REQ_PRIO) {
        res |= HWM_PRIO_FLAG;
    }

    if (flags & REQ_NOMERGE) {
        res |= HWM_NOMERGE_FLAG;
    }
    if (!(flags & REQ_IDLE)) {
        res |= HWM_NOIDLE_FLAG;
    }

    if (flags & REQ_INTEGRITY) {
        res |= HWM_INTEGRITY_FLAG;
    }

    if (flags & REQ_FUA) {
        res |= HWM_FUA_FLAG;
    }
    if (flags & REQ_PREFLUSH) {
        res |= HWM_FLUSH_FLAG;
    }

    if (flags & REQ_RAHEAD) {
        res |= HWM_READAHEAD_FLAG;
    }

    return res;
}


static bool should_log(struct bio *bio) 
{
    return
      ((bio->BI_RW & REQ_FUA) || (bio->BI_RW & REQ_PREFLUSH) ||
      (bio->BI_RW & REQ_OP_FLUSH) || (bio_op(bio) == REQ_OP_WRITE) ||
      (bio_op(bio) == BIO_DISCARD_FLAG) ||
      (bio_op(bio) == REQ_OP_SECURE_ERASE) ||
      (bio_op(bio) == REQ_OP_WRITE_ZEROES));
}


/*
 * Debug output to dmesg to see what is happening. Only tested on 3.13 and 4.4
 * kernels (and mostly accurrate on 4.4). Only enabled for <= 4.4 kernels
 * because I'm too lazy to do all the flag translations for this in 4.15. See
 * the output log of a workload for the textual values CrashMonkey spits out.
 */
static void print_rw_flags(unsigned long rw, unsigned long flags) 
{
    int i;
    printk(KERN_INFO "\traw rw flags: 0x%.8lx\n", rw);
    for (i = REQ_OP_WRITE; i < __REQ_NR_BITS; i++) {
        if (rw & (1ULL << i)) {
            printk(KERN_INFO "\t%s\n", flag_names[i]);
        }
    }
    printk(KERN_INFO "\traw flags flags: %.8lx\n", flags);
    for (i = REQ_OP_WRITE; i < __REQ_NR_BITS; i++) {
        if (flags & (1ULL << i)) {
            printk(KERN_INFO "\t%s\n", flag_names[i]);
        }
    }
}


// TODO(ashmrtn): Currently not thread safe/reentrant. Make it so.
static void disk_wrapper_bio(struct bio* bio) 
{
    int copied_data;
    struct disk_write_op *write;
    struct hwm_device* hwm;
    ktime_t curr_time;

    
    // printk(KERN_INFO "hwm: bio rw of size %u headed for 0x%lx (sector 0x%lx)"
    //     " has flags:\n", bio->BI_SIZE, bio->BI_SECTOR * 512, bio->BI_SECTOR);
    // print_rw_flags(bio->BI_RW, bio->bi_flags);
    
    // Log information about writes, fua, and flush/flush_seq events in kernel memory.
    if (Device.log_on && should_log(bio)) {
        curr_time = ktime_get();

        printk(KERN_WARNING "hwm: bio rw of size %u headed for 0x%llu (sector 0x%llu)"
                        " has flags:\n", bio->BI_SIZE, bio->BI_SECTOR * 512, bio->BI_SECTOR);
        print_rw_flags(bio->BI_RW, bio->bi_flags);

        // Log data to disk logs.
        write = kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
        if (write == NULL) {
            printk(KERN_WARNING "hwm: unable to make new write node\n");
            goto passthrough;
        }

        write->metadata.bi_flags = convert_flags(bio->bi_flags);
        write->metadata.bi_rw = convert_flags(bio->BI_RW);
        write->metadata.write_sector = bio->BI_SECTOR;
        write->metadata.size = bio->BI_SIZE;
        write->metadata.time_ns = ktime_to_ns(curr_time);

        // Protect playing around with our list of logged bios.
        spin_lock(&Device.lock);

        if (Device.current_write == NULL) {
          // With the default first checkpoint, this case should never happen.
          printk(KERN_WARNING "hwm: found empty list of previous disk ops\n");

          // This is the first write in the log.
          Device.writes = write;
          // Set the first write in the log so that it's picked up later.
          Device.current_log_write = write;
        } else {
          // Some write(s) was/were already made so add this to the back of the
          // chain and update pointers.
          Device.current_write->next = write;
        }

        Device.current_write = write;
        spin_unlock(&Device.lock);

        write->data = kmalloc(write->metadata.size, GFP_NOIO);
        if (write->data == NULL) {
            printk(KERN_WARNING "hwm: unable to get memory for data logging\n");
            kfree(write);
            goto passthrough;
        }

        copied_data = 0;

        struct bio_vec vec;
        struct bvec_iter iter;
        bio_for_each_segment(vec, bio, iter) {
          //printk(KERN_INFO "hwm: making new page for segment of data\n");

          void *bio_data = kmap(vec.bv_page);
          memcpy((void*) (write->data + copied_data), bio_data + vec.bv_offset,
                vec.bv_len);
          kunmap(bio_data);
          copied_data += vec.bv_len;
        }
        // Sanity check which prints data copied to the log.
        printk(KERN_INFO "hwm: copied %u bytes of from %lx data:"
            "\n~~~\n%p\n~~~\n",
            write->metadata.size, write->metadata.write_sector * 512, write->data);
    }

passthrough:
    // Pass request off to normal device driver.
    // Get the value if gendisk (bio->bi_bdev->bd_disk = hwm->target_dev) without queue
    hwm = (struct hwm_device*) &Device;

    bio->bi_bdev = hwm->target_bd;
    printk(KERN_WARNING "hwm: target device: %s \n", bio->bi_bdev->bd_disk->disk_name);
    printk(KERN_WARNING "hwm: Major number is %d:\n", hwm->gd->major);
    printk(KERN_WARNING "hwm: Sending bio to cow_brd\n");
    submit_bio(bio);
    printk(KERN_WARNING "hwm: Sending bio to cow_brd done.....\n");
}



// The device operations structure.
static const struct block_device_operations disk_wrapper_ops = {
    .owner   = THIS_MODULE,
    .submit_bio = disk_wrapper_bio,
    .ioctl   = disk_wrapper_ioctl,
};


static struct file *bdev_file, *flags_bdev_file;

// TODO(ashmrtn): Fix error when wrong device path is passed.
static int __init disk_wrapper_init(void) 
{
    printk(KERN_INFO "hwm: Hello World from disk_wrapper module\n");
    unsigned long queue_flags;
    struct block_device *flags_device, *target_device;
    struct disk_write_op *first = NULL;
    ktime_t curr_time;

    if (strlen(target_device_path) == 0) {
        return -ENOTTY;
    }
    if (strlen(flags_device_path) == 0) {
        return -ENOTTY;
    }
    printk(KERN_INFO "hwm: Wrapping device %s with flags device %s\n", 
                    target_device_path, flags_device_path);

    // Initializing logging status and checkpoint counter
    Device.log_on = false;
    // Make a checkpoint marking the beginning of the log. This will be useful
    // when watches are implemented and people begin a watch at the very start of
    // a test.
    Device.current_checkpoint = 1;
    first = kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
    if (first == NULL) {
        printk(KERN_WARNING "hwm: error allocating default checkpoint\n");
        goto out;
    }

    curr_time = ktime_get();

    //Initialize the checkpoint metadata
    first->metadata.bi_rw = HWM_CHECKPOINT_FLAG;
    first->metadata.bi_flags = HWM_CHECKPOINT_FLAG;
    first->metadata.time_ns = ktime_to_ns(curr_time);

    Device.writes = first;
    Device.current_write = first;
    Device.current_log_write = Device.current_write;

    // Get registered.
    major_num = register_blkdev(major_num, "hwm");
    if (major_num <= 0) {
        printk(KERN_WARNING "hwm: unable to get major number\n");
        goto out;
    }

    bdev_file = bdev_file_open_by_path(strim(target_device_path), 
            BLK_OPEN_READ, &Device, NULL);
    if (IS_ERR(bdev_file)) {
        printk(KERN_WARNING "hwm: unable to grab underlying device file with path %s\n", target_device_path);
        goto out;
    }
    target_device = file_bdev(bdev_file);

    if (!target_device || IS_ERR(target_device)) {
        printk(KERN_WARNING "hwm: unable to grab underlying device\n");
        goto out;
    }
    if (!target_device->bd_disk->queue) {
        printk(KERN_WARNING "hwm: attempt to wrap device with no request queue\n");
        goto out;
    }

    pr_info("hwm: opened “%s” → disk=%s major=%d minor=%d "
       "capacity=%llu sectors, log_blk_sz=%u\n",
       target_device_path,
       target_device->bd_disk->disk_name,
       MAJOR(target_device->bd_dev),
       MINOR(target_device->bd_dev),
       (unsigned long long)get_capacity(target_device->bd_disk),
       queue_logical_block_size(target_device->bd_disk->queue));
       
    Device.target_dev = target_device->bd_disk;
    Device.target_partno = bdev_partno(target_device);
    printk(KERN_INFO "hwm: Target device partition no %d\n", Device.target_partno);
    Device.target_bd = target_device;
    // Device.bdev_file = bdev_file;
 
    flags_bdev_file = bdev_file_open_by_path(strim(flags_device_path), 
            BLK_OPEN_READ, NULL, NULL);
    unsigned long flags_error = IS_ERR_VALUE((unsigned long)flags_bdev_file);

    if (IS_ERR(flags_bdev_file)) {
        printk(KERN_WARNING "hwm: unable to grab underlying device file %lu\n", flags_error);
        goto out;
    }
    flags_device = file_bdev(flags_bdev_file);
  
    if (!flags_device || IS_ERR(flags_device)) {
        printk(KERN_WARNING "hwm: unable to grab device to clone flags\n");
        goto out;
    }
    if (!flags_device->bd_queue) {
        printk(KERN_WARNING "hwm: attempt to wrap device with no request queue\n");
        goto out;
    }

    queue_flags = flags_device->bd_queue->queue_flags;
    fput(flags_bdev_file);

    // Set up our internal device.
    spin_lock_init(&Device.lock);
    // And the gendisk structure.
    Device.gd = blk_alloc_disk(NULL, NUMA_NO_NODE);
 
    if (!Device.gd) {
      printk(KERN_INFO "hwm: alloc_disk failure\n");
      goto out;
    }

    Device.gd->private_data = &Device;
    Device.gd->major = major_num;
    printk(KERN_INFO "hwm: Major number is %d\n", major_num);
    Device.gd->first_minor = target_device->bd_disk->first_minor;
    Device.gd->minors = target_device->bd_disk->minors;
    set_capacity(Device.gd, get_capacity(target_device->bd_disk));
    strcpy(Device.gd->disk_name, "hwm");
    Device.gd->fops = &disk_wrapper_ops;
    // Device.gd->part0 = target_device;

    if (Device.gd->queue == NULL) {
        printk(KERN_INFO "hwm: Device.gd queue null\n");
        goto out;
    }


    // printk(KERN_INFO "Before assigning\n");
    // printk(KERN_INFO "Queue flags: %lu\n", Device.gd->queue->queue_flags);
    // printk(KERN_INFO "Queue Data: %p\n", Device.gd->queue->queuedata);

    // unsigned long flags = (1UL << QUEUE_FLAG_SAME_COMP) |
    //                     (1UL << QUEUE_FLAG_INIT_DONE) |
    //                     (1UL << QUEUE_FLAG_STATS) |
    //                     (1UL << QUEUE_FLAG_REGISTERED) |
    //                     (1UL << QUEUE_FLAG_SQ_SCHED); /* single queue style io dispatch */
    
    // printk(KERN_INFO "TESTING FLGAS : %lx\n", flags);
    // Device.gd->queue->queue_flags = flags;

    queue_flags &= ~(1UL << QUEUE_FLAG_SQ_SCHED);

    Device.gd->queue->queue_flags = queue_flags;
    Device.gd->queue->queuedata = &Device;

    printk(KERN_INFO "hwm: working with queue with:\n\tflags 0x%lx\n", 
        Device.gd->queue->queue_flags);
  
    printk(KERN_INFO "hwm: Before add_disk");

    int err = add_disk(Device.gd);
    if(err) {
        printk(KERN_INFO "hwm: Error in adding disk");
        goto out;
    }
    
    printk(KERN_NOTICE "hwm: initialized\n");
    return 0;

out:
    unregister_blkdev(major_num, "hwm");
    return -ENOMEM;
}

static void __exit hello_cleanup(void) {
    printk(KERN_INFO "hwm: Starting clean up ....\n");

    free_logs();
    printk(KERN_INFO "hwm: Free_logs done...\n");
    // fput(Device.bdev_file);

    if (bdev_file) {
        fput(bdev_file);
    }

    // put_disk(Device.gd);
    if (Device.gd->queue){
      blk_put_queue(Device.gd->queue);
    }

    printk(KERN_INFO "hwm: del_gendisk is starting....\n");
    del_gendisk(Device.gd);
    printk(KERN_INFO "hwm: del_gendisk done....\n");
    
    put_disk(Device.gd);
    printk(KERN_INFO "hwm: put disk done .... \n");
      
    unregister_blkdev(major_num, "hwm");

    printk(KERN_INFO "hwm: Cleaning up bye!\n");
}

module_init(disk_wrapper_init);
module_exit(hello_cleanup);

