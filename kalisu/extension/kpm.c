// SPDX-License-Identifier: GPL-2.0
/* KaliSU extension-hub wiring for the KaliSU-owned KPM bridge. */
#include <linux/errno.h>

#include "extension.h"
#include "kpm.h"
#include "kpm/bridge.h"
#include "kpm/kpm.h"

static int kalisu_extension_kpm_get(u32 extension_id, u64 *value)
{
    (void)extension_id;
    *value = kalisu_kpm_bridge(KALISU_KPM_BRIDGE_PROBE, 0, 0, 0, 0) ==
             KALISU_KPM_BRIDGE_ABI_VERSION;
    return 0;
}

const struct ksu_extension_handler_ops kalisu_extension_kpm_ops = {
    .get = kalisu_extension_kpm_get,
};

int kalisu_extension_kpm_io(u32 extension_id, void __user *arg)
{
    (void)extension_id;
    if (kalisu_kpm_bridge(KALISU_KPM_BRIDGE_PROBE, 0, 0, 0, 0) !=
        KALISU_KPM_BRIDGE_ABI_VERSION)
        return -EOPNOTSUPP;
    return kalisu_do_kpm(arg);
}
