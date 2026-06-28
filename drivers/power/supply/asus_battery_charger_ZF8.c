/*
 * Copyright (c) 2019-2020, The ASUS Company. All rights reserved.
 */

#define pr_fmt(fmt) "BATTERY_CHG: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>

#include <linux/of_gpio.h>
//ASUS BSP +++
#include "fg-core.h"
#include "asus_battery_charger_ZF8.h"
//ASUS BSP ---

#include "../../thermal/qcom/adc-tm.h"
#include <dt-bindings/iio/qcom,spmi-vadc.h>
#include <dt-bindings/iio/qcom,spmi-adc7-pm8350.h>
#include <dt-bindings/iio/qcom,spmi-adc7-pm8350b.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/asusdebug.h>

bool g_once_usb_thermal = false;
bool g_asuslib_init = false;

struct iio_channel *usb_conn_temp_vadc_chan;

//[+++] Add the external function
extern int battery_chg_write(struct battery_chg_dev *bcdev, void *data, int len);
//[---] Add the external function

enum {
    QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
    QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
    QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
};

static const char * const power_supply_usb_type_text[] = {
    "Unknown", "USB", "DCP", "CDP", "ACA", "C",
    "PD", "PD_DRP", "PD_PPS", "BrickID"
};
/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
    "HVDCP", "HVDCP_3", "HVDCP_3P5"
};

//ASUS_BSP +++ add to printk the WIFI hotspot & QXDM UTS event
bool g_qxdm_en = false;
bool g_wifi_hs_en = false;
//ASUS_BSP --- add to printk the WIFI hotspot & QXDM UTS event

//ASUS_BSP +++ LiJen add to printk the WIFI hotspot & QXDM UTS event
#include <linux/proc_fs.h>
#define uts_status_PROC_FILE	"driver/UTSstatus"
static struct proc_dir_entry *uts_status_proc_file;
static int uts_status_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "WIFIHS:%d, QXDM:%d\n", g_wifi_hs_en, g_qxdm_en);
	return 0;
}

static ssize_t uts_status_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	switch (val) {
	case 0:
		printk("%s: WIFI Hotspot disable\n");
		g_wifi_hs_en = false;
		break;
	case 1:
		printk("%s: WIFI Hotspot enable\n");
		g_wifi_hs_en = true;
		break;
	case 2:
		printk("%s: QXDM disable\n");
		g_qxdm_en = false;
		break; 
	case 3:
		printk("%s: QXDM enable\n");
		g_qxdm_en = true;
		break;       
	default:
		printk("%s: Invalid mode\n");
		break;
	}
    
	return len;
}

static int uts_status_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, uts_status_proc_read, NULL);
}

static const struct file_operations uts_status_fops = {
	.owner = THIS_MODULE,
    .open = uts_status_proc_open,
    .read = seq_read,
	.write = uts_status_proc_write,
    .release = single_release,
};

void static create_uts_status_proc_file(void)
{
	uts_status_proc_file = proc_create(uts_status_PROC_FILE, 0666, NULL, &uts_status_fops);

    if (uts_status_proc_file) {
		printk("create_uts_status_proc_file sucessed!\n");
    } else {
	    printk("create_uts_status_proc_file failed!\n");
    }
}
//ASUS_BSP --- LiJen add to printk the WIFI hotspot & QXDM UTS event

static const char *get_usb_type_name(u32 usb_type)
{
    u32 i;

    if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
        usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5) {
        for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
             i++) {
            if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
                return qc_power_supply_usb_type_text[i];
        }
        return "Unknown";
    }

    for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
        if (i == usb_type)
            return power_supply_usb_type_text[i];
    }

    return "Unknown";
}

static int read_property_id(struct battery_chg_dev *bcdev,
            struct psy_state *pst, u32 prop_id)
{
    struct battery_charger_req_msg req_msg = { { 0 } };

    req_msg.property_id = prop_id;
    req_msg.battery_id = 0;
    req_msg.value = 0;
    req_msg.hdr.owner = MSG_OWNER_BC;
    req_msg.hdr.type = MSG_TYPE_REQ_RESP;
    req_msg.hdr.opcode = pst->opcode_get;

    pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name,
        req_msg.property_id);

    return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static ssize_t oem_prop_read(enum battman_oem_property prop, size_t count)
{
    struct battman_oem_read_buffer_req_msg req_msg = { { 0 } };
    int rc;

    req_msg.hdr.owner = PMIC_GLINK_MSG_OWNER_OEM;
    req_msg.hdr.type = MSG_TYPE_REQ_RESP;
    req_msg.hdr.opcode = OEM_OPCODE_READ_BUFFER;
    req_msg.oem_property_id = prop;
    req_msg.data_size = count;

    rc = battery_chg_write(g_bcdev, &req_msg, sizeof(req_msg));
    if (rc < 0) {
        pr_err("Failed to read buffer rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t oem_prop_write(enum battman_oem_property prop,
                    u32 *buf, size_t count)
{
    struct battman_oem_write_buffer_req_msg req_msg = { { 0 } };
    int rc;

    req_msg.hdr.owner = PMIC_GLINK_MSG_OWNER_OEM;
    req_msg.hdr.type = MSG_TYPE_REQ_RESP;
    req_msg.hdr.opcode = OEM_OPCODE_WRITE_BUFFER;
    req_msg.oem_property_id = prop;
    memcpy(req_msg.data_buffer, buf, sizeof(u32)*count);
    req_msg.data_size = count;

    if (g_bcdev == NULL) {
        pr_err("g_bcdev is null\n");
        return -1;
    }
    rc = battery_chg_write(g_bcdev, &req_msg, sizeof(req_msg));
    if (rc < 0) {
        pr_err("Failed to write buffer rc=%d\n", rc);
        return rc;
    }

    return count;
}

int asus_get_Batt_ID(void)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_BATT_ID, 1);
    if (rc < 0) {
        pr_err("Failed to get BattID rc=%d\n", rc);
        return rc;
    }
    return 0;
}

//[+++] Addd the interface for accessing the BATTERY power supply
static ssize_t asus_get_FG_SoC_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    union power_supply_propval prop = {};
    int bat_cap, rc = 0;

    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_CAPACITY, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery SOC, rc=%d\n", rc);
        return rc;
    }
    bat_cap = prop.intval;
    printk(KERN_ERR "%s. BAT_SOC : %d", __func__, bat_cap);

    return scnprintf(buf, PAGE_SIZE, "%d\n", bat_cap);
}
static CLASS_ATTR_RO(asus_get_FG_SoC);
//[---] Addd the interface for accessing the BATTERY power supply

//[+++] Add the interface for accesing the inforamtion of ChargerPD on ADSP
static ssize_t asus_get_PlatformID_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_ADSP_PLATFORM_ID, 1);
    if (rc < 0) {
        pr_err("Failed to get PlatformID rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.PlatformID);
}
static CLASS_ATTR_RO(asus_get_PlatformID);

static ssize_t asus_get_BattID_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = asus_get_Batt_ID();
    if (rc < 0) {
        pr_err("Failed to get BattID rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.BATT_ID);
}
static CLASS_ATTR_RO(asus_get_BattID);

static ssize_t get_usb_type_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    //struct power_supply *psy;
    struct psy_state *pst;
    int rc = 0 , val = 0;

    rc = oem_prop_read(BATTMAN_OEM_AdapterVID, 1);
    if (rc < 0) {
        pr_err("Failed to get CHG_LIMIT_EN rc=%d\n", rc);
        return rc;
    }

    if(ChgPD_Info.AdapterVID == 0xB05){
        return scnprintf(buf, PAGE_SIZE, "PD_ASUS_PPS_30W_2A\n");
    }

    if (g_bcdev == NULL)
        return -1;
    CHG_DBG("%s\n", __func__);
    pst = &g_bcdev->psy_list[PSY_TYPE_USB];
    rc = read_property_id(g_bcdev, pst, 11);//11:USB_REAL_TYPE
    if (!rc) {
        val = pst->prop[11];//11:USB_REAL_TYPE
        CHG_DBG("%s. val : %d\n", __func__, val);
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(get_usb_type);

static ssize_t charger_limit_en_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;

    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. enable : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_CHG_LIMIT_EN, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set CHG_LIMIT_EN rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t charger_limit_en_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_CHG_LIMIT_EN, 1);
    if (rc < 0) {
        pr_err("Failed to get CHG_LIMIT_EN rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.chg_limit_en);
}
static CLASS_ATTR_RW(charger_limit_en);

static ssize_t charger_limit_cap_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;

    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. cap : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_CHG_LIMIT_CAP, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set CHG_LIMIT_CAP rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t charger_limit_cap_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_CHG_LIMIT_CAP, 1);
    if (rc < 0) {
        pr_err("Failed to get CHG_LIMIT_CAP rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.chg_limit_cap);
}
static CLASS_ATTR_RW(charger_limit_cap);

static ssize_t usbin_suspend_en_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    rc = oem_prop_write(BATTMAN_OEM_USBIN_SUSPEND, &tmp, 1);

    pr_err("%s. enable : %d", __func__, tmp);
    if (rc < 0) {
        pr_err("Failed to set USBIN_SUSPEND_EN rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t usbin_suspend_en_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_USBIN_SUSPEND, 1);
    if (rc < 0) {
        pr_err("Failed to get USBIN_SUSPEND_EN rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.usbin_suspend_en);
}
static CLASS_ATTR_RW(usbin_suspend_en);

static ssize_t charging_suspend_en_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. enable : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_CHARGING_SUSPNED, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set CHARGING_SUSPEND_EN rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t charging_suspend_en_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_CHARGING_SUSPNED, 1);
    if (rc < 0) {
        pr_err("Failed to get CHARGING_SUSPEND_EN rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.charging_suspend_en);
}
static CLASS_ATTR_RW(charging_suspend_en);

static ssize_t get_ChgPD_FW_Ver_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_CHGPD_FW_VER, 32);
    if (rc < 0) {
        pr_err("%s. Failed to get ChgPD_FW_Ver rc=%d\n", __func__, rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", ChgPD_Info.ChgPD_FW);
}
static CLASS_ATTR_RO(get_ChgPD_FW_Ver);

static ssize_t asus_get_fw_version_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_FW_VERSION, 1);
    if (rc < 0) {
        pr_err("Failed to get FW_version rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "0x00%x\n", ChgPD_Info.firmware_version);
}
static CLASS_ATTR_RO(asus_get_fw_version);

static ssize_t asus_get_batt_temp_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_BATT_TEMP, 1);
    if (rc < 0) {
        pr_err("Failed to get Batt_temp rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.batt_temp);
}
static CLASS_ATTR_RO(asus_get_batt_temp);

static ssize_t enter_ship_mode_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    struct battery_charger_ship_mode_req_msg msg = { { 0 } };

    int rc, tmp;
    bool ship_en;
    tmp = simple_strtol(buf, NULL, 10);
    ship_en = tmp;
    if (ship_en == 0) {
        CHG_DBG("%s. NO action for SHIP mode\n", __func__);
        return count;
    }
    msg.hdr.owner = MSG_OWNER_BC;
    msg.hdr.type = MSG_TYPE_REQ_RESP;
    msg.hdr.opcode = 0x36;// = BC_SHIP_MODE_REQ_SET
    msg.ship_mode_type = 0;// = SHIP_MODE_PMIC

    rc = battery_chg_write(g_bcdev, &msg, sizeof(msg));
    if (rc < 0)
        pr_err("%s. Failed to write SHIP mode: %d\n", rc);
    CHG_DBG("%s. Set SHIP Mode OK\n", __func__);

    return count;
}

static ssize_t enter_ship_mode_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "No function\n");
}
static CLASS_ATTR_RW(enter_ship_mode);

static ssize_t once_usb_thermal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	if (g_once_usb_thermal)
		return sprintf(buf, "FAIL\n");
	else
		return sprintf(buf, "PASS\n");
}
static CLASS_ATTR_RO(once_usb_thermal);

static ssize_t pm8350b_icl_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. set BATTMAN_OEM_PM8350B_ICL : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_PM8350B_ICL, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set BATTMAN_OEM_PM8350B_ICL rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t pm8350b_icl_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_PM8350B_ICL, 1);
    if (rc < 0) {
        pr_err("Failed to get BATTMAN_OEM_PM8350B_ICL rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.pm8350b_icl);
}
static CLASS_ATTR_RW(pm8350b_icl);

static ssize_t smb1396_icl_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. set BATTMAN_OEM_SMB1396_ICL : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_SMB1396_ICL, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set BATTMAN_OEM_SMB1396_ICL rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t smb1396_icl_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_SMB1396_ICL, 1);
    if (rc < 0) {
        pr_err("Failed to get BATTMAN_OEM_SMB1396_ICL rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.smb1396_icl);
}
static CLASS_ATTR_RW(smb1396_icl);

static ssize_t batt_FCC_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. set BATTMAN_OEM_FCC : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_FCC, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set BATTMAN_OEM_FCC rc=%d\n", rc);
        return rc;
    }

    return count;
}

static ssize_t batt_FCC_show(struct class *c,
                    struct class_attribute *attr, char *buf)
{
    int rc;

    rc = oem_prop_read(BATTMAN_OEM_FCC, 1);
    if (rc < 0) {
        pr_err("Failed to get BATTMAN_OEM_FCC rc=%d\n", rc);
        return rc;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", ChgPD_Info.batt_fcc);
}
static CLASS_ATTR_RW(batt_FCC);

static ssize_t set_debugmask_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = (u32) simple_strtol(buf, NULL, 16);

    CHG_DBG("%s. set BATTMAN_OEM_DEBUG_MASK : 0x%x", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_DEBUG_MASK, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set BATTMAN_OEM_DEBUG_MASK rc=%d\n", rc);
        return rc;
    }

    return count;
}
static CLASS_ATTR_WO(set_debugmask);

static ssize_t smb_setting_store(struct class *c,
                    struct class_attribute *attr,
                    const char *buf, size_t count)
{
    int rc;
    u32 tmp;
    tmp = simple_strtol(buf, NULL, 10);

    CHG_DBG("%s. set BATTMAN_OEM_SMB_Setting : %d", __func__, tmp);
    rc = oem_prop_write(BATTMAN_OEM_SMB_Setting, &tmp, 1);
    if (rc < 0) {
        pr_err("Failed to set BATTMAN_OEM_SMB_Setting rc=%d\n", rc);
        return rc;
    }

    return count;
}
static CLASS_ATTR_WO(smb_setting);

static struct attribute *asuslib_class_attrs[] = {
    &class_attr_asus_get_FG_SoC.attr,
    &class_attr_asus_get_PlatformID.attr,
    &class_attr_asus_get_BattID.attr,
    &class_attr_get_usb_type.attr,
    &class_attr_charger_limit_en.attr,
    &class_attr_charger_limit_cap.attr,
    &class_attr_usbin_suspend_en.attr,
    &class_attr_charging_suspend_en.attr,
    &class_attr_get_ChgPD_FW_Ver.attr,
    &class_attr_asus_get_fw_version.attr,
    &class_attr_asus_get_batt_temp.attr,
    &class_attr_enter_ship_mode.attr,
    &class_attr_once_usb_thermal.attr,
    &class_attr_pm8350b_icl.attr,
    &class_attr_smb1396_icl.attr,
    &class_attr_batt_FCC.attr,
    &class_attr_set_debugmask.attr,
    &class_attr_smb_setting.attr,
    NULL,
};
ATTRIBUTE_GROUPS(asuslib_class);

struct class asuslib_class = {
    .name = "asuslib",
    .class_groups = asuslib_class_groups,
};

int g_temp_THR = 70000;

void asus_usb_thermal_work(struct work_struct *work)
{
	int rc;
	int adc_temp;

	rc = iio_read_channel_processed(usb_conn_temp_vadc_chan, &adc_temp);
	if (rc < 0)
		CHG_DBG_E("%s: iio_read_channel_processed fail\n", __func__);
	else
		CHG_DBG_E("%s: usb_conn_temp = %d\n", __func__, adc_temp);

	if (adc_temp > g_temp_THR) {
		g_once_usb_thermal = 1;
	}

	schedule_delayed_work(&g_bcdev->asus_usb_thermal_work, msecs_to_jiffies(60000));
}

int asus_init_power_supply_prop(void) {

    // Initialize the power supply for usb properties
    if (!qti_phy_usb)
        qti_phy_usb = power_supply_get_by_name("usb");

    if (!qti_phy_usb) {
        pr_err("Failed to get usb power supply, rc=%d\n");
        return -ENODEV;
    }

    // Initialize the power supply for battery properties
    if (!qti_phy_bat)
        qti_phy_bat = power_supply_get_by_name("battery");

    if (!qti_phy_bat) {
        pr_err("Failed to get battery power supply, rc=%d\n");
        return -ENODEV;
    }
    return 0;
};

static void handle_notification(struct battery_chg_dev *bcdev, void *data,
                size_t len)
{
    struct evtlog_context_resp_msg3 *evtlog_msg;
    struct oem_set_OTG_WA_resp *OTG_WA_msg;
    struct oem_set_Charger_Type_resp *Update_charger_type_msg;
    struct pmic_glink_hdr *hdr = data;
    int rc;
    static int pre_chg_type = 0;

    switch(hdr->opcode) {
    case OEM_ASUS_EVTLOG_IND:
        if (len == sizeof(*evtlog_msg)) {
            evtlog_msg = data;
            pr_err("[adsp] evtlog= %s\n", evtlog_msg->buf);
        }
        break;
    case OEM_PD_EVTLOG_IND:
        if (len == sizeof(*evtlog_msg)) {
            evtlog_msg = data;
            pr_err("[PD] %s\n", evtlog_msg->buf);
        }
        break;
    case OEM_SET_OTG_WA:
        if (len == sizeof(*OTG_WA_msg)) {
            OTG_WA_msg = data;
            CHG_DBG("%s OEM_SET_OTG_WA. enable : %d, HWID : %d\n", __func__, OTG_WA_msg->enable, g_ASUS_hwID);
            if(g_ASUS_hwID <= HW_REV_EVB2) {
                if (gpio_is_valid(POGO_OTG_GPIO)) {
                    rc = gpio_direction_output(POGO_OTG_GPIO, OTG_WA_msg->enable);
                    if (rc)
                        pr_err("%s. Failed to control POGO_OTG_EN\n", __func__);
                } else {
                    CHG_DBG_E("%s. POGO_OTG_GPIO is invalid\n", __func__);
                }
            }

            if(g_ASUS_hwID >= HW_REV_ER) {
                if (gpio_is_valid(OTG_LOAD_SWITCH_GPIO)) {
                    rc = gpio_direction_output(OTG_LOAD_SWITCH_GPIO, OTG_WA_msg->enable);
                    if (rc)
                        pr_err("%s. Failed to control OTG_Load_Switch\n", __func__);
                } else {
                    CHG_DBG_E("%s. OTG_LOAD_SWITCH_GPIO is invalid\n", __func__);
                }
            }
        } else {
            pr_err("Incorrect response length %zu for OEM_SET_OTG_WA\n",
                len);
        }
        break;
    case OEM_SET_CHARGER_TYPE_CHANGE:
        if (len == sizeof(*Update_charger_type_msg)) {
            Update_charger_type_msg = data;
            CHG_DBG("%s OEM_SET_CHARGER_TYPE_CHANGE. new type : %d, old type : %d\n", __func__, Update_charger_type_msg->charger_type, pre_chg_type);
            if (Update_charger_type_msg->charger_type != pre_chg_type) {
                switch (Update_charger_type_msg->charger_type) {
                case ASUS_CHARGER_TYPE_LEVEL0:
                    g_SWITCH_LEVEL = SWITCH_LEVEL0_DEFAULT;
                break;
                case ASUS_CHARGER_TYPE_LEVEL1:
                    g_SWITCH_LEVEL = SWITCH_LEVEL2_QUICK_CHARGING;
                break;
                case ASUS_CHARGER_TYPE_LEVEL2:
                    g_SWITCH_LEVEL = SWITCH_LEVEL3_QUICK_CHARGING;
                break;
                case ASUS_CHARGER_TYPE_LEVEL3:
                    g_SWITCH_LEVEL = SWITCH_LEVEL4_QUICK_CHARGING;
                break;
                default:
                    g_SWITCH_LEVEL = SWITCH_LEVEL0_DEFAULT;
                break;
                }
                if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) && qti_phy_bat)
                    power_supply_changed(qti_phy_bat);
            }
            pre_chg_type = Update_charger_type_msg->charger_type;
        } else {
            pr_err("Incorrect response length %zu for OEM_SET_CHARGER_TYPE_CHANGE\n",
                len);
        }
        break;
    default:
        pr_err("Unknown opcode: %u\n", hdr->opcode);
        break;
    }

}

static void handle_message(struct battery_chg_dev *bcdev, void *data,
                size_t len)
{
    struct battman_oem_read_buffer_resp_msg *oem_read_buffer_resp_msg;
    struct battman_oem_write_buffer_resp_msg *oem_write_buffer_resp_msg;

    struct pmic_glink_hdr *hdr = data;
    bool ack_set = false;
    
    switch (hdr->opcode) {
    case OEM_OPCODE_READ_BUFFER:
        if (len == sizeof(*oem_read_buffer_resp_msg)) {
            oem_read_buffer_resp_msg = data;
            switch (oem_read_buffer_resp_msg->oem_property_id) {
            case BATTMAN_OEM_ADSP_PLATFORM_ID:
                ChgPD_Info.PlatformID = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_BATT_ID:
                ChgPD_Info.BATT_ID = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                if(ChgPD_Info.BATT_ID < 51000*1.15 && ChgPD_Info.BATT_ID > 51000*0.85)
                    asus_extcon_set_state_sync(bat_id_extcon, 1);
                else if(ChgPD_Info.BATT_ID < 100000*1.15 && ChgPD_Info.BATT_ID > 100000*0.85)
                    asus_extcon_set_state_sync(bat_id_extcon, 1);
                else
                    asus_extcon_set_state_sync(bat_id_extcon, 0);
                break;
            case BATTMAN_OEM_CHG_LIMIT_EN:
                CHG_DBG("%s BATTMAN_OEM_CHG_LIMIT_EN successfully\n", __func__);
                ChgPD_Info.chg_limit_en = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_CHG_LIMIT_CAP:
                CHG_DBG("%s BATTMAN_OEM_CHG_LIMIT_CAP successfully\n", __func__);
                ChgPD_Info.chg_limit_cap = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_USBIN_SUSPEND:
                ChgPD_Info.usbin_suspend_en = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_CHARGING_SUSPNED:
                CHG_DBG("%s BATTMAN_OEM_CHARGING_SUSPNED successfully\n", __func__);
                ChgPD_Info.charging_suspend_en = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_CHGPD_FW_VER:
                CHG_DBG("%s. ChgPD_FW : %s\n", __func__, (char*)oem_read_buffer_resp_msg->data_buffer);
                strcpy(ChgPD_Info.ChgPD_FW, (char*)oem_read_buffer_resp_msg->data_buffer);
                ack_set = true;
                break;
            case BATTMAN_OEM_FW_VERSION:
                ChgPD_Info.firmware_version = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_BATT_TEMP:
                ChgPD_Info.batt_temp = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_PM8350B_ICL:
                ChgPD_Info.pm8350b_icl = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_SMB1396_ICL:
                ChgPD_Info.smb1396_icl = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_FCC:
                ChgPD_Info.batt_fcc = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            case BATTMAN_OEM_AdapterVID:
                ChgPD_Info.AdapterVID = oem_read_buffer_resp_msg->data_buffer[0];
                ack_set = true;
                break;
            default:
                ack_set = true;
                pr_err("Unknown property_id: %u\n", oem_read_buffer_resp_msg->oem_property_id);
            }
        } else {
            pr_err("Incorrect response length %zu for OEM_OPCODE_READ_BUFFER\n", len);
        }
        break;
    case OEM_OPCODE_WRITE_BUFFER:
        if (len == sizeof(*oem_write_buffer_resp_msg)) {
            oem_write_buffer_resp_msg = data;
            switch (oem_write_buffer_resp_msg->oem_property_id) {
            case BATTMAN_OEM_CHG_LIMIT_EN:
            case BATTMAN_OEM_CHG_LIMIT_CAP:
            case BATTMAN_OEM_USBIN_SUSPEND:
            case BATTMAN_OEM_CHARGING_SUSPNED:
            case BATTMAN_OEM_PM8350B_ICL:
            case BATTMAN_OEM_SMB1396_ICL:
            case BATTMAN_OEM_FCC:
            case BATTMAN_OEM_DEBUG_MASK:
            case BATTMAN_OEM_SMB_Setting:
                CHG_DBG("%s set property:%d successfully\n", __func__, oem_write_buffer_resp_msg->oem_property_id);
                ack_set = true;
                break;
            default:
                ack_set = true;
                pr_err("Unknown property_id: %u\n", oem_write_buffer_resp_msg->oem_property_id);
            }
        } else {
            pr_err("Incorrect response length %zu for OEM_OPCODE_READ_BUFFER\n", len);
        }
        break;
    default:
        pr_err("Unknown opcode: %u\n", hdr->opcode);
        ack_set = true;
        break;
    }

    if (ack_set)
        complete(&bcdev->ack);
}

static int asusBC_msg_cb(void *priv, void *data, size_t len)
{
    struct pmic_glink_hdr *hdr = data;

    // pr_err("owner: %u type: %u opcode: %u len: %zu\n", hdr->owner, hdr->type, hdr->opcode, len);

    if (hdr->owner == PMIC_GLINK_MSG_OWNER_OEM) {
        if (hdr->type == MSG_TYPE_NOTIFY)
            handle_notification(g_bcdev, data, len);
        else
            handle_message(g_bcdev, data, len);
    }
    return 0;
}

static void asusBC_state_cb(void *priv, enum pmic_glink_state state)
{
    pr_err("Enter asusBC_state_cb\n");
}

static char *charging_stats[] = {
	"UNKNOWN",
	"CHARGING",
	"DISCHARGING",
	"NOT_CHARGING",
	"FULL"
};
char *health_type[] = {
	"UNKNOWN",
	"GOOD",
	"OVERHEAT",
	"DEAD",
	"OVERVOLTAGE",
	"UNSPEC_FAILURE",
	"COLD",
	"WATCHDOG_TIMER_EXPIRE",
	"SAFETY_TIMER_EXPIRE",
	"OVERCURRENT",
	"CALIBRATION_REQUIRED",
	"WARM",
	"COOL",
	"HOT"
};
static struct timespec64   g_last_print_time;
static void print_battery_status(void) {
	union power_supply_propval prop = {};
	char battInfo[256];
    int bat_cap, fcc,bat_vol,bat_cur,bat_temp,charge_status,bat_health,rc = 0;
    char UTSInfo[256]; //ASUS_BSP add to printk the WIFI hotspot & QXDM UTS event

    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_CAPACITY, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery SOC, rc=%d\n", rc);
    }
    bat_cap = prop.intval;

    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery full design, rc=%d\n", rc);
    }
    fcc = prop.intval;
    
    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery vol , rc=%d\n", rc);
    }
    bat_vol = prop.intval;
    
    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery current , rc=%d\n", rc);
    }
    bat_cur= prop.intval;
    
    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_TEMP, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery temp , rc=%d\n", rc);
    }
    bat_temp= prop.intval;
    
    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_STATUS, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery status , rc=%d\n", rc);
    }
    charge_status= prop.intval;

    rc = power_supply_get_property(qti_phy_bat,
        POWER_SUPPLY_PROP_HEALTH, &prop);
    if (rc < 0) {
        pr_err("Failed to get battery health , rc=%d\n", rc);
    }
    bat_health= prop.intval;

	snprintf(battInfo, sizeof(battInfo), "report Capacity ==>%d, FCC:%dmAh, BMS:%d, V:%dmV, Cur:%dmA, ",
			bat_cap,
			fcc/1000,
			bat_cap,
			bat_vol/1000,
			bat_cur/1000);
	snprintf(battInfo, sizeof(battInfo), "%sTemp:%d.%dC, BATID:%d, CHG_Status:%d(%s), BAT_HEALTH:%s \n",
			battInfo,
			bat_temp/10,
			bat_temp%10,
			ChgPD_Info.BATT_ID,
			charge_status,
			charging_stats[charge_status],
			health_type[bat_health]);

	ASUSEvtlog("[BAT][Ser]%s", battInfo);
	
	//ASUS_BSP +++ add to printk the WIFI hotspot & QXDM UTS event
	snprintf(UTSInfo, sizeof(UTSInfo), "WIFI_HS=%d, QXDM=%d", g_wifi_hs_en, g_qxdm_en);
	ASUSEvtlog("[UTS][Status]%s", UTSInfo);
	//ASUS_BSP --- add to printk the WIFI hotspot & QXDM UTS event
	
	ktime_get_coarse_real_ts64(&g_last_print_time);
	schedule_delayed_work(&g_bcdev->update_gauge_status_work, 180*HZ);
}

void static update_gauge_status_worker(struct work_struct *dat)
{
	print_battery_status();
}

int asus_chg_resume(struct device *dev)
{
    struct timespec64 mtNow;

    ktime_get_coarse_real_ts64(&mtNow);
    if (mtNow.tv_sec - g_last_print_time.tv_sec >= 180) {
			cancel_delayed_work(&g_bcdev->update_gauge_status_work);
            schedule_delayed_work(&g_bcdev->update_gauge_status_work, 0);
    }
	return 0;
}

//ASUS BSP : Show "+" on charging icon +++
void asus_set_qc_state_worker(struct work_struct *work)
{
    asus_extcon_set_state_sync(quickchg_extcon, g_SWITCH_LEVEL);
    CHG_DBG("%s: switchaa: %d\n", __func__, g_SWITCH_LEVEL);
}

void set_qc_stat(int status)
{
    CHG_DBG("%s: status: %d\n", __func__, status);

    if(!g_asuslib_init) return;
    switch (status) {
    //"qc" stat happends in charger mode only, refer to smblib_get_prop_batt_status
    case POWER_SUPPLY_STATUS_CHARGING:
    case POWER_SUPPLY_STATUS_NOT_CHARGING:
        cancel_delayed_work(&asus_set_qc_state_work);
        schedule_delayed_work(&asus_set_qc_state_work, msecs_to_jiffies(2000));
        break;
    default:
        cancel_delayed_work(&asus_set_qc_state_work);
        asus_extcon_set_state_sync(quickchg_extcon, SWITCH_LEVEL0_DEFAULT);
        break;
    }
}
//ASUS BSP : Show "+" on charging icon ---

int asuslib_init(void) {
    int rc = 0;
    struct pmic_glink_client_data client_data = { };
    struct pmic_glink_client    *client;

    printk(KERN_ERR "%s +++\n", __func__);
    // Initialize the necessary power supply
    rc = asus_init_power_supply_prop();
    if (rc < 0) {
        pr_err("Failed to init power_supply chains\n");
        return rc;
    }

    // Register the class node
    rc = class_register(&asuslib_class);
    if (rc) {
        pr_err("%s: Failed to register asuslib class\n", __func__);
        return -1;
    }

    if(g_ASUS_hwID <= HW_REV_EVB2)
    {
        POGO_OTG_GPIO = of_get_named_gpio(g_bcdev->dev->of_node, "POGO_OTG_EN", 0);
        rc = gpio_request(POGO_OTG_GPIO, "POGO_OTG_EN");
        if (rc) {
            pr_err("%s: Failed to initalize the POGO_OTG_EN\n", __func__);
            return -1;
        }
    }

    if(g_ASUS_hwID >= HW_REV_ER)
    {
        OTG_LOAD_SWITCH_GPIO = of_get_named_gpio(g_bcdev->dev->of_node, "OTG_LOAD_SWITCH", 0);
        rc = gpio_request(OTG_LOAD_SWITCH_GPIO, "OTG_LOAD_SWITCH");
        if (rc) {
            pr_err("%s: Failed to initalize the OTG_LOAD_SWITCH\n", __func__);
            return -1;
        }

        if (gpio_is_valid(OTG_LOAD_SWITCH_GPIO)) {
            rc = gpio_direction_output(OTG_LOAD_SWITCH_GPIO, 0);
            if (rc)
                pr_err("%s. Failed to control OTG_Load_Switch\n", __func__);
        } else {
            CHG_DBG_E("%s. OTG_LOAD_SWITCH_GPIO is invalid\n", __func__);
        }
    }

    //[+++] Init the PMIC-GLINK
    client_data.id = PMIC_GLINK_MSG_OWNER_OEM;
    client_data.name = "asus_BC";
    client_data.msg_cb = asusBC_msg_cb;
    client_data.priv = g_bcdev;
    client_data.state_cb = asusBC_state_cb;
    client = pmic_glink_register_client(g_bcdev->dev, &client_data);
    if (IS_ERR(client)) {
        rc = PTR_ERR(client);
        if (rc != -EPROBE_DEFER)
            dev_err(g_bcdev->dev, "Error in registering with pmic_glink %d\n",
                rc);
        return rc;
    }
    //[---] Init the PMIC-GLINK

    //[+++] Init the info structure of ChargerPD from ADSP
    ChgPD_Info.PlatformID = 0;
    ChgPD_Info.BATT_ID = 0;
    // ChgPD_Info.VBUS_SRC = 0;
    ChgPD_Info.chg_limit_en = 0;
    ChgPD_Info.chg_limit_cap = 0;
    ChgPD_Info.usbin_suspend_en = 0;
    ChgPD_Info.charging_suspend_en = 0;
    ChgPD_Info.firmware_version = 0;
    ChgPD_Info.pm8350b_icl = 0;
    ChgPD_Info.smb1396_icl = 0;
    ChgPD_Info.batt_fcc = 0;
    //[---] Init the info structure of ChargerPD from ADSP
    bat_extcon = extcon_dev_allocate(asus_fg_extcon_cable);
    if (IS_ERR(bat_id_extcon)) {
        rc = PTR_ERR(bat_extcon);
    }       
    bat_extcon->fnode_name = "battery";
    printk("[BAT]extcon_dev_register");
    rc = extcon_dev_register(bat_extcon);
    bat_extcon->name = st_battery_name;
    bat_id_extcon = extcon_dev_allocate(asus_fg_extcon_cable);
    if (IS_ERR(bat_id_extcon)) {
        rc = PTR_ERR(bat_id_extcon);
    }       
    bat_id_extcon->fnode_name = "battery_id";
    printk("[BAT]extcon_dev_register");
    rc = extcon_dev_register(bat_id_extcon);

    //[+++]Register the extcon for quick_charger
    quickchg_extcon = extcon_dev_allocate(asus_fg_extcon_cable);
    if (IS_ERR(quickchg_extcon)) {
        rc = PTR_ERR(quickchg_extcon);
        printk(KERN_ERR "[BAT][CHG] failed to allocate ASUS quickchg extcon device rc=%d\n", rc);
    }
    quickchg_extcon->fnode_name = "quick_charging";

    rc = extcon_dev_register(quickchg_extcon);
    if (rc < 0)
        printk(KERN_ERR "[BAT][CHG] failed to register ASUS quickchg extcon device rc=%d\n", rc);

    asus_extcon_set_state_sync(quickchg_extcon, SWITCH_LEVEL0_DEFAULT);

    INIT_DELAYED_WORK(&asus_set_qc_state_work, asus_set_qc_state_worker);
    //[---]Register the extcon for quick_charger

    asus_get_Batt_ID();

	usb_conn_temp_vadc_chan = iio_channel_get(g_bcdev->dev, "pm8350b_amux_thm4");
	if (IS_ERR_OR_NULL(usb_conn_temp_vadc_chan)) {
		CHG_DBG_E("%s: usb_conn_temp iio_channel_get fail\n", __func__);
	}

	create_uts_status_proc_file(); //ASUS_BSP LiJen add to printk the WIFI hotspot & QXDM UTS event

	INIT_DELAYED_WORK(&g_bcdev->asus_usb_thermal_work, asus_usb_thermal_work);
	schedule_delayed_work(&g_bcdev->asus_usb_thermal_work, msecs_to_jiffies(0));
	
	INIT_DELAYED_WORK(&g_bcdev->update_gauge_status_work, update_gauge_status_worker);
	schedule_delayed_work(&g_bcdev->update_gauge_status_work, 0);
    CHG_DBG_E("Load the asuslib_init Succesfully\n");
    g_asuslib_init = true;
    return rc;
}

int asuslib_deinit(void) {
    g_asuslib_init = false;
    class_unregister(&asuslib_class);
    return 0;
}
