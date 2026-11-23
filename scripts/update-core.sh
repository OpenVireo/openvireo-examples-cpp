#!/bin/sh
# Points cmake/OpenVireoCore.cmake at another core release: the version and each platform's
# libovcore checksum, from the release's SHA256SUMS.
#   scripts/update-core.sh 1.0.1
set -eu
version=${1:?usage: scripts/update-core.sh <version>}
cd "$(dirname "$0")/.."
sums=$(curl -fsSL "https://github.com/openvireo/openvireo-core/releases/download/v$version/SHA256SUMS")
file=cmake/OpenVireoCore.cmake
sed -i.bak "s/^set(OPENVIREO_CORE_VERSION .*)/set(OPENVIREO_CORE_VERSION $version)/" "$file"
for platform in darwin-arm64 darwin-x64 linux-arm64 linux-x64 windows-x64; do
  sum=$(printf '%s\n' "$sums" | awk -v f="libovcore-$version-$platform" '$2 ~ "^"f"\\.(tar\\.gz|zip)$" { print $1 }')
  [ -n "$sum" ] || { echo "no libovcore-$version-$platform in SHA256SUMS" >&2; mv "$file.bak" "$file"; exit 1; }
  sed -i.tmp "s/^set(_ovcore_sha256_$platform .*)/set(_ovcore_sha256_$platform $sum)/" "$file" && rm "$file.tmp"
done
rm "$file.bak"
echo "OpenVireo core $version"
