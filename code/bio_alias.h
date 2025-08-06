/*
* Copyright (c) 2024–2025 Yifei Liu
* Copyright (c) 2024–2025 Geoff Kuenning 
* Copyright (c) 2024–2025 Md. Kamal Parvez
* Copyright (c) 2024–2025 Scott Smolka
* Copyright (c) 2024–2025 Erez Zadok
* Copyright (c) 2024–2025 Stony Brook University
* Copyright (c) 2024–2025 The Research Foundation of SUNY
*
* This software is released under the Apache License, Version 2.0.
* You may obtain a copy of the License at:
*     http://www.apache.org/licenses/LICENSE-2.0
*
* This work is associated with the paper:
* "Enhanced File System Testing through Input and Output Coverage,"
* published in the Proceedings of the ACM SYSTOR 2025.
*/


#ifndef CRASHMONKEY_BIO_ALIAS_H
#define CRASHMONKEY_BIO_ALIAS_H

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) 

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_size
#define BI_SECTOR               bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio, err)
#define BIO_IO_ERR(bio, err)    bio_endio(bio, err)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)) || \
  (LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0))

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio, err)
#define BIO_IO_ERR(bio, err)    bio_endio(bio, err)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))


#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)

#define BI_RW                   bi_opf
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_OP_DISCARD
#define BIO_IS_WRITE(bio)       op_is_write(bio_op(bio))

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)  \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)) || \
  (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)       \
  && LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 3)) || \
  (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)       \
  && LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 7))

#define BI_RW                   bi_opf
#define BI_DISK                 bi_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_OP_DISCARD
#define BIO_IS_WRITE(bio)       op_is_write(bio_op(bio))

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0))

#define BI_RW                   bi_opf
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_OP_DISCARD
#define BIO_IS_WRITE(bio)       op_is_write(bio_op(bio))

#else
#error "Unsupported kernel version: CrashMonkey has not been tested with " \
  "your kernel version."
#endif

#endif //CRASHMONKEY_BIO_ALIAS_H
