#!/bin/sh
# Build and package a release from a fresh checkout in the static build image.
set -eu
cd "$(dirname "$0")/.."

tag=$1
arch=$2
version=$(sed -n 's/^VERSION *= *//p' config.mk)
[ "$tag" = "v$version" ] || {
    echo "Release tag $tag must match VERSION ($version) in config.mk" >&2
    exit 1
}
[ "$(uname -m)" = "$arch" ] || {
    echo "Expected a native $arch build host" >&2
    exit 1
}

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
trap 'exit 1' HUP INT TERM

make -j4 PKG_CONFIG='pkg-config --static' \
    LDFLAGS="-static -Wl,--start-group \$(LIBS) -lstdc++ -Wl,--end-group"
strip nod
headers=$(readelf -lW -d nod)
if printf '%s\n' "$headers" | grep -E 'INTERP|\(NEEDED\)'; then
    echo 'nod has a dynamic interpreter or library dependency' >&2
    exit 1
fi
[ "$(./nod -v)" = "nod $version" ]
./tests/smoke.sh

make install DESTDIR="$stage" PREFIX=/usr MANPREFIX=/usr/share/man
licenses="$stage/usr/share/licenses/nod"
mkdir -p "$licenses"
cp -R /usr/local/share/nod-licenses/. "$licenses/"
cp LICENSE "$licenses/nod"
cp vendor/cui-native/LICENSE "$licenses/CUI"
cp vendor/cui-native/vendor/third_party/clay/LICENSE "$licenses/clay"
cp vendor/cui-native/vendor/third_party/sheenbidi/LICENSE "$licenses/SheenBidi"
cp vendor/cui-native/vendor/third_party/unicode/*/LICENSE.txt "$licenses/Unicode"

archive="nod-linux-$arch.tar.gz"
tar -czf "$archive" -C "$stage/usr" bin share
sha256sum "$archive" > "$archive.sha256"
