/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KALISU_KPM_BRIDGE_H_
#define _KALISU_KPM_BRIDGE_H_

/*
 * KaliSU kernel-to-kernel KPM ABI v1. This must match the provider source in
 * KaliSU-KernelPatch. The tags identify the protocol and are not credentials;
 * KaliSU authenticates the manager/root caller before creating this request.
 */
#define KALISU_KPM_BRIDGE_MAGIC  0x4b414c494b504d01UL
#define KALISU_KPM_BRIDGE_MAGIC2 0x4b414c4942524447UL
#define KALISU_KPM_BRIDGE_MAGIC3 0x4b414c49494e5446UL

#define KALISU_KPM_BRIDGE_ABI_VERSION 1
#define KALISU_KPM_BRIDGE_OUTPUT_MAX 4096

enum kalisu_kpm_bridge_op {
    KALISU_KPM_BRIDGE_PROBE = 0,
    KALISU_KPM_BRIDGE_LOAD = 1,
    KALISU_KPM_BRIDGE_UNLOAD = 2,
    KALISU_KPM_BRIDGE_NUM = 3,
    KALISU_KPM_BRIDGE_LIST = 4,
    KALISU_KPM_BRIDGE_INFO = 5,
    KALISU_KPM_BRIDGE_CONTROL = 6,
    KALISU_KPM_BRIDGE_VERSION = 7,
    KALISU_KPM_BRIDGE_LOAD_EVENT = 8,
};

long kalisu_kpm_bridge(unsigned long op, unsigned long a1, unsigned long a2,
                       unsigned long a3, unsigned long a4);

#endif /* _KALISU_KPM_BRIDGE_H_ */
