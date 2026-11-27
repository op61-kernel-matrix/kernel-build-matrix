// SPDX-License-Identifier: GPL-2.0
/*
 * KaliSU-owned diagnostic side of KaliSU Bridge.
 *
 * Authentication lives exclusively in KALISU_IOCTL_BRIDGE. These root-readable
 * files identify the running core and its bridge ABI for recovery/debugging;
 * their presence is never sufficient to authorize a manager APK.
 */
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

#include "bridge/kalisu_bridge.h"
#include "ksu.h"
#include "uapi/supercall.h"

static struct kobject *kalisu_bridge_kobj;

static ssize_t identity_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "KaliSU\n");
}

static ssize_t abi_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", (unsigned int)KALISU_BRIDGE_ABI_VERSION);
}

static ssize_t uapi_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u %u\n", (unsigned int)KALISU_UAPI_ABI_VERSION,
                      (unsigned int)KERNEL_SU_UAPI_VERSION);
}

static ssize_t core_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", (unsigned int)KERNEL_SU_VERSION);
}

static ssize_t runtime_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#ifdef MODULE
    return sysfs_emit(buf, "lkm%s\n", ksu_late_loaded ? "-late" : "");
#else
    return sysfs_emit(buf, "native\n");
#endif
}

static ssize_t features_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf,
                      "authenticated_uapi=1 certificate_registry=1 "
                      "owned_identity=1\n");
}

static struct kobj_attribute identity_attr = __ATTR(identity, 0400, identity_show, NULL);
static struct kobj_attribute abi_attr = __ATTR(abi, 0400, abi_show, NULL);
static struct kobj_attribute uapi_attr = __ATTR(uapi, 0400, uapi_show, NULL);
static struct kobj_attribute core_attr = __ATTR(core, 0400, core_show, NULL);
static struct kobj_attribute runtime_attr = __ATTR(runtime, 0400, runtime_show, NULL);
static struct kobj_attribute features_attr = __ATTR(features, 0400, features_show, NULL);

static struct attribute *kalisu_bridge_attrs[] = {
    &identity_attr.attr, &abi_attr.attr, &uapi_attr.attr, &core_attr.attr, &runtime_attr.attr,
    &features_attr.attr, NULL,
};

static const struct attribute_group kalisu_bridge_group = {
    .attrs = kalisu_bridge_attrs,
};

int kalisu_bridge_init(void)
{
    int ret;

    kalisu_bridge_kobj = kobject_create_and_add("kalisu_bridge", kernel_kobj);
    if (!kalisu_bridge_kobj)
        return -ENOMEM;

    ret = sysfs_create_group(kalisu_bridge_kobj, &kalisu_bridge_group);
    if (ret) {
        kobject_put(kalisu_bridge_kobj);
        kalisu_bridge_kobj = NULL;
        return ret;
    }

    pr_info("KaliSU Bridge v%u diagnostic endpoint ready\n", (unsigned int)KALISU_BRIDGE_ABI_VERSION);
    return 0;
}

void kalisu_bridge_exit(void)
{
    if (!kalisu_bridge_kobj)
        return;

    sysfs_remove_group(kalisu_bridge_kobj, &kalisu_bridge_group);
    kobject_put(kalisu_bridge_kobj);
    kalisu_bridge_kobj = NULL;
}
