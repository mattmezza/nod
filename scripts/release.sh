#!/bin/sh
# Build, package and publish a source release from a clean committed checkout.
set -eu
cd "$(dirname "$0")/.."
name=${1:-}
printf '%s\n' "$name" | LC_ALL=C grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+$' || {
    echo 'Usage: make release name=v0.1.0' >&2; exit 1;
}
version=$(sed -n 's/^VERSION *= *//p' config.mk)
[ "$name" = "v$version" ] || { echo "Set VERSION in config.mk to ${name#v} first" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo 'Commit all changes before releasing' >&2; exit 1; }
command -v gh >/dev/null
gh auth status >/dev/null 2>&1
git rev-parse --verify "refs/tags/$name" >/dev/null 2>&1 && { echo "Tag $name already exists" >&2; exit 1; }
if git ls-remote --exit-code --tags origin "refs/tags/$name" >/dev/null 2>&1; then
    echo "Remote tag $name already exists" >&2; exit 1
fi
make fetch-cui
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
prefix="nod-$version"
mkdir "$tmp/$prefix"
git archive HEAD | tar -xf - -C "$tmp/$prefix"
mkdir -p "$tmp/$prefix/vendor"
cp -R vendor/cui-native "$tmp/$prefix/vendor/"
# Fresh-extraction build verifies the release's offline dependency payload.
make -C "$tmp/$prefix" -j4 all
[ "$("$tmp/$prefix/nod" -v)" = "nod $version" ] || { echo 'Binary version check failed' >&2; exit 1; }
"$tmp/$prefix/tests/smoke.sh"
make -C "$tmp/$prefix" distclean
tar -czf "$prefix.tar.gz" -C "$tmp" "$prefix"
sha256sum "$prefix.tar.gz" > "$prefix.tar.gz.sha256"
git tag -a "$name" -m "nod $name"
git push origin HEAD "refs/tags/$name"
gh release create "$name" "$prefix.tar.gz" "$prefix.tar.gz.sha256" \
    --verify-tag --title "nod $name" --notes-file CHANGELOG.md
