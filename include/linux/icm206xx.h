/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_ICM206XX_H
#define _LINUX_ICM206XX_H

#include <linux/errno.h>
#include <linux/kconfig.h>

#if IS_REACHABLE(CONFIG_ASUS_PICASSO_ICM206XX)
int icm_reset_ois_channel(void);
#else
static inline int icm_reset_ois_channel(void)
{
	return -ENODEV;
}
#endif

#endif
