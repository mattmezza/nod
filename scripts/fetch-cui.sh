#!/bin/sh
# Download the pinned upstream revision and produce its native source bundle.
set -eu
revision=${1:?revision required}
checksum=${2:?checksum required}
destination=${3:?destination required}
[ ${#revision} -eq 40 ] || { echo 'Invalid CUI revision' >&2; exit 1; }
case "$revision" in *[!0-9a-f]*) exit 1;; esac
command -v python3 >/dev/null
command -v curl >/dev/null
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl --fail --location --retry 3 "https://codeload.github.com/mattmezza/cui/tar.gz/$revision" -o "$tmp/source.tar.gz"
printf '%s  %s\n' "$checksum" "$tmp/source.tar.gz" | sha256sum --check --status
mkdir "$tmp/source"
tar -xzf "$tmp/source.tar.gz" -C "$tmp/source" --strip-components=1
python3 "$tmp/source/scripts/native_bundle.py" --output "$tmp/native.tar.gz"
tar -xzf "$tmp/native.tar.gz" -C "$tmp"
rm -rf "$tmp/cui-native/app.c" "$tmp/cui-native/main.c" "$tmp/cui-native/mail.c" "$tmp/cui-native/Makefile" "$tmp/cui-native/assets"
printf '%s\n' "$revision" > "$tmp/cui-native/REVISION"
mkdir -p "$(dirname "$destination")"
rm -rf "$destination"
mv "$tmp/cui-native" "$destination"
echo "CUI $revision ready in $destination"
