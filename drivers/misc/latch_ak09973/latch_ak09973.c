#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/hwmon-sysfs.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>

#define LATCH_DEBUG_LOG 1

#ifdef LATCH_DEBUG_LOG
	#define log_ak09973(fmt, args...) printk(KERN_INFO"[%s] "fmt,DRIVER_NAME,##args)
	#define err_ak09973(fmt, args...) printk(KERN_ERR "[%s] "fmt,DRIVER_NAME,##args)
#else
	#define log_ak09973(fmt, args...)
	#define err_ak09973(fmt, args...)
#endif

#define DRIVER_NAME                   "latch_ak09973"
#define INT_NAME                      "LatchAk09973_INT"
#define REPORT_WAKE_LOCK_TIMEOUT      (1 * HZ)
#define DELAYED_WORK_TIME             500

static struct latch_ak09973_str {
    int sleep;
	int status;
	int enable;
	int hardcode;
    int debounce;
    struct i2c_client     *client;
	struct wakeup_source  *wake_src;
	struct regulator      *vdd_supply;
	struct mutex          latch_mutex;
	struct delayed_work   latch_ak09973_work;
}* latch_ak09973_dev;

struct device *dev = NULL;
static struct workqueue_struct 	*latch_ak09973_wq;
static struct i2c_client        *report_client;
static int major;
static int g_state=-1;
static int is_suspend=0;
static int g_threshold1X=550;
static int g_threshold2X=550;
static int g_threshold1Y=400;
static int ASUS_LATCH_AK09973_GPIO;
static int ASUS_LATCH_AK09973_IRQ;

static int latch_ak09973_suspend(struct device *dev)
{
	log_ak09973("latch_ak09973 SUSPEND +++\n");
	is_suspend = 1;
	log_ak09973("latch_ak09973 SUSPEND ---\n");
	return 0;
}

static int latch_ak09973_resume(struct device *dev)
{
	log_ak09973("latch_ak09973 RESUME +++\n");
	is_suspend = 0;
	log_ak09973("latch_ak09973 RESUME ---\n");
	return 0;
}

static int i2c_write_bytes(struct i2c_client *client, char *write_buf, int writelen)
{
	struct i2c_msg msg;
	int ret=-1;

	msg.flags = !I2C_M_RD;		//write
	msg.addr = client->addr;
	msg.len = writelen;
	msg.buf = write_buf;

	ret = i2c_transfer(client->adapter,&msg, 1);
	if(ret <= 0)
	{
		err_ak09973("[latch_ak09973] %s error %d,will retry\n",__func__,ret);
		ret = i2c_transfer(client->adapter,&msg, 1);
		if(ret <= 0)
		{
			err_ak09973("[latch_ak09973] %s error %d\n",__func__,ret);
		}
	}
	return ret;
}

static int i2c_read_bytes(struct i2c_client *client, short addr, char *data)
{
	int err = 0;
	unsigned char buf[16] = {0};
	struct i2c_msg msgs;
	buf[0] = addr & 0xFF;

	err = i2c_write_bytes(client, buf, 1);
	if (err !=1)
		err_ak09973("[latch_ak09973] i2c_write_bytes:err %d\n", err);

	msleep(1);//wait for ic

	msgs.flags = I2C_M_RD;		//read
	msgs.addr = client->addr;
	msgs.len = 8;
	msgs.buf = data;

	err = i2c_transfer(client->adapter,&msgs, 1);
	if(err <= 0)
	{
		err_ak09973("[latch_ak09973] %s error %d,will retry\n",__func__,err);
		err = i2c_transfer(client->adapter,&msgs, 1);
		if(err <= 0)
		{
			err_ak09973("[latch_ak09973] %s error %d\n",__func__,err);
		}
	}
	return err;
}

static irqreturn_t latch_ak09973_reenable_irq(int irq, void *dev_id)
{

	unsigned char data[8]={0};
	int err = 0;
	int SWX=-1;
	int SWY=-1;

	if(is_suspend == 1){
		log_ak09973("%s,is_suspend=1\n",__func__);
		__pm_wakeup_event(latch_ak09973_dev->wake_src, REPORT_WAKE_LOCK_TIMEOUT+(4 * HZ));
		queue_delayed_work(latch_ak09973_wq, &latch_ak09973_dev->latch_ak09973_work, msecs_to_jiffies(DELAYED_WORK_TIME*2));
        return IRQ_HANDLED;
    }

	log_ak09973("[ISR] %s latch_ak09973_interrupt = %d\n",__func__,ASUS_LATCH_AK09973_IRQ);
	__pm_wakeup_event(latch_ak09973_dev->wake_src, REPORT_WAKE_LOCK_TIMEOUT+(1 * HZ));
	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(report_client, 0x17, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] %s:err %d\n",__func__,err);
		return IRQ_HANDLED;
	}

	SWX = (data[0]&0x2)>>1;  //ST-D1
	SWY = (data[0]&0x4)>>2;  //ST-D2
	err = cancel_delayed_work(&latch_ak09973_dev->latch_ak09973_work);
	if(err == 1 ){
		log_ak09973("cancel pending work  SWX=%d  SWY=%d\n",SWX,SWY);
	}else{
		log_ak09973("no pending work  SWX=%d  SWY=%d\n",SWX,SWY);
	}
	

	queue_delayed_work(latch_ak09973_wq, &latch_ak09973_dev->latch_ak09973_work, msecs_to_jiffies(DELAYED_WORK_TIME));

	return IRQ_HANDLED;
}


static void debounce_latch_ak09973_report_function(struct work_struct *dat)
{
//	struct i2c_client *client = to_i2c_client(&client->dev);
	unsigned char data[8]={0};
	int err = 0;
	char * envp[2];
	int X_value=0;
	int Y_value=0;
	int SWX=-1;
	int SWY=-1;

	__pm_wakeup_event(latch_ak09973_dev->wake_src, REPORT_WAKE_LOCK_TIMEOUT);
	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(report_client, 0x17, data);
	if (err != 1)
		err_ak09973("[latch_ak09973] show mode:err %d\n", err);

	mutex_unlock(&latch_ak09973_dev->latch_mutex);

	SWX = (data[0]&0x2)>>1;  //ST-D1
	SWY = (data[0]&0x4)>>2;  //ST-D2
	if(data[6] >= 128)
		X_value = ~((data[5]^255)* 256 + (data[6]^255));
	else
	        X_value = data[5]*256 + data[6];

	if(data[3] >= 128)
		Y_value = ~((data[3]^255)* 256 + (data[4]^255));
	else
		Y_value = data[3]*256 + data[4];

	log_ak09973("interrupt  X_value=%d SWX=%d Y_value=%d SWY=%d\n",X_value,SWX,Y_value,SWY);

	if( ((SWX==0) && (SWY==0)) )
	{
		envp[0] = "STATUS=CLOSE";
		envp[1] = NULL;
		kobject_uevent_env(&report_client->dev.kobj, KOBJ_CHANGE, envp);
		g_state=2;
		if((SWX==0) && (SWY==0))
			log_ak09973("old latch g_threshold1X=%d\n",g_threshold1X);
		else
			log_ak09973("new latch g_threshold2X=%d\n",g_threshold2X);
	}else if( ((SWX==0) && (SWY==1)) )
	{
		envp[0] = "STATUS=OPEN";
		envp[1] = NULL;
		kobject_uevent_env(&report_client->dev.kobj, KOBJ_CHANGE, envp);
		g_state=1;
		if((SWX==0) && (SWY==1))
			log_ak09973("old latch g_threshold1X=%d\n",g_threshold1X);
		else
			log_ak09973("new latch g_threshold2X=%d\n",g_threshold2X);
	}else if(SWX==1)
	{
		envp[0] = "STATUS=NONE";
		envp[1] = NULL;
		kobject_uevent_env(&report_client->dev.kobj, KOBJ_CHANGE, envp);
		g_state=0;
	}

}

static ssize_t show_action_mode(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8]={0};
	int err = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x21, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] show mode:err %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x21 error");
	}
	log_ak09973("[latch_ak09973] %s  mode=%x\n",__func__,data[0]);
	return snprintf(buf,  PAGE_SIZE,"mode %x\n", data[0]);
}

static int latch_write_bytes(struct i2c_client *client, short addr, char value)
{
	int err = 0;
	unsigned char buf[16] = {0};

	buf[0] = addr & 0xFF;
	buf[1] = value;

	err = i2c_write_bytes(client, buf, 2);
	if (err !=1)
		err_ak09973("[latch_ak09973] i2c_write_bytes:err %d\n", err);
	return err;
}

static int latch_write_threshold(struct i2c_client *client, short addr, int bop1x,int brp1x)
{
	int err = 0;
	unsigned char buf[16] = {0};

	buf[0] = addr & 0xFF; //0x22 set x threshole;  0x23,set y threshold
	buf[1] = (bop1x & 0xFF00)>>8; //bopx high byte
	buf[2] = bop1x & 0xFF; //bopx low byte
	buf[3] = (brp1x & 0xFF00)>>8;  //brpx high byte
	buf[4] = brp1x & 0xFF;  //brpx low byte

	err = i2c_write_bytes(client, buf, 5);
	if (err !=1)
		err_ak09973("[latch_ak09973] i2c_write_bytes:err %d\n", err);

	return err;
}


static ssize_t store_X_threshold(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int request=0;
	struct i2c_client *client = to_i2c_client(dev);

	sscanf(buf, "%d", &request);

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	ret=latch_write_threshold(client,0x22,request+32,request-32);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);

	if(ret<0){
		log_ak09973("[latch_ak09973]%s: latch_write_threshold write X1threshold %d fail\n",__func__,request);
		return count;
	}

	g_threshold1X=request;
	log_ak09973("[latch_ak09973]%s: X1threshold=%d\n",__func__,request);
	return count;
}

static ssize_t show_X_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//unsigned char data[8]={0};
	//int err = 0;
	log_ak09973("[latch_ak09973]%s: g_threshold1X=%d\n",__func__,g_threshold1X);
	return snprintf(buf, PAGE_SIZE,"g_threshold1X=%d\n", g_threshold1X);
}

static ssize_t store_Y_threshold(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int request=0;
	struct i2c_client *client = to_i2c_client(dev);

	sscanf(buf, "%d", &request);

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	ret=latch_write_threshold(client,0x23,request+32,request-32);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if(ret<0){
		log_ak09973("[latch_ak09973]%s: latch_write_threshold write threshold %d fail\n",__func__,request);
		return count;
	}
	g_threshold1Y=request;
	log_ak09973("[latch_ak09973]%s: threshold=%d\n",__func__,request);
	return count;
}


static ssize_t show_Y_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//unsigned char data[8]={0};
	//int err = 0;
	log_ak09973("[latch_ak09973]%s: g_threshold1Y=%d\n",__func__,g_threshold1Y);
	return snprintf(buf, PAGE_SIZE,"g_threshold1Y=%d\n", g_threshold1Y);
}

static ssize_t store_trigger_update(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	log_ak09973("[latch_ak09973]%s: entry \n",__func__);
	queue_delayed_work(latch_ak09973_wq, &latch_ak09973_dev->latch_ak09973_work, 0);
	return count;
}

static ssize_t store_action_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int request=0 ;

	struct i2c_client *client = to_i2c_client(dev);
   	sscanf(buf, "%x", &request);

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	ret = latch_write_bytes(client, 0x21, request);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if(ret < 0)
	{
		err_ak09973("[latch_ak09973] store mode error\n");
		return count;
	}
	log_ak09973("[latch_ak09973]write %d to mode\n",request);
	return count;
}

static ssize_t show_latch_ak09973_interrupt_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8]={0};
	int err = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x20, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] interrupt show:err %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x20 error");
	}
	log_ak09973("[latch_ak09973] %s  data[0]=%d data[1]=%d\n",__func__,data[0],data[1]);
	return snprintf(buf,  PAGE_SIZE,"int %d-%d\n", data[0],data[1]);
}

static int latch_write_interrupt(struct i2c_client *client, short addr, char value)
{
	int err = 0;
	unsigned char buf[16] = {0};

	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	buf[2] = value;

	err = i2c_write_bytes(client, buf, 3);
	if (err !=1)
		err_ak09973("[latch_ak09973] i2c_write_bytes:err %d\n", err);

	return err;
}

static ssize_t store_latch_ak09973_interrupt_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int  request=0;

	struct i2c_client *client = to_i2c_client(dev);
   	sscanf(buf, "%x", &request);

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	ret = latch_write_interrupt(client, 0x20, request);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if(ret < 0)
	{
		err_ak09973("[latch_ak09973]write interrupt send error\n");
		return count;
	}
	log_ak09973("[latch_ak09973]write %d to interrupr\n",request);
	return count;
}

static ssize_t show_st_xyz(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8] = {0};
	unsigned char modedata[8] = {0};
	int err = 0;
	int sleeptime = 0;
	long int q,p,o = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x21, modedata);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x21 error");
	}

	switch(modedata[0])
	{
		case 66: sleeptime = 200;
				  break;
		case 68: sleeptime = 100;
				  break;
		case 70: sleeptime = 50;
				  break;
		case 72: sleeptime = 20;
				  break;
		case 74: sleeptime = 10;
				  break;
		case 76: sleeptime = 2;
				  break;
		case 78: sleeptime = 1;
				  break;
		default:
				  break;
	}

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x17, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
	}


	if(data[0] == 0)
	{
		msleep(sleeptime);
		mutex_lock(&latch_ak09973_dev->latch_mutex);
		err = i2c_read_bytes(client, 0x17, data);
		mutex_unlock(&latch_ak09973_dev->latch_mutex);
		if (err != 1){
			err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
			return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
		}
	}

	q = data[5] * 256 + data[6];
	p = data[3] * 256 + data[4];
	o = data[1] * 256 + data[2];
	log_ak09973("[latch_ak09973] %s  status:%d-%d z:%ld y:%ld x:%ld\n",__func__,data[0],data[1],o,p,q);
	return snprintf(buf,  PAGE_SIZE,"status:%d-%d z:%ld y:%ld x:%ld\n", data[0],data[1],o,p,q);
}

static ssize_t show_x(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8] = {0};
	unsigned char modedata[8] = {0};
	int err = 0;
	int sleeptime = 0;
	long int q,p,o = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x21, modedata);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x21 error");
	}

	switch(modedata[0])
	{
		case 66: sleeptime = 200;
				  break;
		case 68: sleeptime = 100;
				  break;
		case 70: sleeptime = 50;
				  break;
		case 72: sleeptime = 20;
				  break;
		case 74: sleeptime = 10;
				  break;
		case 76: sleeptime = 2;
				  break;
		case 78: sleeptime = 1;
				  break;
		default:
				  break;
	}

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x17, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
	}

	if(data[0] == 0)
	{
		msleep(sleeptime);
		mutex_lock(&latch_ak09973_dev->latch_mutex);
		err = i2c_read_bytes(client, 0x17, data);
		mutex_unlock(&latch_ak09973_dev->latch_mutex);
		if (err != 1){
			err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
			return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
		}
	}

	q = data[5] * 256 + data[6];
	p = data[3] * 256 + data[4];
	o = data[1] * 256 + data[2];

	if(data[5] >= 128)
		q = ~((data[5]^255)* 256 + (data[6]^255));

	if(data[3] >= 128)
		p = ~((data[3]^255)* 256 + (data[4]^255));

	if(data[1] >= 128)
		o = ~((data[1]^255)* 256 + (data[2]^255));
	log_ak09973("[latch_ak09973] %s  x=%d\n",__func__,q);
	return snprintf(buf,  PAGE_SIZE,"%d\n",q);
}

static ssize_t show_y(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8] = {0};
	unsigned char modedata[8] = {0};
	int err = 0;
	int sleeptime = 0;
	long int q,p,o = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x21, modedata);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x21 error");
	}

	switch(modedata[0])
	{
		case 66: sleeptime = 200;
				  break;
		case 68: sleeptime = 100;
				  break;
		case 70: sleeptime = 50;
				  break;
		case 72: sleeptime = 20;
				  break;
		case 74: sleeptime = 10;
				  break;
		case 76: sleeptime = 2;
				  break;
		case 78: sleeptime = 1;
				  break;
		default:
				  break;
	}

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x17, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
	}

	if(data[0] == 0)
	{
		msleep(sleeptime);
		mutex_lock(&latch_ak09973_dev->latch_mutex);
		err = i2c_read_bytes(client, 0x17, data);
		mutex_unlock(&latch_ak09973_dev->latch_mutex);
		if (err != 1){
			err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
			return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
		}
	}

	q = data[5] * 256 + data[6];
	p = data[3] * 256 + data[4];
	o = data[1] * 256 + data[2];

	if(data[5] >= 128)
		q = ~((data[5]^255)* 256 + (data[6]^255));

	if(data[3] >= 128)
		p = ~((data[3]^255)* 256 + (data[4]^255));

	if(data[1] >= 128)
		o = ~((data[1]^255)* 256 + (data[2]^255));
	log_ak09973("[latch_ak09973] %s  y=%d\n",__func__,p);
	return snprintf(buf,  PAGE_SIZE,"%d\n",p);
}

static ssize_t show_z(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8] = {0};
	unsigned char modedata[8] = {0};
	int err = 0;
	int sleeptime = 0;
	long int q,p,o = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x21, modedata);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x21 error");
	}

	switch(modedata[0])
	{
		case 66: sleeptime = 200;
				  break;
		case 68: sleeptime = 100;
				  break;
		case 70: sleeptime = 50;
				  break;
		case 72: sleeptime = 20;
				  break;
		case 74: sleeptime = 10;
				  break;
		case 76: sleeptime = 2;
				  break;
		case 78: sleeptime = 1;
				  break;
		default:
				  break;
	}

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x17, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
	}

	if(data[0] == 0)
	{
		msleep(sleeptime);
		mutex_lock(&latch_ak09973_dev->latch_mutex);
		err = i2c_read_bytes(client, 0x17, data);
		mutex_unlock(&latch_ak09973_dev->latch_mutex);
		if (err != 1){
			err_ak09973("[latch_ak09973] read stxyz error %d\n", err);
			return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x17 error");
		}
	}
	q = data[5] * 256 + data[6];
	p = data[3] * 256 + data[4];
	o = data[1] * 256 + data[2];

	if(data[5] >= 128)
		q = ~((data[5]^255)* 256 + (data[6]^255));

	if(data[3] >= 128)
		p = ~((data[3]^255)* 256 + (data[4]^255));

	if(data[1] >= 128)
		o = ~((data[1]^255)* 256 + (data[2]^255));
	log_ak09973("[latch_ak09973] %s  z=%d\n",__func__,o);
	return snprintf(buf,  PAGE_SIZE,"%d\n",o);
}

static ssize_t show_latch_ak09973_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char data[8] = {0};
	int err = 0;

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	err = i2c_read_bytes(client, 0x00, data);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	if (err != 1){
		err_ak09973("[latch_ak09973] read status error %d\n", err);
		return snprintf(buf,  PAGE_SIZE,"%s\n", "read 0x00 error");
	}
	log_ak09973("[latch_ak09973] %s  status=%d\n",__func__,data[0]);
	return snprintf(buf, PAGE_SIZE,"%x\n", data[0]);
}

static ssize_t store_latch_ak09973_status(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t show_latch_ak09973_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	char * state = 0;
	if(g_state==0){
		state="NONE";
	}else if(g_state==1){
		state="OPEN";
	}else if(g_state==2){
		state="CLOSE";
	}
	log_ak09973("[latch_ak09973] %s  g_state=%d\n",__func__,g_state);
	return snprintf(buf, PAGE_SIZE,"%s\n", state);
}

static DEVICE_ATTR(mode, 0664, show_action_mode, store_action_mode);
static DEVICE_ATTR(interrupt, 0664,show_latch_ak09973_interrupt_enable, store_latch_ak09973_interrupt_enable);
static DEVICE_ATTR(stxyz, 0664,show_st_xyz, NULL);
static DEVICE_ATTR(X, 0664,show_x, NULL);
static DEVICE_ATTR(Y, 0664,show_y, NULL);
static DEVICE_ATTR(Z, 0664,show_z, NULL);
static DEVICE_ATTR(X_threshold, 0664,show_X_threshold, store_X_threshold);
static DEVICE_ATTR(Y_threshold, 0664,show_Y_threshold, store_Y_threshold);
static DEVICE_ATTR(trigger_update, 0664,NULL, store_trigger_update);
static DEVICE_ATTR(status, 0664, show_latch_ak09973_status, store_latch_ak09973_status);
static DEVICE_ATTR(state, 0664, show_latch_ak09973_state, NULL);

static struct attribute *latch_ak09973_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_interrupt.attr,
	&dev_attr_stxyz.attr,
	&dev_attr_X.attr,
	&dev_attr_Y.attr,
	&dev_attr_Z.attr,
	&dev_attr_X_threshold.attr,
	&dev_attr_Y_threshold.attr,
	&dev_attr_trigger_update.attr,
	&dev_attr_status.attr,
	&dev_attr_state.attr,
	NULL
};

static struct attribute_group latch_ak09973_group = {
	.name = "latch_ak09973",
	.attrs = latch_ak09973_attrs
};

static void set_pinctrl(struct device *dev)
{
	int ret;
	struct pinctrl *key_pinctrl;
	struct pinctrl_state *set_state;

	key_pinctrl = devm_pinctrl_get(dev);
	set_state = pinctrl_lookup_state(key_pinctrl, "latch_gpio_high");
	ret = pinctrl_select_state(key_pinctrl, set_state);
	log_ak09973("%s: pinctrl_select_state = %d\n", __FUNCTION__, ret);
}

static int init_data(void)
{
	int ret = 0;
	
	// Memory allocation for data structure 
	latch_ak09973_dev = kzalloc(sizeof (struct latch_ak09973_str), GFP_KERNEL);
	if (!latch_ak09973_dev) {
		err_ak09973("Memory allocation fails for latch ak09973\n");
		ret = -ENOMEM;
		goto init_data_err;
	}
	latch_ak09973_dev->wake_src=wakeup_source_create("LatchAk09973_wake_lock");
	wakeup_source_add(latch_ak09973_dev->wake_src);
	//wakeup_source_init(&latch_ak09973_dev->wake_src, "LatchAk09973_wake_lock");
	return 0;
init_data_err:
	err_ak09973("Init Data ERROR\n");
	return ret;
}

static int init_irq (void)
{
	int ret = 0;

	/* GPIO to IRQ */
	ASUS_LATCH_AK09973_IRQ = gpio_to_irq(ASUS_LATCH_AK09973_GPIO);

	if (ASUS_LATCH_AK09973_IRQ < 0) {
		err_ak09973("[IRQ] gpio_to_irq ERROR, irq=%d.\n", ASUS_LATCH_AK09973_IRQ);
	}else {
		log_ak09973("[IRQ] gpio_to_irq IRQ %d successed on GPIO:%d\n", ASUS_LATCH_AK09973_IRQ, ASUS_LATCH_AK09973_GPIO);
	}

	ret = request_threaded_irq(ASUS_LATCH_AK09973_IRQ, NULL, latch_ak09973_reenable_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				INT_NAME, latch_ak09973_dev);

	if (ret < 0)
		err_ak09973("[IRQ] request_irq() ERROR %d.\n", ret);
	else {
		log_ak09973("[IRQ] Enable irq !! \n");
		enable_irq_wake(ASUS_LATCH_AK09973_IRQ);
	}

	return 0;
}

static int latch_ak09973_probe(struct i2c_client *client, const struct i2c_device_id * id)
{
	int ret = 0;

	log_ak09973("Probe +++\n");
	ret = init_data();
	if (ret < 0)
		goto probe_err;

	mutex_init(&latch_ak09973_dev->latch_mutex);

	/* GPIO */
	ASUS_LATCH_AK09973_GPIO = of_get_named_gpio(client->dev.of_node, "latch,int-gpio", 0);
    log_ak09973("[GPIO] GPIO =%d(%d)\n", ASUS_LATCH_AK09973_GPIO, gpio_get_value(ASUS_LATCH_AK09973_GPIO));
	gpio_free(ASUS_LATCH_AK09973_GPIO);
	/* GPIO Request */
	report_client = client;
	set_pinctrl(&client->dev);

	/* GPIO Direction */
	ret = gpio_direction_input(ASUS_LATCH_AK09973_GPIO);
	if (ret < 0) {
		err_ak09973("[GPIO] Unable to set the direction of gpio %d\n", ASUS_LATCH_AK09973_GPIO);
		goto probe_err;
	}

	latch_ak09973_dev->vdd_supply = regulator_get(&client->dev, "vdd");
	if (IS_ERR(latch_ak09973_dev->vdd_supply)) {
        err_ak09973("ret vdd regulator failed,ret=%d", ret);
        goto probe_err;
    }
	ret = regulator_enable(latch_ak09973_dev->vdd_supply);
	if (ret) {
		err_ak09973("enable vcc_i2c regulator failed,ret=%d", ret);
    }
	msleep(10);
	//check i2c function
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err_ak09973("[latch_ak09973] I2C function test error\n");
		goto probe_err;
	} else
		log_ak09973("[latch_ak09973] I2C function test pass\n");

	/* sysfs */
	ret = sysfs_create_group(&client->dev.kobj, &latch_ak09973_group);
	if (ret) {
		err_ak09973("Latch ak09973 sysfs_create_group ERROR.\n");
		goto probe_err;
	}

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	latch_write_bytes(client,0x30,0);
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	msleep(5);
	mutex_lock(&latch_ak09973_dev->latch_mutex);
	latch_write_bytes(client,0x21,0x44); //value is 0x28 ,Wide measurement range; low noise drive; mode4
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	msleep(5);

	mutex_lock(&latch_ak09973_dev->latch_mutex);
	latch_write_threshold(client,0x22,g_threshold1X+32,g_threshold1X-32); //0x22, write X threshold
	latch_write_threshold(client,0x23,g_threshold1Y+32,g_threshold1Y-32); //0x24, write Y threshold
	ret = latch_write_interrupt(client, 0x20, 0x06); //set SWX1EN(D1),SWX2EN(D2),SWY1EN(D3)
	if(ret < 0)
	{
		err_ak09973("[latch_ak09973]write interrupt [0x00] [0x06]  send error\n");
		mutex_unlock(&latch_ak09973_dev->latch_mutex);
		goto probe_err;
	}
	mutex_unlock(&latch_ak09973_dev->latch_mutex);
	msleep(5);

	/* Work Queue init */
    latch_ak09973_wq = create_singlethread_workqueue("latch_ak09973_wq");
    INIT_DEFERRABLE_WORK(&latch_ak09973_dev->latch_ak09973_work, debounce_latch_ak09973_report_function);
    ret = init_irq();
    if (ret < 0)
         goto probe_err;
    queue_delayed_work(latch_ak09973_wq, &latch_ak09973_dev->latch_ak09973_work, 0); //add thhis work to get the reboot status

    log_ak09973("Probe ---\n");
	return 0;

probe_err:
	err_ak09973("Probe ERROR\n");
	return ret;
}

/*****************************************************************************
* I2C Driver
*****************************************************************************/
static const struct i2c_device_id latch_id_table[] = {
    {DRIVER_NAME, 1},
    {},
};
MODULE_DEVICE_TABLE(i2c, latch_id_table);

static struct of_device_id latchak09973_match_table[] = {
	{ .compatible = "qcom,latch-ak09973",},
	{},
};

static struct dev_pm_ops latch_pm = {
    .suspend = latch_ak09973_suspend,
    .resume  = latch_ak09973_resume,    
};

static struct i2c_driver latch_ak09973_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = latchak09973_match_table,
		.pm  = &latch_pm,
	},
	.probe          = latch_ak09973_probe,
	.id_table	= latch_id_table,
};

static int __init latch_ak09973_init(void)
{
	int err = 0;
	log_ak09973("Driver latch_ak09973 +++\n");
	err = i2c_add_driver(&latch_ak09973_driver);
	if (err != 0) {
		err_ak09973("[Latch Ak09973] i2c_driver_register fail, Error : %d\n", err);
		return err;
    }
	log_ak09973("Driver latch_ak09973 ---\n");
	return err;
}

static void __exit latch_ak09973_exit(void)
{
	int err = 0;
	log_ak09973("Driver EXIT +++\n");
	err = regulator_disable(latch_ak09973_dev->vdd_supply);
    if (err)
	{
    	err_ak09973("disable ibb regulator failed,ret=%d\n", err);
    }
    mutex_destroy(&latch_ak09973_dev->latch_mutex);
	free_irq(ASUS_LATCH_AK09973_IRQ, latch_ak09973_dev);
	wakeup_source_remove(latch_ak09973_dev->wake_src);
	wakeup_source_destroy(latch_ak09973_dev->wake_src);
	//wakeup_source_trash(&latch_ak09973_dev->wake_src);
	unregister_chrdev(major,DRIVER_NAME);
	i2c_del_driver(&latch_ak09973_driver);
	latch_ak09973_dev=NULL;
	kfree(latch_ak09973_dev);
	gpio_free(ASUS_LATCH_AK09973_GPIO);
	log_ak09973("Driver EXIT ---\n");
}

module_init(latch_ak09973_init);
module_exit(latch_ak09973_exit);

MODULE_DESCRIPTION("Latch Ak09973");
MODULE_LICENSE("GPL v2");
