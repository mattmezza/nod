#!/bin/sh
set -eu

repo=https://github.com/mattmezza/nod
files='bin/nod bin/nod-send bin/nod-dnd
share/man/man1/nod.1 share/man/man1/nod-send.1 share/man/man1/nod-dnd.1
share/nod/config.ini'

fail() {
    printf 'nod installer: %s\n' "$*" >&2
    exit 1
}

usage() {
    printf '%s\n' 'Usage: install.sh [install|upgrade] [vMAJOR.MINOR.PATCH]' \
        '       install.sh uninstall' \
        "Set NOD_PREFIX to override the default prefix (\$HOME/.local)."
}

download() {
    curl --proto '=https' --proto-redir '=https' -fsSL --retry 3 "$@"
}

remove_links() {
    for file in $files; do
        if [ "$(readlink "$prefix/$file" || :)" = "$store/current/$file" ]; then
            rm -f -- "$prefix/$file"
        fi
    done
}

cleanup() {
    if [ "$(readlink "$store/current" || :)" != "${stage##*/}" ]; then
        rm -rf -- "$stage"
        if [ -z "$previous" ]; then
            remove_links
        fi
    fi
}

main() {
    action=${1:-install}
    if [ "$#" -gt 0 ]; then
        shift
    fi
    case "$action" in
        -h|--help) usage; return 0 ;;
        install|upgrade)
            [ "$#" -le 1 ] || fail 'expected at most one release tag'
            platform="$(uname -s):$(uname -m)"
            case "$platform" in
                Linux:x86_64) arch=x86_64 ;;
                Linux:aarch64|Linux:arm64) arch=aarch64 ;;
                *) fail "no binary for $platform" ;;
            esac
            ;;
        uninstall) [ "$#" -eq 0 ] || fail 'uninstall takes no release tag' ;;
        *) usage >&2; fail "unknown command $action" ;;
    esac

    prefix=${NOD_PREFIX:-"${HOME:?Set HOME or NOD_PREFIX}/.local"}
    case "$prefix" in
        /*) ;;
        *) fail 'NOD_PREFIX must be an absolute path' ;;
    esac
    if [ "$action" = uninstall ] && [ ! -d "$prefix" ]; then
        return 0
    fi
    umask 022
    mkdir -p -- "$prefix"
    prefix=$(cd -P -- "$prefix" && pwd -P)
    prefix=${prefix%/}
    store="$prefix/lib/nod"

    # Hold the same lock for installation and removal, including across upgrades.
    exec 9<"${prefix:-/}"
    flock -x 9
    previous=$(readlink "$store/current" || :)
    case "$previous" in
        .release.*/*) fail "unexpected link at $store/current" ;;
        .release.*) ;;
        '') [ ! -e "$store/current" ] || fail "unexpected file at $store/current" ;;
        *) fail "unexpected link at $store/current" ;;
    esac

    if [ "$action" = uninstall ]; then
        remove_links
        if [ -n "$previous" ]; then
            rm -f -- "$store/current"
            rm -rf -- "${store:?}/$previous"
            rmdir -- "$store" 2>/dev/null || :
        fi
        printf 'Uninstalled nod from %s\n' "${prefix:-/}"
        return 0
    fi

    tag=${1:-}
    if [ -z "$tag" ]; then
        latest=$(curl --proto '=https' -fsS --retry 3 -o /dev/null \
            -w '%{redirect_url}' "$repo/releases/latest")
        case "$latest" in
            "$repo/releases/tag/"*) tag=${latest##*/} ;;
            *) fail 'could not find the latest published release' ;;
        esac
    fi
    case "$tag" in *[!a-zA-Z0-9.-]*) fail "invalid release tag: $tag" ;; esac
    printf '%s\n' "$tag" | LC_ALL=C grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9][a-zA-Z0-9.-]*)?$' \
        || fail "invalid release tag: $tag"

    for file in $files; do
        if [ -e "$prefix/$file" ] || [ -L "$prefix/$file" ]; then
            [ "$(readlink "$prefix/$file" || :)" = "$store/current/$file" ] \
                || fail "installation path already in use: $prefix/$file"
        fi
    done

    mkdir -p -- "$store" "$prefix/bin" "$prefix/share/man/man1" "$prefix/share/nod"
    stage=$(mktemp -d "$store/.release.XXXXXXXX")
    trap cleanup EXIT
    trap 'exit 1' HUP INT TERM

    asset="nod-linux-$arch.tar.gz"
    base="$repo/releases/download/$tag"
    download "$base/$asset.sha256" -o "$stage/$asset.sha256" \
        || fail "static assets for $tag ($arch) are unavailable"
    digest=$(awk -v file="$asset" '$2 == file { print $1 }' "$stage/$asset.sha256")
    [ "${#digest}" -eq 64 ] || fail "missing or ambiguous checksum for $asset"
    case "$digest" in *[!0-9a-f]*) fail "invalid checksum for $asset" ;; esac
    download "$base/$asset" -o "$stage/$asset"
    (cd "$stage" && printf '%s  %s\n' "$digest" "$asset" | sha256sum -c -) \
        || fail "checksum verification failed for $asset"
    tar -oxzf "$stage/$asset" -C "$stage"
    rm -f -- "$stage/$asset" "$stage/$asset.sha256"
    for file in $files; do
        [ -f "$stage/$file" ] || fail "archive is missing $file"
    done
    [ "$("$stage/bin/nod" -v)" = "nod ${tag#v}" ] || fail "unexpected nod version in $asset"

    # A single rename activates the whole package. Running copies keep their open files.
    for file in $files; do
        if [ ! -L "$prefix/$file" ]; then
            ln -sT -- "$store/current/$file" "$prefix/$file"
        fi
    done
    chmod 755 "$stage"
    ln -s -- "${stage##*/}" "$stage/next"
    mv -Tf -- "$stage/next" "$store/current"
    if [ -n "$previous" ]; then
        rm -rf -- "${store:?}/$previous"
    fi
    printf 'Installed nod %s in %s/bin\n' "$tag" "$prefix"
}

main "$@"
