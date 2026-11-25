#!/usr/bin/env python3
"""Reconcile SuSFS 2.4 SELinux hiding with the pinned KaliSU bridge tree.

Derived from Bouteillepleine/OnePlus-SukiSu_NMS's
susfs_sukisu_selinux_hide.py.  Keep the newer policy helpers, remove the old
runtime text/LSM hooking that SuSFS moves into the kernel tree, and expose only
the state used by the kernel-side SuSFS patch.
"""

from pathlib import Path
import re
import sys


ENABLE = '''static int ksu_selinux_hide_enable()
{
    pr_info("selinux_hide: init selinux hide\\n");
    if (!backup_sepolicy) {
        pr_err("no backup sepolicy available, please save feature and reboot to retry!\\n");
        return -EAGAIN;
    }
    security_dump_masked_av_fn = find_kernel_symbol_exact("security_dump_masked_av");
    if (!security_dump_masked_av_fn)
        pr_warn("security_dump_masked_av not found!\\n");
    context_struct_compute_av_fn = find_kernel_symbol_exact("context_struct_compute_av");
    if (!context_struct_compute_av_fn)
        pr_warn("context_struct_compute_av not found!\\n");

    return 0;
}

'''

CALLBACKS = '''void ksu_selinux_hide_handle_second_stage(void)
{
    initialize_fake_status();
    if (fake_status)
        static_key_disable(&fake_status_initialize_key.key);
    else
        pr_warn("selinux_hide: fake status needs late initialization\\n");
}

void ksu_selinux_hide_handle_post_fs_data(void)
{
    static_key_disable(&fake_status_initialize_key.key);
    if (!fake_status)
        pr_err("selinux_hide: fake status is not initialized after post-fs-data!\\n");
}

'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")

    if '#include "infra/symbol_resolver.h"' not in text:
        text = replace_once(
            text,
            '#include "selinux_hide.h"\n',
            '#include "selinux_hide.h"\n#include "infra/symbol_resolver.h"\n',
            "symbol-resolver include",
        )

    if 'find_kernel_symbol_exact("write_op")' in text:
        start = re.search(r'^typedef int \(\*sel_open_handle_status_fn\)[^\n]*\n', text, re.M)
        end = re.search(r'^static int selinux_hide_feature_get\(', text, re.M)
        if not start or not end or end.start() <= start.start():
            raise SystemExit("could not locate the obsolete SELinux runtime-hook region")
        callbacks = '' if 'void ksu_selinux_hide_handle_second_stage(void)' in text else CALLBACKS
        text = text[: start.start()] + callbacks + ENABLE + text[end.start() :]

    text, _ = re.subn(
        r'^static void hook_selinux_status_open\(\)\n\{.*?\n\}\n',
        '',
        text,
        count=1,
        flags=re.M | re.S,
    )

    # The kernel-side SuSFS patch references these objects directly.  They must
    # have external linkage; the original KaliSU implementation kept them local.
    visibility = {
        'static bool ksu_selinux_hide_enabled __read_mostly = false;':
            'bool ksu_selinux_hide_enabled __read_mostly = false;',
        'static bool ksu_selinux_hide_running __read_mostly = false;':
            'bool ksu_selinux_hide_running __read_mostly = false;',
        'static DEFINE_STATIC_KEY_FALSE(fake_status_initialize_key);':
            'DEFINE_STATIC_KEY_FALSE(fake_status_initialize_key);',
        'static struct page *fake_status = NULL;':
            'struct page *fake_status = NULL;',
        'static void initialize_fake_status()':
            'void initialize_fake_status()',
    }
    for old, new in visibility.items():
        if old in text:
            text = text.replace(old, new, 1)

    for fn in ('security_dump_masked_av_fn', 'context_struct_compute_av_fn'):
        text = text.replace(f'if (&{fn})', f'if ({fn})')

    forbidden = (
        'selinux_write_op',
        'ksu_patch_text(',
        'ksu_lsm_hook(',
        'ksu_lsm_unhook(',
        'hook_selinux_status_open',
        'ksu_selinux_hide_unhook',
        'orig_sel_open_handle_status',
    )
    survivors = [token for token in forbidden if token in text]
    if survivors:
        raise SystemExit('obsolete SELinux-hook remnants survived: ' + ', '.join(survivors))

    required = (
        'void ksu_selinux_hide_handle_second_stage(void)',
        'void ksu_selinux_hide_handle_post_fs_data(void)',
        'DEFINE_STATIC_KEY_FALSE(fake_status_initialize_key);',
        'struct page *fake_status = NULL;',
        'void initialize_fake_status()',
    )
    missing = [token for token in required if token not in text]
    if missing:
        raise SystemExit('required SuSFS SELinux bridge state missing: ' + ', '.join(missing))

    path.write_text(text, encoding="utf-8")
    print('KaliSU SELinux hide reconciled with the SuSFS 2.4 kernel-side implementation')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
