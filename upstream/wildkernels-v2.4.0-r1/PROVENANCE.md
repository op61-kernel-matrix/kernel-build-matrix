# Boot-Proven Upstream Snapshot

This directory is an immutable metadata and compatibility snapshot used by the validation branch.

- Repository: `WildKernels/OnePlus_KernelSU_SUSFS`
- Release: `v2.4.0-r1`
- Release URL: <https://github.com/WildKernels/OnePlus_KernelSU_SUSFS/releases/tag/v2.4.0-r1>
- Tagged commit: `10f9741ee0bbff5adf69f3260019c7ec5ba63be1`
- Snapshot contents: 169 device configurations, 169 pinned manifests, compatibility documentation, and two compatibility scripts
- Boot-proven reference artifact: `AK3_OP13r_A16_android14-6.1.157_KSUN_33497_SuSFS_v2.4.0.zip`
- Reference artifact SHA-256: `81124e056a076f2262eec3c911355df0b3986b17be6d53511de85aa1fdb9e03e`

## Reference source pins

- Common kernel: `7a7ae28e849b0f9b42b8b6fc26feebe1864bf7ac`
- Modules/devicetree: `dda73f7205501c09f7d0f48fd5f6d7f4b2220ca7`

GitHub Actions caches are deliberately not copied. They are repository-scoped, mutable acceleration data rather than reproducible source inputs. The new repository will create its own caches from these pinned sources after a successful build.
