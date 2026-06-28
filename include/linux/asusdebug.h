/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASUSDEBUG_H__
#define __ASUSDEBUG_H__

#include <linux/printk.h>

static inline __printf(1, 2) void ASUSEvtlog(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

#endif
