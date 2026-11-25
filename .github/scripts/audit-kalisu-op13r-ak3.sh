#!/usr/bin/env bash
set -euo pipefail

zip_path=${1:?usage: audit-kalisu-op13r-ak3.sh ZIP_PATH}
zip_name=$(basename "$zip_path")
[[ -s "$zip_path" ]] || { echo "::error::Missing KaliSU AK3 archive: $zip_path"; exit 1; }
[[ "$zip_name" == NOSTALGIC-AK3_OP13r_A16_android14-6.1.157_KaliSU_30000-6_NoMount_vSuSFS-2.4.0.zip ]] || {
  echo "::error::Unexpected canary filename: $zip_name"
  exit 1
}

audit_dir=$(mktemp -d)
trap 'rm -rf "$audit_dir"' EXIT
unzip -q "$zip_path" -d "$audit_dir"

for required in Image anykernel.sh banner OP13r-6.1.157_A16.txt; do
  [[ -s "$audit_dir/$required" ]] || { echo "::error::Missing AK3 payload: $required"; exit 1; }
done

cmp -s "$audit_dir/banner" "$GITHUB_WORKSPACE/.github/actions/build-gaming-kernel/assets/kalisu-banner" || {
  echo "::error::KaliSU flashing banner mismatch"
  exit 1
}

installer=$audit_dir/anykernel.sh
grep -Fq 'kernel.string=KaliSU OP13R 6.1.157 canary' "$installer"
grep -Fq 'ui_print "[KaliSU SPOOFED]"' "$installer"
grep -Fq 'Core stack: KaliSU 3.0.0 (30000-6) + SuSFS 2.4.0 GKI-2' "$installer"
grep -Fq 'NoMount: development inline VFS engine' "$installer"

if grep -RaiEq 'nostalg|wild[[:space:]_-]*kernels?|fatal|bakasu|finasu|sukisu|resuki|nova|ksun|kernelsu[[:space:]_-]*next|bouteillepleine' \
  "$installer" "$audit_dir/banner"; then
  echo "::error::Foreign public branding survived in KaliSU installer files"
  exit 1
fi

echo "KaliSU OP13R AnyKernel3 audit passed: $zip_name"
