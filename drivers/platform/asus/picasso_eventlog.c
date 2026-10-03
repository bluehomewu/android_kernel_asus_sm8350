// SPDX-License-Identifier: GPL-2.0-only
/* Minimal Picasso event logging for recovery bring-up; not OEM ASDF logging. */
#include <linux/asusdebug.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/printk.h>

void ASUSEvtlog(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	printk(KERN_INFO "ASUS: %pV\n", &vaf);
	va_end(args);
}
EXPORT_SYMBOL(ASUSEvtlog);
