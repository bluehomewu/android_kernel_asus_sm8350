#include "focaltech_core.h"
#include "asus_tp.h"

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
u8 Rcoefleft = 0x0A, RcoefRight=0x0A;
int pre_angle = 0;
/*****************************************************************************
* 1.Static function prototypes
*******************************************************************************/
void set_rotation_mode()
{
    struct fts_ts_data *ts_data = fts_data;
    int ret;
    u8 mode = 0;
    
// rotation to 0    
    if (ts_data->rotation_angle == ANGLE_0) {
      FTS_INFO("Rotation angle 0");
      ret = fts_write_reg(FTS_REG_EDGEPALM_MODE_EN, 0x00);
      msleep(5);
      fts_read_reg(FTS_REG_EDGEPALM_MODE_EN, &mode);
      if (mode!=0x00) {
	FTS_INFO("Set to angle 0 fail reg read 0x%x",mode);
      }
    }
// rotation to 90
    if (ts_data->rotation_angle == ANGLE_90) {
      FTS_INFO("Rotation angle 90");
      ret = fts_write_reg(FTS_REG_EDGEPALM_MODE_EN, 0x01);
      msleep(5);
      fts_read_reg(FTS_REG_EDGEPALM_MODE_EN, &mode);
      if (mode!=0x01) {
	FTS_INFO("Set to angle 90 fail reg read 0x%x",mode);
      }
    }

// rotation to 270
    if (ts_data->rotation_angle == ANGLE_270) {
      FTS_INFO("Rotation angle 270");
      ret = fts_write_reg(FTS_REG_EDGEPALM_MODE_EN, 0x02);
      msleep(5);
      fts_read_reg(FTS_REG_EDGEPALM_MODE_EN, &mode);
      if (mode!=0x02) {
	FTS_INFO("Set to angle 270 fail reg read 0x%x",mode);
      }
    }    
}

void disable_edge_palm(void) {
    int ret;
    u8 mode = 0;
    
    ret = fts_write_reg(FTS_REG_EDGEPALM_MODE_EN, 0x00);
    msleep(5);
    fts_read_reg(FTS_REG_EDGEPALM_MODE_EN, &mode);
    if (mode!=0x00) {
	FTS_INFO("Set to angle 0 fail reg read 0x%x",mode);
    } else
        FTS_INFO("Edge palm disabled");
}

void set_edge_palm(void) {
    struct fts_ts_data *ts_data = fts_data;
    u8 l_val = 0 , r_val = 0;
    int retl = 0, retr = 0;
    u8 r_set = 0, l_set = 0;

    if (fts_data->edge_palm_enable == 0) { // game genie set edge palm disable
	mutex_lock(&fts_data->reg_lock);
	retl = fts_write_reg(FTS_REG_EDGEPALM_LEFT, 0x0);
	l_set = 0x0;
	msleep(5);
	retr = fts_write_reg(FTS_REG_EDGEPALM_RIGHT,0x0);
	r_set = 0x0;
	mutex_unlock(&fts_data->reg_lock);
    }
    
    if (fts_data->edge_palm_enable == 1) { // game genie set edge palm enable
        mutex_lock(&fts_data->reg_lock);
      	if (ts_data->rotation_angle == ANGLE_0)  {
	    retl = fts_write_reg(FTS_REG_EDGEPALM_LEFT, 0x0);
	    l_set = 0x0;
	    msleep(5);
	    retr = fts_write_reg(FTS_REG_EDGEPALM_RIGHT,0x0);
	    r_set = 0x0;
	}

	if ((ts_data->rotation_angle == ANGLE_90) || (ts_data->rotation_angle == ANGLE_270))  {
	    retl = fts_write_reg(FTS_REG_EDGEPALM_LEFT, Rcoefleft);
	    l_set = Rcoefleft;
	    msleep(5);
	    retr = fts_write_reg(FTS_REG_EDGEPALM_RIGHT, RcoefRight);
	    r_set = RcoefRight;
	}
	mutex_unlock(&fts_data->reg_lock);
    }    
      
    if (fts_data->edge_palm_enable == 2) { // not in game mode or never set
        mutex_lock(&fts_data->reg_lock);
      	if (ts_data->rotation_angle == ANGLE_0)  {
	    retl = fts_write_reg(FTS_REG_EDGEPALM_LEFT, 0x0);
	    l_set = 0x0;
	    msleep(5);   
	    retr = fts_write_reg(FTS_REG_EDGEPALM_RIGHT,0x0);
	    r_set = 0x0;
	}

	if ((ts_data->rotation_angle == ANGLE_90) || (ts_data->rotation_angle == ANGLE_270))  {
	    retl = fts_write_reg(FTS_REG_EDGEPALM_LEFT, 0x0A);
	    l_set = 0x0A;
	    msleep(5);  
	    retr = fts_write_reg(FTS_REG_EDGEPALM_RIGHT, 0x0A);
	    r_set = 0x0A;
	}
	mutex_unlock(&fts_data->reg_lock);
    }
    
    mutex_lock(&fts_data->reg_lock);    
    fts_read_reg(FTS_REG_EDGEPALM_LEFT, &l_val);
    fts_read_reg(FTS_REG_EDGEPALM_RIGHT, &r_val);
    mutex_unlock(&fts_data->reg_lock);
    
    if ((l_set != l_val) || (r_set!= r_val)) {
        FTS_INFO("Set edge palm error set (L:%x, R:%x), read (L:%x, R:%x)",l_set,r_set,l_val,r_val);
    } else
	FTS_INFO("Set rotation reg to %d , palm range left %x right %x",ts_data->rotation_angle,l_val,r_val);
}

void set_report_rate () {
    struct fts_ts_data *ts_data = fts_data;
    int ret = 0 , i = 0;
    u8 rate = 0;
    
    if (fts_data->report_rate == REPORT_RATE_0) {
	mutex_lock(&fts_data->reg_lock);      
	for (i = 0; i < 6; i++) {
	    ret = fts_write_reg(FTS_REG_REPORT_RATE, 0x00);
	    msleep(20);
	    ret = fts_read_reg(FTS_REG_REPORT_RATE, &rate);
	    if (rate!=0x00){
	      FTS_DEBUG("set report rate to 120Hz fail rate 0x%X , retry %d",rate, i);
	      msleep(20);
	    } else {
      	      ts_data->report_rate = REPORT_RATE_0;
	      FTS_DEBUG("set report rate to 120Hz rate %X",rate);
	      break;
	    }
	}
	mutex_unlock(&fts_data->reg_lock);
	return;
    }
    
    if (fts_data->report_rate == REPORT_RATE_1) {
	mutex_lock(&fts_data->reg_lock);      
	for (i = 0; i < 6; i++) {
	    ret = fts_write_reg(FTS_REG_REPORT_RATE, 0x24);
	    msleep(20);
	    ret = fts_read_reg(FTS_REG_REPORT_RATE, &rate);
	    if (rate!=0x24){
	      FTS_DEBUG("set report rate to 300Hz fail rate 0x%X , retry %d",rate, i);
	      msleep(20);
	    } else {
      	      ts_data->report_rate = REPORT_RATE_1;
	      FTS_DEBUG("set report rate to 300Hz rate %X",rate);
	      break;
	    }
	}
	mutex_unlock(&fts_data->reg_lock);
	return;
    } 
}

void set_sub_noise_mode(bool enable) {
    int ret = 0 , i = 0;
    u8 mode = 0;
    
    if (enable) {
        mutex_lock(&fts_data->reg_lock);      
        for (i = 0; i < 6; i++) {
            ret = fts_write_reg(FTS_REG_SUBNOISE_MODE, 0x01);
            msleep(20);
            ret = fts_read_reg(FTS_REG_SUBNOISE_MODE, &mode);
            if (mode!=0x01){
                FTS_DEBUG("enter sub noise mode fail, mode 0x%X , retry %d",mode, i);
                msleep(20);
            } else {
                fts_data->sub_noise = ENABLE;
                FTS_DEBUG("enter sub noise mode %X",mode);
                break;
            }
        }
        mutex_unlock(&fts_data->reg_lock);
        return;
    } else {
        mutex_lock(&fts_data->reg_lock);      
        for (i = 0; i < 6; i++) {
            ret = fts_write_reg(FTS_REG_SUBNOISE_MODE, 0x00);
            msleep(20);
            ret = fts_read_reg(FTS_REG_SUBNOISE_MODE, &mode);
            if (mode!=0x00){
                FTS_DEBUG("exit sub noise mode fail, mode 0x%X , retry %d",mode, i);
                msleep(20);
            } else {
                fts_data->sub_noise = DISABLE;
                FTS_DEBUG("exit sub noise mode %X",mode);
                break;
            }
        }
        mutex_unlock(&fts_data->reg_lock);
        return;
    }
}


static ssize_t fts_rotation_mode_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d \n", fts_data->rotation_angle);
}

static ssize_t fts_rotation_mode_store(
    struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        bool reconfig = false;
	if (buf[0] == '1') {
	  if (pre_angle != 1)
	      reconfig = true;  
	  fts_data->rotation_angle = ANGLE_90;
	  pre_angle = 1;
	} else if (buf[0] == '2') {
  	  if (pre_angle != 2)
	      reconfig = true;  
	  fts_data->rotation_angle = ANGLE_270;
	  pre_angle = 2;
	} else if (buf[0] == '0') {
	  if (pre_angle != 0)
	      reconfig = true; 
	  fts_data->rotation_angle = ANGLE_0;
	  pre_angle = 0;
	}
	
	if (reconfig) {
	    set_rotation_mode();
	    if ((fts_data->edge_palm_enable==1) || (fts_data->edge_palm_enable==2)) { // rotation when game playing 
		set_edge_palm();
	    }
	    if (fts_data->edge_palm_enable == 0){
	        disable_edge_palm();
	    }
	    
	    if ((fts_data->rotation_angle == ANGLE_0) && (fts_data->edge_palm_enable==2)) { // exit edge palm
	       disable_edge_palm();
	    }
	}
	
	return count;
}

static ssize_t rise_report_rate_show (struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 rate = 0;
    int report_rate = 0;
    fts_read_reg(FTS_REG_REPORT_RATE, &rate);
    
    if (rate == 0x24)
        report_rate = 300;
    else
        report_rate = 120;
    count = snprintf(buf + count, PAGE_SIZE, "%d\n",
                     report_rate);

    return count;
}

static ssize_t rise_report_rate_store(
    struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{  
        bool reconfig = false;
	if (buf[0] == '0') {
	  if (fts_data->pre_report_rate != 0)
	      reconfig = true;
	  fts_data->report_rate = REPORT_RATE_0; //120Hz
	  fts_data->pre_report_rate = 0;
	  fts_data->power_saving_mode = true;
	}
	
	if (buf[0] == '1') {
  	  if (fts_data->pre_report_rate != 1)
	      reconfig = true;  
	  fts_data->report_rate = REPORT_RATE_1; //300Hz
	  fts_data->power_saving_mode = false;
	  fts_data->pre_report_rate =1;
	}
       
    if (reconfig)  
        set_report_rate();

    FTS_DEBUG("Report rate set to:%d", fts_data->report_rate);
    return count;
}

static ssize_t fts_extra_config_store(
    struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct fts_ts_data *ts_data = fts_data;

    ts_data->extra_reconfig = buf[0]-'0';

    FTS_DEBUG("Extra touch extra config:%d", ts_data->extra_reconfig);

    if (ts_data->extra_reconfig == 1) {
        FTS_DEBUG("Reconfig touch size");
    }
    
    if (ts_data->extra_reconfig == 2) {
        if (!ts_data->sub_noise) {
            FTS_DEBUG("Reconfig touch frequency");
            set_sub_noise_mode(true);
        }
    }
    
    if (ts_data->extra_reconfig == 0) {
        FTS_DEBUG("Exit Extra mode");
        if (ts_data->sub_noise)
            set_sub_noise_mode(false);

    }
    return count;
}

static DEVICE_ATTR(fts_rotation_mode, S_IRUGO | S_IWUSR, fts_rotation_mode_show, fts_rotation_mode_store);
static DEVICE_ATTR(rise_report_rate, S_IRUGO | S_IWUSR, rise_report_rate_show, rise_report_rate_store);
static DEVICE_ATTR(fts_extra_config, S_IRUGO | S_IWUSR, NULL, fts_extra_config_store);
/* add your attr in here*/
static struct attribute *fts_attributes[] = {
    &dev_attr_fts_rotation_mode.attr,
    &dev_attr_rise_report_rate.attr,
    &dev_attr_fts_extra_config.attr,
    NULL
};

static struct attribute_group asus_game_attribute_group = {
    .attrs = fts_attributes
};

void report_rate_recovery(struct fts_ts_data *ts_data) 
{
    FTS_INFO("reconfig with report rate %d",fts_data->report_rate);
    set_report_rate();    
}

int asus_game_create_sysfs(struct fts_ts_data *ts_data)
{
    int ret = 0;

    ret = sysfs_create_group(&ts_data->dev->kobj, &asus_game_attribute_group);
    if (ret) {
        FTS_ERROR("[EX]: asus_create_group() failed!!");
        sysfs_remove_group(&ts_data->dev->kobj, &asus_game_attribute_group);
        return -ENOMEM;
    } else {
        FTS_INFO("[EX]: asus_create_group() succeeded!!");
    }
    
    ts_data->game_mode = DISABLE;
    ts_data->rotation_angle = 0;
    ts_data->report_rate = REPORT_RATE_1;
    fts_data->edge_palm_enable = 2;
    fts_data->pre_report_rate = 1;   
    fts_data->sub_noise = DISABLE;
    
    mutex_init(&ts_data->reg_lock);
    return ret;
}

int asus_game_remove_sysfs(struct fts_ts_data *ts_data)
{
    sysfs_remove_group(&ts_data->dev->kobj, &asus_game_attribute_group);
    return 0;
}
