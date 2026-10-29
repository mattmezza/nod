#!/bin/sh
# Regenerate vendor/cui-native from a CUI checkout (https://github.com/mattmezza/cui).
# Usage: scripts/update-cui.sh /path/to/cui
set -eu
src=${1:?usage: $0 /path/to/cui}
root=$(cd "$(dirname "$0")/.." && pwd)
rev=$(git -C "$src" rev-parse HEAD)
make -C "$src" native-dist
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
tar -xzf "$src/build/cui-native.tar.gz" -C "$tmp"
rm -rf "$root/vendor/cui-native"
mkdir -p "$root/vendor"
mv "$tmp/cui-native" "$root/vendor/cui-native"
# nod is its own host: drop the starter app, its Makefile and bundled fonts
cd "$root/vendor/cui-native"
rm -rf app.c main.c mail.c Makefile assets
echo "$rev" > REVISION
echo "vendored CUI $rev; run 'make distclean all'"
