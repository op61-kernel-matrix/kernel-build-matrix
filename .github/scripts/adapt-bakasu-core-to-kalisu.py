#!/usr/bin/env python3
"""Adapt the boot-proven BakaSU GKI2 core to the released KaliSU manager ABI.

The base commit is pinned by the workflow.  This adapter changes only the
manager authority, public version/brand, and the KaliSU Bridge ABI consumed by
KaliSU 3.0.0-30000-7.  It deliberately leaves the proven hook, preinit, SuSFS,
and GKI2 implementation intact.
"""

from __future__ import annotations

import argparse
from pathlib import Path


KALISU_PACKAGE = "osbocc.wiyjay.urkykk"
KALISU_CERT_SIZE = "0x53b"
KALISU_CERT_SHA256 = (
    "d8ed664c1c83cf2a49ee45f3c1e6cecdc7ecc2697fcb877db03ecd70713aa546"
)


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one occurrence, found {count}: {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def insert_before_once(path: Path, marker: str, addition: str) -> None:
    replace_once(path, marker, addition + marker)


def adapt(root: Path, bridge_source: Path) -> None:
    kernel = root / "kernel"
    uapi = root / "uapi" / "supercall.h"
    kbuild = kernel / "Kbuild"
    manager_sign = kernel / "manager" / "manager_sign.h"
    init = kernel / "core" / "init.c"
    dispatch = kernel / "supercall" / "dispatch.c"

    for required in (uapi, kbuild, manager_sign, init, dispatch):
        if not required.is_file():
            raise RuntimeError(f"missing pinned-core file: {required}")

    bridge_dir = kernel / "bridge"
    bridge_dir.mkdir(parents=True, exist_ok=True)
    for name in ("kalisu_bridge.c", "kalisu_bridge.h"):
        source = bridge_source / name
        if not source.is_file():
            raise RuntimeError(f"missing KaliSU Bridge source: {source}")
        (bridge_dir / name).write_bytes(source.read_bytes())

    replace_once(kbuild, "kernelsu-objs := core/init.o\n", "kernelsu-objs := core/init.o\nkernelsu-objs += bridge/kalisu_bridge.o\n")
    replace_once(kbuild, "REPO_NAME := BakaSU", "REPO_NAME := KaliSU")
    replace_once(
        kbuild,
        "KSU_VERSION := $(shell expr 30000 + $(KSU_LOCAL_VERSION) + 700)",
        "KSU_VERSION := 30000",
    )
    replace_once(
        kbuild,
        "KSU_VERSION_FULL := $(subst %KSU_VERSION%,$(KSU_VERSION),$(KSU_VERSION_FULL))",
        "KSU_VERSION_FULL := $(subst %KSU_VERSION%,$(KSU_VERSION),$(KSU_VERSION_FULL))\n"
        "KSU_VERSION_FULL := v3.0.0-548d1dda@KaliSU",
    )
    insert_before_once(
        kbuild,
        "# Custom Signs\n",
        f"KSU_MANAGER_PACKAGE := {KALISU_PACKAGE}\n\n",
    )

    replace_once(
        manager_sign,
        "// Baka-SU/BakaSU\n#define EXPECTED_SIZE_BAKASU 0x377\n"
        '#define EXPECTED_HASH_BAKASU "d3469712b6214462764a1d8d3e5cbe1d6819a0b629791b9f4101867821f1df64"',
        "// KaliSU 3.0.0-30000-7 release APK\n"
        f"#define EXPECTED_SIZE_BAKASU {KALISU_CERT_SIZE}\n"
        f'#define EXPECTED_HASH_BAKASU "{KALISU_CERT_SHA256}"',
    )
    replace_once(
        kernel / "manager" / "apk_sign.c",
        "{ EXPECTED_SIZE_BAKASU, EXPECTED_HASH_BAKASU }, /* Baka-SU/BakaSU */",
        "{ EXPECTED_SIZE_BAKASU, EXPECTED_HASH_BAKASU }, /* KaliSU release authority */",
    )

    replace_once(
        init,
        '#include "uapi/supercall.h"\n',
        '#include "uapi/supercall.h"\n#include "bridge/kalisu_bridge.h"\n',
    )
    replace_once(
        init,
        '    pr_info("initializing symbol resolver\\n");\n    ksu_init_symbol_resolver();\n',
        '    pr_info("initializing symbol resolver\\n");\n    ksu_init_symbol_resolver();\n'
        '    ret = kalisu_bridge_init();\n'
        '    if (ret)\n'
        '        pr_warn("KaliSU Bridge diagnostic endpoint unavailable: %d\\n", ret);\n',
    )
    replace_once(
        init,
        "    ksu_module_blacklist_exit();\n\n    put_cred(ksu_cred);",
        "    ksu_module_blacklist_exit();\n    kalisu_bridge_exit();\n\n    put_cred(ksu_cred);",
    )

    replace_once(
        uapi,
        "// 6: add KSU_IOCTL_LOOKUP_SYMBOLS and KSU_GET_INFO_FLAG_PREINIT; modules.rc and preinit may live under /metadata/vold/ksu\n"
        "static const __u32 KERNEL_SU_UAPI_VERSION = 6;",
        "// 6: add KSU_IOCTL_LOOKUP_SYMBOLS and KSU_GET_INFO_FLAG_PREINIT; modules.rc and preinit may live under /metadata/vold/ksu\n"
        "// 7: add the authenticated KaliSU Bridge ABI 1 handshake\n"
        "static const __u32 KERNEL_SU_UAPI_VERSION = 7;",
    )
    bridge_uapi = """
/* KaliSU 3.0.0-30000-7 Bridge ABI, mirrored by libkernelsu.so. */
DEFINE_KSU_UAPI_CONST(__u32, KALISU_BRIDGE_MAGIC, 0x4b534231U)
DEFINE_KSU_UAPI_CONST(__u16, KALISU_BRIDGE_ABI_VERSION, 1)
DEFINE_KSU_UAPI_CONST(__u32, KALISU_UAPI_ABI_VERSION, 1)
DEFINE_KSU_UAPI_CONST(__u32, KALISU_BRIDGE_FLAG_AUTHENTICATED, (1U << 0))
DEFINE_KSU_UAPI_CONST(__u32, KALISU_BRIDGE_FLAG_CERT_REGISTRY, (1U << 1))
DEFINE_KSU_UAPI_CONST(__u32, KALISU_BRIDGE_FLAG_LKM, (1U << 2))
DEFINE_KSU_UAPI_CONST(__u32, KALISU_BRIDGE_FLAG_NATIVE, (1U << 3))

struct kalisu_bridge_cmd {
    __u32 magic;
    __u16 abi_version;
    __u16 struct_size;
    __u32 uapi_abi_version;
    __u32 uapi_version;
    __u32 core_version;
    __u32 flags;
    __u32 manager_appid;
    __u32 reserved;
    __u64 nonce[2];
};

"""
    insert_before_once(uapi, "/* IOCTL command definitions */\n", bridge_uapi)
    insert_before_once(
        uapi,
        "// Downstream add IOCTL command definitions\n",
        "DEFINE_KSU_UAPI_CONST(__u32, KALISU_IOCTL_BRIDGE, "
        "_IOWR('K', 23, struct kalisu_bridge_cmd))\n\n",
    )

    bridge_handler = r'''
static int do_kalisu_bridge(void __user *arg)
{
#ifdef CONFIG_KSU_DISABLE_MANAGER
    return -EOPNOTSUPP;
#else
    struct kalisu_bridge_cmd request;
    struct kalisu_bridge_cmd response = {
        .magic = KALISU_BRIDGE_MAGIC,
        .abi_version = KALISU_BRIDGE_ABI_VERSION,
        .struct_size = sizeof(struct kalisu_bridge_cmd),
        .uapi_abi_version = KALISU_UAPI_ABI_VERSION,
        .uapi_version = KERNEL_SU_UAPI_VERSION,
        .core_version = KERNEL_SU_VERSION,
        .flags = KALISU_BRIDGE_FLAG_AUTHENTICATED |
                 KALISU_BRIDGE_FLAG_CERT_REGISTRY,
        .manager_appid = ksu_get_uid_t(current_uid()) % 100000U,
    };

    if (copy_from_user(&request, arg, sizeof(request)))
        return -EFAULT;
    if (request.magic != KALISU_BRIDGE_MAGIC)
        return -EINVAL;
    if (request.struct_size != sizeof(struct kalisu_bridge_cmd))
        return -EMSGSIZE;
    if (request.abi_version != KALISU_BRIDGE_ABI_VERSION)
        return -EPROTO;
    if (!request.nonce[0] && !request.nonce[1])
        return -EINVAL;

    response.nonce[0] = request.nonce[0];
    response.nonce[1] = request.nonce[1];
#ifdef MODULE
    response.flags |= KALISU_BRIDGE_FLAG_LKM;
#else
    response.flags |= KALISU_BRIDGE_FLAG_NATIVE;
#endif
    if (copy_to_user(arg, &response, sizeof(response)))
        return -EFAULT;
    return 0;
#endif
}

'''
    insert_before_once(dispatch, "static int do_get_info(void __user *arg)\n", bridge_handler)
    bridge_map = '''    {
        .cmd = KALISU_IOCTL_BRIDGE,
        .name = "KALISU_BRIDGE",
        .handler = do_kalisu_bridge,
        .perm_check = only_manager
    },
'''
    insert_before_once(dispatch, "    // downstream begin\n", bridge_map)

    # The shipped APK compares kernel UAPI to manager UAPI 7 and issues this
    # exact ioctl number/size.  Fail adaptation if either contract drifts.
    adapted_uapi = uapi.read_text(encoding="utf-8")
    if "_IOWR('K', 23, struct kalisu_bridge_cmd)" not in adapted_uapi:
        raise RuntimeError("KaliSU Bridge ioctl contract was not installed")
    if "static const __u32 KERNEL_SU_UAPI_VERSION = 7;" not in adapted_uapi:
        raise RuntimeError("KaliSU UAPI 7 was not installed")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--bridge-source", required=True, type=Path)
    args = parser.parse_args()
    adapt(args.root.resolve(), args.bridge_source.resolve())


if __name__ == "__main__":
    main()
