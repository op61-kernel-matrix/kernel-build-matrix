/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KaliSU KPM UAPI adapter. User pointers are copied into bounded kernel
 * buffers before the private provider bridge receives them.
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#ifdef CONFIG_ARM64
#include <asm/ptrace.h>
#endif

#include "infra/symbol_resolver.h"
#include "uapi/extension.h"
#include "bridge.h"
#include "kpm.h"

#define KALISU_KPM_NAME_LEN 32
#define KALISU_KPM_ARGS_LEN 1024
#define KALISU_KPM_PATH_LEN 1024
#define KALISU_KPM_EVENT_LEN 64
#define KALISU_KPM_INFO_LEN 256

long __nocfi kalisu_kpm_bridge(unsigned long op, unsigned long a1,
                               unsigned long a2, unsigned long a3,
                               unsigned long a4)
{
#ifdef CONFIG_ARM64
    typedef long (*handler_t)(const struct pt_regs *);
    static handler_t handler;
    struct pt_regs regs = { 0 };
    handler_t fn = READ_ONCE(handler);

    if (!fn) {
        fn = (handler_t)find_kernel_symbol_exact("__arm64_sys_truncate");
        if (!fn)
            fn = (handler_t)find_kernel_symbol_exact(
                "__arm64_sys_truncate.cfi_jt");
        if (!fn)
            fn = (handler_t)find_kernel_symbol_exact(
                "__arm64_sys_truncate.cfi");
        if (!fn)
            return -EOPNOTSUPP;
        WRITE_ONCE(handler, fn);
    }

    regs.pstate = PSR_MODE_EL1h;
    regs.regs[0] = KALISU_KPM_BRIDGE_MAGIC;
    regs.regs[6] = KALISU_KPM_BRIDGE_MAGIC2;
    regs.regs[7] = KALISU_KPM_BRIDGE_MAGIC3;
    regs.regs[8] = KALISU_KPM_BRIDGE_ABI_VERSION;
    regs.regs[1] = KALISU_KPM_BRIDGE_PROBE;

    if (fn(&regs) != KALISU_KPM_BRIDGE_ABI_VERSION)
        return -EOPNOTSUPP;

    regs.regs[1] = op;
    regs.regs[2] = a1;
    regs.regs[3] = a2;
    regs.regs[4] = a3;
    regs.regs[5] = a4;
    return fn(&regs);
#else
    return -EOPNOTSUPP;
#endif
}

static int kalisu_copy_string(char *buf, const char __user *src, size_t size,
                              bool optional)
{
    long len;

    buf[0] = '\0';
    if (!src)
        return optional ? 0 : -EINVAL;
    len = strncpy_from_user(buf, src, size);
    if (len < 0)
        return len;
    if (len >= size)
        return -ENAMETOOLONG;
    if (!optional && !len)
        return -EINVAL;
    return 0;
}

static int kalisu_copy_output(void __user *out, const char *buf, long len,
                              size_t size)
{
    if (len < 0)
        return len;
    if (len >= size)
        return -ENOBUFS;
    return copy_to_user(out, buf, len + 1) ? -EFAULT : 0;
}

static int kalisu_handle_kpm_load_event(const struct kalisu_kpm_cmd *cmd)
{
    struct kalisu_kpm_load_event_cmd load;
    char *path = NULL;
    char *args = NULL;
    char *event = NULL;
    int rc;

    if (copy_from_user(&load, u64_to_user_ptr(cmd->arg2), sizeof(load)))
        return -EFAULT;

    path = kzalloc(KALISU_KPM_PATH_LEN, GFP_KERNEL);
    args = kzalloc(KALISU_KPM_ARGS_LEN, GFP_KERNEL);
    event = kzalloc(KALISU_KPM_EVENT_LEN, GFP_KERNEL);
    if (!path || !args || !event) {
        rc = -ENOMEM;
        goto out;
    }

    rc = kalisu_copy_string(path, u64_to_user_ptr(cmd->arg1),
                            KALISU_KPM_PATH_LEN, false);
    if (!rc)
        rc = kalisu_copy_string(args, u64_to_user_ptr(load.args),
                                KALISU_KPM_ARGS_LEN, true);
    if (!rc)
        rc = kalisu_copy_string(event, u64_to_user_ptr(load.event),
                                KALISU_KPM_EVENT_LEN, false);
    if (!rc)
        rc = kalisu_kpm_bridge(KALISU_KPM_BRIDGE_LOAD_EVENT,
                               (unsigned long)path, (unsigned long)args,
                               (unsigned long)event, 0);

out:
    kfree(event);
    kfree(args);
    kfree(path);
    return rc;
}

static int kalisu_handle_kpm(const struct kalisu_kpm_cmd *cmd)
{
    const char __user *arg1 = u64_to_user_ptr(cmd->arg1);
    const char __user *arg2 = u64_to_user_ptr(cmd->arg2);
    char *buf;
    char *name;
    char *args;
    long rc;
    size_t size;

    if (cmd->control_code == KALISU_KPM_NUM)
        return kalisu_kpm_bridge(KALISU_KPM_BRIDGE_NUM, 0, 0, 0, 0);

    if (cmd->control_code == KALISU_KPM_LOAD_EVENT)
        return kalisu_handle_kpm_load_event(cmd);

    if (cmd->control_code == KALISU_KPM_LOAD ||
        cmd->control_code == KALISU_KPM_CONTROL) {
        size = cmd->control_code == KALISU_KPM_LOAD ?
               KALISU_KPM_PATH_LEN : KALISU_KPM_NAME_LEN;
        buf = kzalloc(size + KALISU_KPM_ARGS_LEN, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;
        args = buf + size;
        rc = kalisu_copy_string(buf, arg1, size, false);
        if (!rc)
            rc = kalisu_copy_string(args, arg2, KALISU_KPM_ARGS_LEN,
                                    cmd->control_code == KALISU_KPM_LOAD);
        if (!rc)
            rc = kalisu_kpm_bridge(cmd->control_code,
                                   (unsigned long)buf,
                                   (unsigned long)args, 0, 0);
        kfree(buf);
        return rc;
    }

    if (cmd->control_code == KALISU_KPM_UNLOAD) {
        name = kmalloc(KALISU_KPM_NAME_LEN, GFP_KERNEL);
        if (!name)
            return -ENOMEM;
        rc = kalisu_copy_string(name, arg1, KALISU_KPM_NAME_LEN, false);
        if (!rc)
            rc = kalisu_kpm_bridge(KALISU_KPM_BRIDGE_UNLOAD,
                                   (unsigned long)name, 0, 0, 0);
        kfree(name);
        return rc;
    }

    if (cmd->control_code == KALISU_KPM_INFO) {
        name = kmalloc(KALISU_KPM_NAME_LEN + KALISU_KPM_INFO_LEN,
                       GFP_KERNEL);
        if (!name)
            return -ENOMEM;
        buf = name + KALISU_KPM_NAME_LEN;
        rc = kalisu_copy_string(name, arg1, KALISU_KPM_NAME_LEN, false);
        if (!rc) {
            rc = kalisu_kpm_bridge(KALISU_KPM_BRIDGE_INFO,
                                   (unsigned long)name,
                                   (unsigned long)buf,
                                   KALISU_KPM_INFO_LEN, 0);
            rc = kalisu_copy_output((void __user *)arg2, buf, rc,
                                    KALISU_KPM_INFO_LEN);
        }
        kfree(name);
        return rc;
    }

    if (cmd->control_code != KALISU_KPM_LIST &&
        cmd->control_code != KALISU_KPM_VERSION)
        return -EOPNOTSUPP;
    if (!cmd->arg2 || cmd->arg2 > KALISU_KPM_BRIDGE_OUTPUT_MAX)
        return -EINVAL;

    size = cmd->arg2;
    buf = kzalloc(size, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    rc = kalisu_kpm_bridge(cmd->control_code, (unsigned long)buf,
                           size, 0, 0);
    if (rc >= 0) {
        long length = rc;

        rc = kalisu_copy_output((void __user *)arg1, buf, length, size);
        if (!rc && cmd->control_code == KALISU_KPM_LIST)
            rc = length;
    }
    kfree(buf);
    return rc;
}

int kalisu_do_kpm(void __user *arg)
{
    struct kalisu_kpm_cmd cmd;
    int result;

    /* Reached only through KaliSU's manager-or-root-authorized extension IO. */
    if (copy_from_user(&cmd, arg, sizeof(cmd)))
        return -EFAULT;
    if (!access_ok(u64_to_user_ptr(cmd.result_code), sizeof(result)))
        return -EFAULT;
    result = kalisu_handle_kpm(&cmd);
    return copy_to_user(u64_to_user_ptr(cmd.result_code), &result,
                        sizeof(result)) ? -EFAULT : 0;
}
