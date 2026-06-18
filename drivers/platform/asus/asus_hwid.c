
/*
** =========================================================================
** File:
**     asus_hwid.c
**
** Description:
**     Export function for getting HW ID.
**
** =========================================================================
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h> // for copying from/to user space

#include <linux/asus_hwid.h>

/* === module control definitions === */
#define MODULE_NAME                             "asus_hwid"
#define ASUS_HWID                              "/dev/"MODULE_NAME
#define ASUS_HWID_MAGIC_NUMBER                 0x58585858
#define ASUS_HWID_IOCTL_GROUP                  0x52
#define ASUS_HWID_SET_DEVICE_PARAMETER         _IO(ASUS_HWID_IOCTL_GROUP, 1)
#define ASUS_HWID_SET_MAGIC_NUMBER       	  _IO(ASUS_HWID_IOCTL_GROUP, 2)
#define ASUS_HWID_SET_DBG_LEVEL                _IO(ASUS_HWID_IOCTL_GROUP, 3)
#define ASUS_HWID_GET_DBG_LEVEL                _IO(ASUS_HWID_IOCTL_GROUP, 4)
#define ASUS_HWID_GET_DEVICE_STATUS            _IO(ASUS_HWID_IOCTL_GROUP, 5)
#define ASUS_HWID_SHOW_DEBUG_MESSAGE           _IO(ASUS_HWID_IOCTL_GROUP, 6)


/* === debug method === */
#define DEBUG_LEVEL_CRITICAL	1
#define DEBUG_LEVEL_INFO	2
#define DEBUG_LEVEL_VERBOSE	3
static int g_debug_level = DEBUG_LEVEL_VERBOSE;

inline int asus_hwid_printk_critical(const char *fmt, ...)
{
	if(g_debug_level>=DEBUG_LEVEL_CRITICAL)
		return printk(fmt);

	return 0;
}
inline int asus_hwid_printk_info(const char *fmt, ...)
{
	if(g_debug_level>=DEBUG_LEVEL_INFO)
		return printk(fmt);

	return 0;
}
inline int asus_hwid_printk_verbose(const char *fmt, ...)
{
	if(g_debug_level>=DEBUG_LEVEL_VERBOSE)
		return printk(fmt);

	return 0;
}

#define MAX_MESSAGE_BUFFER	256
static char message_buff[MAX_MESSAGE_BUFFER];


/* === hardware id parsing method === */

#define HWID_CMDLINE_MAX_LENGTH  128
static char asus_hw_id_str[HWID_CMDLINE_MAX_LENGTH] = {0};
module_param_string(info, asus_hw_id_str, HWID_CMDLINE_MAX_LENGTH,0600);
MODULE_PARM_DESC(info,
	"asus_hwid.info=The hardware id information from abl.");


#ifdef CONFIG_ASUS_PICASSO_QTI_EMBEDDED

static void parsing_hardware_id(void);
#if 0
enum DEVICE_HWID g_ASUS_hwID = HW_REV_INVALID;
static int set_hardware_id(char *str)
{
        if ( strcmp("0", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_EVB;
                printk("Kernel HW ID = PICASSO_EVB\n");
        }
	 else if ( strcmp("1", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_EVB2;
                printk("Kernel HW ID = PICASSO_EVB2\n");
        }
        else if ( strcmp("2", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_SR;
                printk("Kernel HW ID = PICASSO_SR\n");
        }
        else if ( strcmp("3", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_ER;
                printk("Kernel HW ID = PICASSO_ER\n");
        }
        else if ( strcmp("4", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_ER2;
                printk("Kernel HW ID = PICASSO_ER2\n");
        }
        else if ( strcmp("5", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_PR;
                printk("Kernel HW ID = PICASSO_PR\n");
        }
        else if ( strcmp("6", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_PR2;
                printk("Kernel HW ID = PICASSO_PR2\n");
        }
        else if ( strcmp("7", str) == 0 )
        {
                g_ASUS_hwID = HW_REV_MP;
                printk("Kernel HW ID = PICASSO_MP\n");
        }

        printk("g_Asus_hwID = %d\n", g_ASUS_hwID);
        return 0;
}
#ifdef CONFIG_MACH_ASUS
__setup("androidboot.id.stage=", set_hardware_id);
EXPORT_SYMBOL(g_ASUS_hwID);
#endif
#endif

static void parsing_hardware_id(void) {
#if 1    
    int project_id = 0;
    int hw_id = 0;
    int sku_id = 0;
    int ddr_id = 0;
    int nfc_id = 0;
    int rf_id = 0;
	char *pstr = asus_hw_id_str;
    char value[16];

    printk("asus_hw_id_str: %s\n",asus_hw_id_str);

    /* format from ABL
    	" asus_hwid.info=0x%02x0x%02x0x%02x0x%02x0x%02x0x%02x", 
     	PJ_ID,ZS675KW_HW_ID,ZS675KW_SKU_ID,ZS675KW_DDR_ID,ZS675KW_NFC_ID,ZS675KW_RF_ID);
    */

    value[4] = 0;
    // PJ_ID
	pstr = strstr(pstr, "0x");
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &project_id)) {
            printk("Error to parsing: PJ_ID\n");
		} else {
            printk("asus_hwid: PJ_ID=%d\n",project_id);
        }
	}

    // HW_ID
    pstr = pstr+4;
	pstr = strstr(pstr, "0x");
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &hw_id)) {
            printk("Error to parsing: HW_ID\n");
		} else {
            printk("asus_hwid: HW_ID=%d\n",hw_id);
        }
	}

    // SKU_ID
    pstr = pstr+4;
	pstr = strstr(pstr, "0x");
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &sku_id)) {
            printk("Error to parsing: SKU_ID\n");
		} else {
            printk("asus_hwid: SKU_ID=%d\n",sku_id);
        }
	}


    // DDR_ID
    pstr = pstr+4;
	pstr = strstr(pstr, "0x");
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &ddr_id)) {
            printk("Error to parsing: DDR_ID\n");
		} else {
            printk("asus_hwid: DDR_ID=%d\n",ddr_id);
        }
	}

    // NFC_ID
    pstr = pstr+4;
	pstr = strstr(pstr, "0x");;
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &nfc_id)) {
            printk("Error to parsing: NFC_ID\n");
		} else {
            printk("asus_hwid: NFC_ID=%d\n",nfc_id);
        }
	}

    // RF_ID
    pstr = pstr+4;
	pstr = strstr(pstr, "0x");
	if (pstr) {
        memcpy(value, pstr, 4);
		if (kstrtoint(value, 16, &rf_id)) {
            printk("Error to parsing: RF_ID\n");
		} else {
            printk("asus_hwid: RF_ID=%d\n",rf_id);
        }
	}
#else
    printk("asus_hw_id_str: %s\n",asus_hw_id_str);
#endif
}

#endif //CONFIG_ASUS_PICASSO_QTI_EMBEDDED



/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static struct file_operations fops =
{
	.owner =            THIS_MODULE,
	.read =             read,
	.write =            write,
	.unlocked_ioctl =            unlocked_ioctl,
	.open =             open,
	.release =          release,
	.llseek =           default_llseek    /* using default implementation as declared in linux/fs.h */
};
static struct miscdevice miscdev =
{
    .minor =    MISC_DYNAMIC_MINOR,
    .name =     MODULE_NAME,
    .fops =     &fops
};

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv =
{
	.suspend =  suspend,
	.resume =   resume,
	.driver =
	{
		.name = MODULE_NAME,
	},
};

static void platform_release(struct device *dev);
static struct platform_device platdev =
{
	.name =     MODULE_NAME,
	.id =       -1,                     /* means that there is only one device */
	.dev =
	{
		.platform_data = NULL,
		.release = platform_release,    /* a warning is thrown during rmmod if this is absent */
	},
};

/* Module info */
MODULE_AUTHOR("Tony TK Yu");
MODULE_DESCRIPTION("ASUS_HWID Kernel Module");
MODULE_LICENSE("GPL v2");

static int __init asus_hwid_init(void)
{
	int err_ret;

	asus_hwid_printk_info("asus_hwid: init_module +++.\n");

    parsing_hardware_id();

	err_ret = misc_register(&miscdev);
	if (err_ret)
	{
		asus_hwid_printk_critical("asus_hwid: misc_register failed.\n");
		return err_ret;
	}

	err_ret = platform_device_register(&platdev);
	if (err_ret)
	{
		asus_hwid_printk_critical("asus_hwid: platform_device_register failed.\n");
	}

	err_ret = platform_driver_register(&platdrv);
	if (err_ret)
	{
		asus_hwid_printk_critical("asus_hwid: platform_driver_register failed.\n");
	}

	asus_hwid_printk_info("asus_hwid: init_module ---.\n");
	return 0;
}

static void __exit asus_hwid_exit(void)
{
	asus_hwid_printk_info("asus_hwid: cleanup_module.\n");

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

	misc_deregister(&miscdev);
}

static int open(struct inode *inode, struct file *file)
{
	asus_hwid_printk_info("asus_hwid: open.\n");
    parsing_hardware_id();

	if (!try_module_get(THIS_MODULE)) {
		asus_hwid_printk_critical("asus_hwid: open fail.\n");
		return -ENODEV;
	}

	return 0;
}

static int release(struct inode *inode, struct file *file)
{
	asus_hwid_printk_info("asus_hwid: release.\n");

	file->private_data = (void*)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
//@TODO: need implementation

	return 0;
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	*ppos = 0;  /* file position not used, always set to 0 */

	/*
	** Prevent unauthorized caller to write data.
	** TouchSense service is the only valid caller.
	*/
	if (file->private_data != (void*)ASUS_HWID_MAGIC_NUMBER)
	{
		asus_hwid_printk_critical("asus_hwid: unauthorized write.\n");
		return -EACCES;
	}

	if ((count <= 0) || (count>(MAX_MESSAGE_BUFFER-1)))
	{
		asus_hwid_printk_critical("asus_hwid: invalid buffer size.\n");
		return -EINVAL;
	}

	/* Copy immediately the input buffer */
	if (0 != copy_from_user(message_buff, buf, count))
	{
		/* Failed to copy all the data, exit */
		asus_hwid_printk_critical("asus_hwid: copy_from_user failed.\n");
		return -EIO;
	}

	printk("asus_hwid: message: %s", message_buff);

	return count;
}

static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case ASUS_HWID_SET_MAGIC_NUMBER:
			asus_hwid_printk_critical("asus_hwid: ioctl: ASUS_HWID_SET_MAGIC_NUMBER.\n");
			file->private_data = (void*)ASUS_HWID_MAGIC_NUMBER;
			break;

		default:
			asus_hwid_printk_critical("asus_hwid: ioctl: default.\n");
			break;
	}
	return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state)
{
	asus_hwid_printk_info("asus_hwid: suspend.\n");
	return 0;
}

static int resume(struct platform_device *pdev)
{
	asus_hwid_printk_info("asus_hwid: resume.\n");

	return 0;   /* can resume */
}

static void platform_release(struct device *dev)
{
	asus_hwid_printk_info("asus_hwid: platform_release.\n");
}

module_init(asus_hwid_init);
module_exit(asus_hwid_exit);
