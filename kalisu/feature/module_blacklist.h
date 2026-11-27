#ifndef __KALISU_MODULE_BLACKLIST_H
#define __KALISU_MODULE_BLACKLIST_H

// Block the modules named in the `kalisu_block_modules` parameter by merging
// them into the kernel's native `module_blacklist`. Call during KaliSU core
// initialization, before vendor modules are loaded. Only the first call has an
// effect.
void kalisu_module_blacklist_apply(void);

// Teardown hook. Intentionally keeps the merged list installed; see the
// implementation for why the pointer is not restored.
void kalisu_module_blacklist_exit(void);

#endif
