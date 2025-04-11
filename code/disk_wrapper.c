#include <linux/blkdev.h>

MODULE_LICENSE("GPL");

struct hello {
  bool log_on;
  u8 target_partno;
};



static int __init disk_wrapper_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");
  struct hello hi;
  return 0;
}

static void __exit hello_cleanup(void) {
  printk(KERN_INFO "hwm: Cleaning up bye!\n");
}

module_init(disk_wrapper_init);
module_exit(hello_cleanup);
