#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk priority macros 
#include <asm/current.h>       // process information, just for fun 
#include <linux/sched.h>       // for "struct task_struct" 

static int __init mod_init(void)
{
     printk(KERN_INFO "hx711 module being loaded.\n");
     printk(KERN_INFO "User space process is '%s'\n", current->comm);
     printk(KERN_INFO "User space PID is  %i\n", current->pid);
     return 0;
}  

static void __exit mod_exit(void) 
{
     printk(KERN_INFO "hx7111 module being unloaded.\n"); 
}  

module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR("Chuankai Lin <chuankai@kai.idv.tw>"); 
MODULE_LICENSE("Dual BSD/GPL"); 
MODULE_DESCRIPTION("hx711 driver");
