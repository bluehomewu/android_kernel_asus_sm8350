// SPDX-License-Identifier: GPL-2.0-only
/* Picasso-only extraction of ASUS init/main.c board parameters. */
#include <linux/asus_hwid.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

char g_lcd_unique_id[10];
bool g_Charger_mode;
char g_verified_boot_state[20];
char g_unlock[2];
int g_ftm_mode;
enum DEVICE_HWID g_ASUS_hwID = HW_REV_INVALID;
enum DEVICE_PROJID g_ASUS_prjID = PROJECT_INVALID;
enum DEVICE_SKUID g_ASUS_skuID = SKU_ID_INVALID;
enum DEVICE_NFCID g_ASUS_nfcID = NFC_VENDOR_INVALID;
enum DEVICE_FPID g_ASUS_fpID = FP_VENDOR_INVALID;
enum DEVICE_DDRID g_ASUS_ddrID = DDR_VENDOR_INVALID;
bool g_is_country_code_QC;
bool g_is_country_code_WW;
bool g_is_country_code_EU;
bool g_is_country_code_RU;
bool g_is_country_code_US;
bool g_is_country_code_CN;
EXPORT_SYMBOL(g_lcd_unique_id);
EXPORT_SYMBOL(g_Charger_mode);
EXPORT_SYMBOL(g_verified_boot_state);
EXPORT_SYMBOL(g_unlock);
EXPORT_SYMBOL(g_ftm_mode);
EXPORT_SYMBOL(g_ASUS_hwID);
EXPORT_SYMBOL(g_ASUS_prjID);
EXPORT_SYMBOL(g_ASUS_skuID);
EXPORT_SYMBOL(g_ASUS_nfcID);
EXPORT_SYMBOL(g_ASUS_fpID);
EXPORT_SYMBOL(g_ASUS_ddrID);
EXPORT_SYMBOL(g_is_country_code_QC);
EXPORT_SYMBOL(g_is_country_code_WW);
EXPORT_SYMBOL(g_is_country_code_EU);
EXPORT_SYMBOL(g_is_country_code_RU);
EXPORT_SYMBOL(g_is_country_code_US);
EXPORT_SYMBOL(g_is_country_code_CN);

static int __init set_lcd_unique_id(char *str)
{
	/* Do not interpret bootloader data as a printf format string. */
	strlcpy(g_lcd_unique_id, str, sizeof(g_lcd_unique_id));
	return 0;
}
__setup("LCD=", set_lcd_unique_id);

static int __init set_charger_mode(char *str)
{
	g_Charger_mode = !strcmp(str, "charger");
	return 0;
}
__setup("androidboot.mode=", set_charger_mode);

static int __init verified_boot_state_param(char *str)
{
	strlcpy(g_verified_boot_state, str, sizeof(g_verified_boot_state));
	return 1;
}
__setup("androidboot.verifiedbootstate=", verified_boot_state_param);

static int __init unlock_param(char *str)
{
	strlcpy(g_unlock, str, sizeof(g_unlock));
	return 1;
}
__setup("UNLOCKED", unlock_param);

static int __init set_ftm_mode(char *str)
{
	g_ftm_mode = !strcmp(str, "1");
	return 0;
}
__setup("androidboot.pre-ftm=", set_ftm_mode);

/* The OEM accepts exactly one decimal digit for these board identifiers. */
static int __init board_digit(const char *str, int max)
{
	if (!str[0] || str[1] || str[0] < '0' || str[0] > '0' + max)
		return -1;
	return str[0] - '0';
}

static int __init set_hardware_id(char *str)
{
	int value = board_digit(str, 7);
	if (value >= 0)
		g_ASUS_hwID = value;
	return 0;
}
__setup("androidboot.id.stage=", set_hardware_id);

static int __init set_project_id(char *str)
{
	int value = board_digit(str, 2);
	if (value >= 0)
		g_ASUS_prjID = value;
	return 0;
}
__setup("androidboot.id.prj=", set_project_id);

static int __init set_sku_id(char *str)
{
	int value = board_digit(str, 7);
	if (value >= 0)
		g_ASUS_skuID = value;
	return 0;
}
__setup("androidboot.id.sku=", set_sku_id);

static int __init set_nfc_id(char *str)
{
	int value = board_digit(str, 1);
	if (value >= 0)
		g_ASUS_nfcID = value;
	return 0;
}
__setup("androidboot.id.nfc=", set_nfc_id);

static int __init set_fp_id(char *str)
{
	int value = board_digit(str, 1);
	if (value >= 0)
		g_ASUS_fpID = value;
	return 0;
}
__setup("androidboot.id.fp=", set_fp_id);

static int __init check_country_code(char *str)
{
	if (!strcmp(str, "QC"))
		g_is_country_code_QC = true;
	else if (!strcmp(str, "WW"))
		g_is_country_code_WW = true;
	else if (!strcmp(str, "EU"))
		g_is_country_code_EU = true;
	else if (!strcmp(str, "RU"))
		g_is_country_code_RU = true;
	else if (!strcmp(str, "US"))
		g_is_country_code_US = true;
	else if (!strcmp(str, "CN"))
		g_is_country_code_CN = true;
	return 0;
}
__setup("androidboot.country_code=", check_country_code);
