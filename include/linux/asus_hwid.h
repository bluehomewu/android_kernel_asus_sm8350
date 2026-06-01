#ifndef _ASUS_HWID_H
#define _ASUS_HWID_H

/*
============================================================================
PICASSO HW ID
============================================================================


*/


#if defined CONFIG_MACH_ASUS_ZS673KS || defined CONFIG_MACH_ASUS_PICASSO
enum DEVICE_HWID
{
       HW_REV_INVALID 	= -1,
       HW_REV_EVB    		= 0,
       HW_REV_EVB2   		= 1,
       HW_REV_SR     		= 2,
       HW_REV_ER     		= 3,
       HW_REV_ER2    		= 4,
       HW_REV_PR     		= 5,
       HW_REV_PR2    		= 6,
       HW_REV_MP     		= 7
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_HWID g_ASUS_hwID;
#endif
enum DEVICE_PROJID
{
       PROJECT_INVALID = -1,
       PROJECT_ANAKIN_ENTRY = 0,
       PROJECT_ANAKIN_ELITE = 1,
       PROJECT_PICASSO	    = 2
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_PROJID g_ASUS_prjID;
#endif

enum DEVICE_SKUID
{
       SKU_ID_INVALID = -1,
       SKU_ID_0  = 0,
       SKU_ID_1  = 1,
       SKU_ID_2  = 2,
       SKU_ID_3  = 3,
       SKU_ID_4  = 4,
       SKU_ID_5  = 5,
       SKU_ID_6  = 6,
       SKU_ID_7  = 7
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_SKUID g_ASUS_skuID;
#endif

enum DEVICE_FPID
{
        FP_VENDOR_INVALID = -1,
        FP_VENDOR1 = 0,
        FP_VENDOR2 = 1
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_FPID g_ASUS_fpID;
#endif

enum DEVICE_NFCID
{
        NFC_VENDOR_INVALID = -1,
        NFC_NOT_SUPPORT = 0,
        NFC_SUPPORT = 1
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_NFCID g_ASUS_nfcID;
#endif

enum DEVICE_DDRID
{
        DDR_VENDOR_INVALID = -1,
        DDR_6400 = 0,
        DDR_5500 = 1
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_DDRID g_ASUS_ddrID;
#endif
#endif // #if defined CONFIG_MACH_ASUS_ZS673KS || defined CONFIG_MACH_ASUS_PICASSO

#ifdef CONFIG_MACH_ASUS_ZS673KS
enum DEVICE_BCID
{
        BC_ID_INVALID    = -1,
        BC_ID_AURA_Light =  0,
        BC_ID_PMOLED     =  1
};
extern enum DEVICE_BCID g_ASUS_bcID;

enum DEVICE_SECDISPID
{
        SEC_DISP_ID_INVALID = -1,
        SEC_DISP_ID_MONO    =  0,
        SEC_DISP_ID_COLOR   =  1
};
#ifdef CONFIG_MACH_ASUS
extern enum DEVICE_SECDISPID g_ASUS_secdispID;
#endif
#endif

#endif //#ifndef _ASUS_HWID_H
