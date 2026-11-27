#include <asm/barrier.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/string.h>

#include "feature/module_blacklist.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep

// Block kernel modules from loading by reusing the kernel's native
// `module_blacklist` (consulted by blacklisted() before a module is laid out),
// instead of hooking init_module/finit_module. It is the standard mechanism and
// needs nothing maintained on our side.
//
// The list is supplied through KaliSU's
// `kalisu_block_modules=name1,name2,...` parameter. It flows through the boot
// patch configuration or the kernel command line (built-in). Names use the
// kernel form (underscores, e.g.
// `oplus_secure_guard`).
//
// KaliSU installs the blacklist during core initialization, before userspace
// starts loading vendor modules. A blacklisted module fails with EPERM, and
// first stage init treats failure of a modules.load entry as fatal unless that
// entry is also present in modules.blocklist. LKM boot patching reconciles that
// ramdisk list. A native build must therefore enable only a device-specific
// default whose first-stage dependency graph was audited before the build.

#ifndef CONFIG_KALISU_BLOCK_MODULES_DEFAULT
#define CONFIG_KALISU_BLOCK_MODULES_DEFAULT ""
#endif

static char kalisu_block_modules[256] = CONFIG_KALISU_BLOCK_MODULES_DEFAULT;

// The param may be parsed before the slab allocator is up (built-in), so copy
// into a static buffer instead of allocating. Truncate rather than fail: a
// param error would abort the whole KaliSU load.
static int kalisu_block_modules_param_set(const char *val, const struct kernel_param *kp)
{
    if (!val || !val[0])
        return 0;

    if (strscpy(kalisu_block_modules, val, sizeof(kalisu_block_modules)) < 0)
        pr_err("kalisu_blocklist: kalisu_block_modules too long, truncated; some modules will not be blocked\n");

    return 0;
}

static const struct kernel_param_ops kalisu_block_modules_param_ops = {
    // A bare parameter would otherwise fail parse_args() and the load.
    .flags = KERNEL_PARAM_OPS_FL_NOARG,
    .set = kalisu_block_modules_param_set,
};

// module_param_cb() has no prefix for a loadable module, but Kbuild prefixes
// it when this source is built into the kernel. Register it as a core parameter
// in the native build so both architectures accept the exact same KaliSU key.
#ifdef MODULE
module_param_cb(kalisu_block_modules, &kalisu_block_modules_param_ops, NULL, 0);
#else
__core_param_cb(kalisu_block_modules, &kalisu_block_modules_param_ops, NULL, 0);
#endif
MODULE_PARM_DESC(kalisu_block_modules, "KaliSU comma-separated native module blocklist");

// Merge the configured names into the kernel's native module_blacklist.
void kalisu_module_blacklist_apply(void)
{
    static bool applied;
    char **native_blacklist;
    const char *existing;
    char *merged;
    size_t existing_len;

    if (applied || !kalisu_block_modules[0])
        return;
    applied = true;

    native_blacklist = (char **)find_kernel_symbol_exact("module_blacklist");
    if (!native_blacklist) {
        pr_warn("kalisu_blocklist: native module_blacklist symbol not found\n");
        return;
    }

    existing = *native_blacklist;
    existing_len = existing ? strlen(existing) : 0;

    // Merge with any existing blacklist (e.g. one set on the kernel cmdline).
    merged =
        kasprintf(GFP_KERNEL, "%s%s%s", existing_len ? existing : "", existing_len ? "," : "", kalisu_block_modules);
    if (!merged) {
        pr_err("kalisu_blocklist: cannot allocate blacklist\n");
        return;
    }

    // Release-publish: pairs with the address dependency in the lockless
    // blacklisted() reader so it can never observe the new pointer before the
    // string contents are visible.
    smp_store_release(native_blacklist, merged);
    // Force every other CPU through a memory barrier so any module load already
    // in flight observes the new list right away (suggested by @backslashxx).
    kick_all_cpus_sync();
    pr_info("kalisu_blocklist: blocking modules: %s\n", merged);
}

void __exit kalisu_module_blacklist_exit(void)
{
    // Intentionally does NOT restore the original module_blacklist pointer.
    // The module loader reads module_blacklist in blacklisted() without any
    // lock or READ_ONCE, testing it for NULL and then dereferencing it; swapping
    // the pointer back (especially to the common NULL) could race that reader
    // into a NULL dereference. The merged string already contains any
    // pre-existing entries and lives on the kernel heap, so leaving it installed
    // stays valid even after KaliSU is unloaded. The one-time, bounded
    // allocation is deliberately leaked.
}
