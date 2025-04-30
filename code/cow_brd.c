/*
 * Ram backed block device driver.
 *
 * Copyright (C) 2007 Nick Piggin
 * Copyright (C) 2007 Novell Inc.
 *
 * Parts derived from drivers/block/rd.c, and drivers/block/loop.c, copyright
 * of their respective owners.
 */

#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/xarray.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
// #include <asm/uaccess.h>
#include <linux/blk-mq.h>
#include <linux/workqueue.h>

#include "disk_wrapper_ioctl.h"
#include "bio_alias.h"

#define SECTOR_SHIFT        9
#define PAGE_SECTORS_SHIFT  (PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS        (1 << PAGE_SECTORS_SHIFT)
#define DEFAULT_COW_RD_SIZE 512000
#define DEVICE_NAME         "cow_brd"

/*
 * Each block ramdisk device has a radix_tree brd_pages of pages that stores
 * the pages containing the block device's contents. A brd page's ->index is
 * its offset in PAGE_SIZE units. This is similar to, but in no way connected
 * with, the kernel's pagecache or buffer cache (which sit above our block
 * device).
 */
struct brd_device {
    int   brd_number;
    struct brd_device *parent_brd;

    // Denotes whether or not a cow_ram is writable and snapshots are active.
    bool  is_writable;
    bool  is_snapshot;

    // struct request_queue  *brd_queue;
    struct gendisk    *brd_disk;
    struct list_head  brd_list;

    /*
     * Backing store of pages. This is the contents of the block device.
     */
    struct xarray	        brd_pages;
    u64			brd_nr_pages;
};

/*
 * Look up and return a brd's page for a given sector.
 */

static struct page *brd_lookup_page(struct brd_device *brd, sector_t sector)
{
    return xa_load(&brd->brd_pages, sector >> PAGE_SECTORS_SHIFT);
}


/*
 * Insert a new page for a given sector, if one does not already exist.
 */
static int brd_insert_page(struct brd_device *brd, sector_t sector, gfp_t gfp)
{
    pgoff_t idx = sector >> PAGE_SECTORS_SHIFT;
    struct page *page;
    struct page *parent_page = NULL;
    void *dst, *parent_src = NULL;
    int ret = 0;

    page = brd_lookup_page(brd, sector);
    if (page)
        return 0;

    page = alloc_page(gfp | __GFP_ZERO | __GFP_HIGHMEM);
    if (!page)
        return -ENOMEM;

    xa_lock(&brd->brd_pages);

    ret = __xa_insert(&brd->brd_pages, idx, page, gfp);

    if (!ret)
        brd->brd_nr_pages++;

    xa_unlock(&brd->brd_pages);

    if (ret < 0) {
        __free_page(page);
        if (ret == -EBUSY)
            ret = 0;
    }

    // Copy over the data from the parent's page to the snapshot page if the parent
    // has a page in this sector address.
    if (brd->parent_brd) {
        parent_page = brd_lookup_page(brd->parent_brd, sector);
        // This page may not have originally existed in the parent.
        if (parent_page) {
            // Map both the parent and snapshot pages so that the kernel can access
            // those addresses. The snapshot page and the parent page both already
            // reside in radix trees, so even when we unmap the pages, the data and
            // the page itself will still remain.
            dst = kmap_atomic(page);
            parent_src = kmap_atomic(parent_page);
            memcpy(dst, parent_src, PAGE_SIZE);
            kunmap_atomic(parent_src);
            kunmap_atomic(dst);
        }
    }

    return ret;
}


/*
 * Zero out a page (set to all 0s)
 */
// static void brd_zero_page(struct brd_device *brd, sector_t sector)
// {
//     struct page *page;

//     page = brd_lookup_page(brd, sector);
//     if (page)
//       clear_highpage(page);
// }


/* Free all backing store pages and xarray. This must only be called when
 * there are no other users of the device.
 */
static void brd_free_pages(struct brd_device *brd)
{
    struct page *page;
    pgoff_t idx;

    xa_for_each(&brd->brd_pages, idx, page) {
      __free_page(page);
      cond_resched();
    }

    xa_destroy(&brd->brd_pages);
}


/*
 * copy_to_brd_setup must be called before copy_to_brd. It may sleep.
 * Allocates the required pages in brd before copying n bytes
 */
static int copy_to_brd_setup(struct brd_device *brd, sector_t sector, size_t n, gfp_t gfp)
{
    unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
    size_t copy;
    int ret;

    copy = min_t(size_t, n, PAGE_SIZE - offset);
    ret = brd_insert_page(brd, sector, gfp);
    if (ret)
      return ret;
    if (copy < n) {
      sector += copy >> SECTOR_SHIFT;
      ret = brd_insert_page(brd, sector, gfp);
    }
    return ret;
}


// static void discard_from_brd(struct brd_device *brd, sector_t sector, size_t n)
// {
//     while (n >= PAGE_SIZE) {
//         brd_zero_page(brd, sector);
//       sector += PAGE_SIZE >> SECTOR_SHIFT;
//       n -= PAGE_SIZE;
//     }
// }


/*
 * Copy n bytes from src to the brd starting at sector. Does not sleep.
 */
static void copy_to_brd(struct brd_device *brd, const void *src, sector_t sector, size_t n)
{
    struct page *page;
    void *dst;
    unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
    size_t copy;

    copy = min_t(size_t, n, PAGE_SIZE - offset);
    page = brd_lookup_page(brd, sector);
    BUG_ON(!page);

    dst = kmap_atomic(page);
    memcpy(dst + offset, src, copy);
    kunmap_atomic(dst);

    if (copy < n) {
      src += copy;
      sector += copy >> SECTOR_SHIFT;
      copy = n - copy;
      page = brd_lookup_page(brd, sector);
      BUG_ON(!page);

      dst = kmap_atomic(page);
      memcpy(dst, src, copy);
      kunmap_atomic(dst);
    }
}


/*
 * Copy n bytes to dst from the brd starting at sector. Does not sleep.
 */
static void copy_from_brd(void *dst, struct brd_device *brd, sector_t sector, size_t n)
{
    struct page *page;
    void *src;
    unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
    size_t copy;

    copy = min_t(size_t, n, PAGE_SIZE - offset);
    page = brd_lookup_page(brd, sector);
    if (page) {
        // Present in the new xarray, so this page has been modified.
        src = kmap_atomic(page);
        memcpy(dst, src + offset, copy);
        kunmap_atomic(src);
    } else if (brd->parent_brd && (page = brd_lookup_page(brd->parent_brd, sector))) {
        // Present in the old xarray, so this page has not been modified.
        src = kmap_atomic(page);
        memcpy(dst, src + offset, copy);
        kunmap_atomic(src);
    } else {
          // Page doesn't exist in either of the xarray, so it must never have been written
          memset(dst, 0, copy);
    }

    if (copy < n) {
        dst += copy;
        sector += copy >> SECTOR_SHIFT;
        copy = n - copy;
        page = brd_lookup_page(brd, sector);
        if (page) {
            src = kmap_atomic(page);
            memcpy(dst, src, copy);
            kunmap_atomic(src);
        } else if (brd->parent_brd &&
          (page = brd_lookup_page(brd->parent_brd, sector))) {
            // Present in the old radix tree so this page has not been modified.
            src = kmap_atomic(page);
            memcpy(dst, src, copy);
            kunmap_atomic(src);
      } else {
            memset(dst, 0, copy);
      }         
    }
}


/*
 * Process a single bvec (one chunk) of a bio (block I/O).
 */
static int brd_do_bvec(struct brd_device *brd, struct page *page,
      unsigned int len, unsigned int off, blk_opf_t opf, sector_t sector)
{
    void *mem;
    int err = 0;

    if (op_is_write(opf)) {
        /*
        * Must use NOIO because we don't want to recurse back into the
        * block or filesystem layers from page reclaim.
        */
        gfp_t gfp = opf & REQ_NOWAIT ? GFP_NOWAIT : GFP_NOIO;

        err = copy_to_brd_setup(brd, sector, len, gfp);
        if (err)
            goto out;
    }

    mem = kmap_atomic(page);
    if (!op_is_write(opf)) {
        copy_from_brd(mem + off, brd, sector, len);
        flush_dcache_page(page);
    } else {
        flush_dcache_page(page);
        copy_to_brd(brd, mem + off, sector, len);
    }
    kunmap_atomic(mem);

out:
    return err;
}


int disk_size = DEFAULT_COW_RD_SIZE;

static void brd_do_discard(struct brd_device *brd, sector_t sector, u32 size)
{
	sector_t aligned_sector = (sector + PAGE_SECTORS) & ~PAGE_SECTORS;
	struct page *page;

	size -= (aligned_sector - sector) * SECTOR_SIZE;
	xa_lock(&brd->brd_pages);
	while (size >= PAGE_SIZE && aligned_sector < disk_size * 2) {
      page = __xa_erase(&brd->brd_pages, aligned_sector >> PAGE_SECTORS_SHIFT);
      if (page)
        __free_page(page);
      aligned_sector += PAGE_SECTORS;
      size -= PAGE_SIZE;
	}
	xa_unlock(&brd->brd_pages);
}


//Used instead of brd_make_request
static void brd_submit_bio(struct bio *bio){	
    struct brd_device *brd = bio->bi_bdev->bd_disk->private_data;
    sector_t sector = bio->bi_iter.bi_sector;
    struct bio_vec bvec;
    struct bvec_iter iter;
    bool rw;

    if (bio_end_sector(bio) > get_capacity(bio->BI_DISK)) {
        bio_io_error(bio);
        return;
    }

    rw = BIO_IS_WRITE(bio);
    // bio->bi_opf |= REQ_SYNC;

    if ((rw || bio->BI_RW & BIO_DISCARD_FLAG) && !brd->is_writable) {
        bio_io_error(bio);
        return;
    }

    if (unlikely(op_is_discard(bio->bi_opf))) {
        brd_do_discard(brd, sector, bio->bi_iter.bi_size);
        bio_endio(bio);
        return;
    }

    bio_for_each_segment(bvec, bio, iter) {
      unsigned int len = bvec.bv_len;
      int err;

      /* Don't support un-aligned buffer */
      WARN_ON_ONCE((bvec.bv_offset & (SECTOR_SIZE - 1)) ||
      		(len & (SECTOR_SIZE - 1)));

      err = brd_do_bvec(brd, bvec.bv_page, len, bvec.bv_offset,
            bio->bi_opf, sector);

      if (err) {
          if (err == -ENOMEM && bio->bi_opf & REQ_NOWAIT) {
              bio_wouldblock_error(bio);
              return;
          }
          bio_io_error(bio);
          return;
		}
      sector += len >> SECTOR_SHIFT;
    }

    bio_endio(bio);
}


#ifdef CONFIG_BLK_DEV_XIP
static int brd_direct_access(struct block_device *bdev, sector_t sector,
      void **kaddr, unsigned long *pfn)
{
  struct brd_device *brd = bdev->bd_disk->private_data;
  struct page *page;

  if (!brd)
    return -ENODEV;
  if (sector & (PAGE_SECTORS-1))
    return -EINVAL;
  if (sector + PAGE_SECTORS > get_capacity(bdev->bd_disk))
    return -ERANGE;
  page = brd_insert_page(brd, sector);
  if (!page)
    return -ENOMEM;
  *kaddr = page_address(page);
  *pfn = page_to_pfn(page);

  return 0;
}
#endif

/*
  Handle custom IOCTL commands sent to the brd_device
 */
static int brd_ioctl(struct block_device *bdev, fmode_t mode,
      unsigned int cmd, unsigned long arg)
{
    int error = 0;
    struct brd_device *brd = bdev->bd_disk->private_data;

    switch (cmd) {
      case COW_BRD_SNAPSHOT:
        if (brd->is_snapshot) {
          return -ENOTTY;
        }
        brd->is_writable = false;
        break;
      case COW_BRD_UNSNAPSHOT:
        if (brd->is_snapshot) {
          return -ENOTTY;
        }
        brd->is_writable = true;
        break;
      case COW_BRD_RESTORE_SNAPSHOT:
        if (!brd->is_snapshot) {
          return -ENOTTY;
        }
        brd_free_pages(brd);
        break;
      case COW_BRD_WIPE:
        if (brd->is_snapshot) {
          return -ENOTTY;
        }
        // Assumes no snapshots are being used right now.
        brd_free_pages(brd);
        break;
      default:
        error = -ENOTTY;
    }

    return error;
}


static const struct block_device_operations brd_fops = {
    .owner =		THIS_MODULE,
    .submit_bio =		brd_submit_bio,
    .ioctl =    brd_ioctl,
#ifdef CONFIG_BLK_DEV_XIP
    .direct_access =  brd_direct_access,
#endif
};


/*
 * And now the modules code and kernel interface.
 */
int major_num = 0;
static int num_disks = 1;
static int num_snapshots = 1;
// int disk_size = DEFAULT_COW_RD_SIZE;
static int max_part = 1;
// static int part_shift;

module_param(num_disks, int, 0444);
MODULE_PARM_DESC(num_disks, "Maximum number of ram block devices");
module_param(num_snapshots, int, 0444);
MODULE_PARM_DESC(num_snapshots, "Number of ram block snapshot devices where "
    "each disk gets it's own snapshot");
module_param(disk_size, int, 0444);
MODULE_PARM_DESC(disk_size, "Size of each RAM disk in kbytes.");
module_param(max_part, int, 0444);
MODULE_PARM_DESC(max_part, "Maximum number of partitions per RAM disk");


MODULE_DESCRIPTION("Ram backed block device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(RAMDISK_MAJOR);
MODULE_ALIAS("cow_brd");

#ifndef MODULE
/* Legacy boot options - nonmodular */
static int __init ramdisk_size(char *str)
{
    disk_size = simple_strtol(str, NULL, 0);
      return 1;
}
__setup("ramdisk_size=", ramdisk_size);
#endif

/*
 * The device scheme is derived from loop.c. Keep them in synch where possible
 * (should share code eventually).
 */
static LIST_HEAD(brd_devices);
static struct dentry *brd_debugfs_dir;

static int brd_alloc(int i)
{
    struct brd_device *brd, *parent_brd;
    struct gendisk *disk;
    char buf[DISK_NAME_LEN];
    int err = -ENOMEM;
    struct queue_limits lim = {
      /*
        * This is so fdisk will align partitions on 4k, because of
        * direct_access API needing 4k alignment, returning a PFN
        * (This is only a problem on very small devices <= 4M,
        *  otherwise fdisk will align on 1M. Regardless this call
        *  is harmless)
        */
      .physical_block_size	= PAGE_SIZE,
      .max_hw_discard_sectors	= UINT_MAX,
      .max_discard_segments	= 1,
      .discard_granularity	= PAGE_SIZE,
      .features		= BLK_FEAT_SYNCHRONOUS |
              BLK_FEAT_NOWAIT,
    };

    list_for_each_entry(brd, &brd_devices, brd_list)
        if (brd->brd_number == i)
            return -EEXIST;

    brd = kzalloc(sizeof(*brd), GFP_KERNEL);
    if (!brd)
      return -ENOMEM;

    brd->brd_number		= i;
    // True on disks until "snapshot" ioctl is called.
    brd->is_writable = true;
    brd->is_snapshot = i >= num_disks;
    list_add_tail(&brd->brd_list, &brd_devices);

    xa_init(&brd->brd_pages);

    if (brd->is_snapshot) {
        snprintf(buf, DISK_NAME_LEN, "cow_ram_snapshot%d_%d", i / num_disks,
          i % num_disks);
      }
    else {
      snprintf(buf, DISK_NAME_LEN, "cow_ram%d", i);
    }
    
    if (!IS_ERR_OR_NULL(brd_debugfs_dir))
        debugfs_create_u64(buf, 0444, brd_debugfs_dir,
          &brd->brd_nr_pages);

    disk = brd->brd_disk = blk_alloc_disk(&lim, NUMA_NO_NODE);
    if (IS_ERR(disk)) {
        err = PTR_ERR(disk);
        goto out_free_dev;
    }

    // brd->brd_queue = disk->queue;
    // if (!brd->brd_queue)
    //     goto out_free_dev;

    disk->major		= major_num;
    disk->first_minor	= i * max_part; 
    disk->fops		= &brd_fops;
    disk->private_data	= brd;
    disk->minors		= max_part; // max_part = 1
    strscpy(disk->disk_name, buf, DISK_NAME_LEN);
    set_capacity(disk, disk_size * 2);

    err = add_disk(disk);
    if (err)
        goto out_cleanup_disk;

    if (i >= num_disks) {
        // Set the parent pointer for this device.
        list_for_each_entry(parent_brd, &brd_devices, brd_list) {
          if (parent_brd->brd_number == i % num_disks) {
            brd->parent_brd = parent_brd;
            break;
          }
        }
    } else {
        brd->parent_brd = NULL;
    }

    return 0;

out_cleanup_disk:
    put_disk(disk);
out_free_dev:
    list_del(&brd->brd_list);
    kfree(brd);
    return err;
}


// static void brd_probe(dev_t dev)
// {
//     brd_alloc(MINOR(dev) / max_part);
// }


static void brd_cleanup(void)
{
    struct brd_device *brd, *next;

    debugfs_remove_recursive(brd_debugfs_dir);

    list_for_each_entry_safe(brd, next, &brd_devices, brd_list) {

        pr_info("cow: cleaning up brd num %d\n", brd->brd_number);

        del_gendisk(brd->brd_disk);
        put_disk(brd->brd_disk);
        brd_free_pages(brd);
        list_del(&brd->brd_list);
        kfree(brd);
    }
}


static inline void brd_check_and_reset_par(void)
{
    if (unlikely(!max_part))
        max_part = 1;

    /*
    * make sure 'max_part' can be divided exactly by (1U << MINORBITS),
    * otherwise, it is possiable to get same dev_t when adding partitions.
    */
    if ((1U << MINORBITS) % max_part != 0) // MINORBITS = 20
        max_part = 1UL << fls(max_part);

    if (max_part > DISK_MAX_PARTS) {
        pr_info("brd: max_part can't be larger than %d, reset max_part = %d.\n",
          DISK_MAX_PARTS, DISK_MAX_PARTS);
        max_part = DISK_MAX_PARTS;
    }
}


static int __init brd_init(void)
{
    printk(KERN_WARNING DEVICE_NAME ": Initializing device\n");

    int err, i;

    brd_check_and_reset_par();
    brd_debugfs_dir = debugfs_create_dir("ramdisk_pages", NULL);

    // 1 base + #Snapshots
    const int nr = num_disks * (1 + num_snapshots);
    printk(KERN_WARNING DEVICE_NAME ": Counting NR %d\n", nr);

    major_num = register_blkdev(major_num, DEVICE_NAME);
    if (major_num <= 0) {
      printk(KERN_WARNING DEVICE_NAME ": unable to get major number\n");
      return -EIO;
    }

    for (i = 0; i < nr; i++) {
        err = brd_alloc(i);
        if (err)
            goto out_free;
    }
    /*
    * brd module now has a feature to instantiate underlying device
    * structure on-demand, provided that there is an access dev node.
    *
    * (1) if rd_nr is specified, create that many upfront. else
    *     it defaults to CONFIG_BLK_DEV_RAM_COUNT
    * (2) User can further extend brd devices by create dev node themselves
    *     and have kernel automatically instantiate actual device
    *     on-demand. Example:
    *		mknod /path/devnod_name b 1 X	# 1 is the rd major
    *		fdisk -l /path/devnod_name
    *	If (X / max_part) was not already created it will be created
    *	dynamically.
    */

    pr_info("cow_brd: module loaded\n");
    printk(KERN_INFO DEVICE_NAME ": module loaded with %d disks and %d snapshots"
        "\n", num_disks, num_disks * num_snapshots);
    return 0;

out_free:
    brd_cleanup();
    pr_info("cow_brd: module NOT loaded !!!\n");
    return err;
}

static void __exit brd_exit(void)
{
    pr_info("cow_brd: Cleaning up the device...\n");
    brd_cleanup();
    unregister_blkdev(major_num, DEVICE_NAME);
    
    pr_info("brd: module unloaded\n");
}

module_init(brd_init);
module_exit(brd_exit);
