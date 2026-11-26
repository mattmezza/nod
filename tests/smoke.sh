#!/bin/sh
# Exercise the release in a separate X server and session bus.
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" != --isolated ]; then
    for cmd in xvfb-run dbus-run-session gdbus dbus-send xrdb; do
        command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
    done
    exec xvfb-run -a dbus-run-session -- "$0" --isolated
fi
tmp=$(mktemp -d)
log=$tmp/nod.log
export XDG_CONFIG_HOME=$tmp/config
mkdir -p "$XDG_CONFIG_HOME/nod"
cat > "$XDG_CONFIG_HOME/nod/config.ini" <<'EOF'
[layout]
max_visible = 0

[normal]
accent = #79c0ff

[dnd]
muted_app = Muted App
muted_app = Quiet, Inc.
EOF
./nod >"$log" 2>&1 &
daemon=$!
cleanup() {
    kill "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true
    cat "$log"
    rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM
call() {
    gdbus call --session --dest org.freedesktop.Notifications \
        --object-path /org/freedesktop/Notifications \
        --method "org.freedesktop.Notifications.$@"
}
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    if info=$(call GetServerInformation 2>/dev/null); then ready=1; break; fi
    sleep 0.2
done
[ "$ready" = 1 ] || { echo 'Daemon did not start' >&2; exit 1; }
grep -Fq 'invalid value for [layout] max_visible' "$log"
version=$(sed -n 's/^VERSION = //p' config.mk)
case "$info" in *"'$version'"*) ;; *) echo "Wrong server version: $info"; exit 1 ;; esac
case "$(./nod-send --version)" in *"$version"*) ;; *) exit 1 ;; esac
case "$(call GetCapabilities)" in *"'body', 'actions'"*) ;; *) exit 1 ;; esac
id=$(./nod-send -p -t 0 'Release smoke test' 'Persistent notification')
case "$id" in ''|*[!0-9]*) echo "Invalid notification ID: $id"; exit 1 ;; esac
[ "$(./nod-send -p -r "$id" 'Updated notification' 'Replacement')" = "$id" ]
call CloseNotification "$id" >/dev/null
./nod-send -a 'MUTED APP' 'Muted by config'
./nod-send -a 'Quiet, Inc.' 'Also muted by config'
[ "$(./nod-dnd off)" = off ]
[ "$(./nod-dnd on)" = on ]
[ "$(./nod-dnd status)" = on ]
./nod-send -u critical 'Suppressed during DND' 'Critical notifications respect DND'
./nod-send --transient 'Transient during DND'
history=$(gdbus call --session --dest org.freedesktop.Notifications \
    --object-path /org/freedesktop/Notifications --method org.nod.History.List)
case "$history" in *'Suppressed during DND'*) ;; *) echo 'Missing notification history'; exit 1 ;; esac
case "$history" in *'Transient during DND'*) echo 'Stored a transient notification'; exit 1 ;; esac
case "$history" in *'Muted by config'*|*'Also muted by config'*) echo 'Stored a muted notification'; exit 1 ;; esac
[ "$(./nod-dnd)" = off ]
printf 'nod.background: #102030\nnod.foreground: #ffffff\n' | xrdb -merge
kill -USR1 "$daemon"
./nod-send -t 100 'After theme reload' 'Daemon remains responsive'
kill -0 "$daemon"
echo "PASS: nod $version server, config, capabilities, send, replace, close, DND and theme reload"
