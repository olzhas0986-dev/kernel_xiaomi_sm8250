/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 */

#ifndef _ZRAM_DRV_H_
#define _ZRAM_DRV_H_

#include <linux/rwsem.h>
#include <linux/zsmalloc.h>
#include <linux/crypto.h>
#include <linux/spinlock.h>

#include "zcomp.h"
#include "zram_dedup.h"

#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define ZRAM_LOGICAL_BLOCK_SHIFT 12
#define ZRAM_LOGICAL_BLOCK_SIZE	(1 << ZRAM_LOGICAL_BLOCK_SHIFT)
#define ZRAM_SECTOR_PER_LOGICAL_BLOCK	\
	(1 << (ZRAM_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))


/*
 * ZRAM is mainly used for memory efficiency so we want to keep memory
 * footprint small and thus squeeze size and zram pageflags into a flags
 * member. The lower ZRAM_FLAG_SHIFT bits is for object size (excluding
 * header), which cannot be larger than PAGE_SIZE (requiring PAGE_SHIFT
 * bits), the higher bits are for zram_pageflags.
 *
 * We use BUILD_BUG_ON() to make sure that zram pageflags don't overflow.
 */
#define ZRAM_FLAG_SHIFT (PAGE_SHIFT + 1)

/* Flags for zram pages (table[page_no].flags) */
enum zram_pageflags {
	/* zram slot is locked */
	ZRAM_LOCK = ZRAM_FLAG_SHIFT,
	ZRAM_SAME,	/* Page consists the same element */
	ZRAM_WB,	/* page is stored on backing_device */
	ZRAM_UNDER_WB,	/* page is under writeback */
	ZRAM_HUGE,	/* Incompressible page */
	ZRAM_IDLE,	/* not accessed page since last idle marking */
	ZRAM_IMPORTANT,	/* the important page */

	__NR_ZRAM_PAGEFLAGS,
};

/*-- Data structures */

struct zram_entry {
	struct rb_node rb_node;
	u32 len;
	u32 checksum;
	unsigned long refcount;
	unsigned long handle;
};

/* Allocated for each disk page */
struct zram_table_entry {
	union {
		struct zram_entry *entry;
		unsigned long element;
	};
	unsigned long flags;
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	ktime_t ac_time;
#endif
};

struct zram_stats {
	atomic64_t compr_data_size;	/* compressed size of pages stored */
	atomic64_t failed_reads;	/* can happen when memory is too low */
	atomic64_t failed_writes;	/* can happen when memory is too low */
	atomic64_t invalid_io;	/* non-page-aligned I/O requests */
	atomic64_t notify_free;	/* no. of swap slot free notifications */
	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */
	atomic64_t important_pages;	/* no. of important pages */
	atomic64_t important_compr_data_size;     /* important compressed size of pages stored */
	atomic64_t huge_pages_since;	/* no. of huge pages since zram set up */
	atomic64_t pages_stored;	/* no. of pages currently stored */
	atomic_long_t max_used_pages;	/* no. of maximum pages stored */
	atomic64_t writestall;		/* no. of write slow paths */
	atomic64_t miss_free;		/* no. of missed free */
#ifdef	CONFIG_ZRAM_WRITEBACK
	atomic64_t bd_count;		/* no. of pages in backing device */
	atomic64_t bd_reads;		/* no. of reads from backing device */
	atomic64_t bd_writes;		/* no. of writes from backing device */
#endif
	atomic64_t dup_data_size;	/*
					 * compressed size of pages
					 * duplicated
					 */
	atomic64_t meta_data_size;	/* size of zram_entries */
};

struct zram_hash {
	spinlock_t lock;
	struct rb_root rb_root;
};

struct zram {
	struct zram_table_entry *table;
	struct zs_pool *mem_pool;
	struct zcomp *comp;
	struct gendisk *disk;
	struct zram_hash *hash;
	size_t hash_size;
	/* Prevent concurrent execution of device init */
	struct rw_semaphore init_lock;
	/*
	 * the number of pages zram can consume for storing compressed data
	 */
	unsigned long limit_pages;

	struct zram_stats stats;
	/*
	 * This is the limit on amount of *uncompressed* worth of data
	 * we can store in a disk.
	 */
	u64 disksize;	/* bytes */
	char compressor[CRYPTO_MAX_ALG_NAME];
	/*
	 * zram is claimed so open request will be failed
	 */
	bool claim; /* Protected by bdev->bd_mutex */
	bool use_dedup;
#ifdef CONFIG_ZRAM_WRITEBACK
	struct file *backing_dev;
	spinlock_t wb_limit_lock;
	bool wb_limit_enable;
	u64 bd_wb_limit;
	struct block_device *bdev;
	unsigned int old_block_size;
	unsigned long *bitmap;
	unsigned long nr_pages;
#endif
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	struct dentry *debugfs_dir;
#endif
};

static inline bool zram_dedup_enabled(struct zram *zram)
{
#ifdef CONFIG_ZRAM_DEDUP
	return zram->use_dedup;
#else
	return false;
#endif
}

void zram_entry_free(struct zram *zram, struct zram_entry *entry);
#endif
