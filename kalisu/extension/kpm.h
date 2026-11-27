/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KALISU_EXTENSION_KPM_H
#define __KALISU_EXTENSION_KPM_H

#include <linux/types.h>

#include "extension.h"

extern const struct ksu_extension_handler_ops kalisu_extension_kpm_ops;
int kalisu_extension_kpm_io(u32 extension_id, void __user *arg);

#endif /* __KALISU_EXTENSION_KPM_H */
