/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_ASUSDEBUG_H
#define _LINUX_ASUSDEBUG_H

#include <linux/compiler.h>

/*
 * Picasso recovery bring-up interface. Events go to the normal printk
 * ring buffer only; no /asdf files, reserved-memory logger or OEM
 * task/locking instrumentation is installed by this implementation.
 */
void ASUSEvtlog(const char *fmt, ...) __printf(1, 2);

#endif
