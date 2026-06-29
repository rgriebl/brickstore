#!/bin/sh
# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only
#
# Launches a sandboxed, headless BrickStore instance with all MCP permissions
# enabled and runs mcp-smoke-test.py against it. The sandbox redirects config,
# data and temp (autosave) locations, so the instance cannot interfere with a
# regular BrickStore installation. Only the BrickLink database and image cache
# are shared (read-mostly) to avoid a lengthy database download.
#
# Usage: BRICKSTORE_BIN=<path-to-binary> mcp-test-sandbox.sh [--keep]

set -eu

BRICKSTORE_BIN="${BRICKSTORE_BIN:-$(dirname "$0")/../builds/qt-dev-desktop-debug/bin/brickstore}"
MCP_PORT="${MCP_PORT:-45111}"
REAL_CONF="${XDG_CONFIG_HOME:-$HOME/.config}/BrickStore/BrickStore.conf"
REAL_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/BrickStore"

[ -x "$BRICKSTORE_BIN" ] || { echo "BrickStore binary not found: $BRICKSTORE_BIN"; exit 1; }
[ -f "$REAL_CONF" ]      || { echo "No BrickStore config found: $REAL_CONF"; exit 1; }
[ -d "$REAL_CACHE" ]     || { echo "No BrickLink cache/database found: $REAL_CACHE"; exit 1; }

SANDBOX="$(mktemp -d /tmp/brickstore-mcp-sandbox-XXXXXX)"
mkdir -p "$SANDBOX/config/BrickStore" "$SANDBOX/data" "$SANDBOX/tmp" "$SANDBOX/work"

# Patch the copied config: all MCP permissions, the requested port, shared
# BrickLink cache, no session restore.
python3 - "$REAL_CONF" "$SANDBOX/config/BrickStore/BrickStore.conf" "$REAL_CACHE" "$MCP_PORT" <<'EOF'
import configparser, sys
cp = configparser.RawConfigParser()
cp.optionxform = str  # QSettings keys are case-sensitive
cp.read(sys.argv[1])
for section in ("MCP", "BrickLink", "General"):
    if not cp.has_section(section):
        cp.add_section(section)
cp.set("MCP", "Permissions", "31")
cp.set("MCP", "Port", sys.argv[4])
cp.set("BrickLink", "CacheDir", sys.argv[3])
cp.set("General", "RestoreLastSession", "false")
# Don't try to reopen last session's documents: they don't exist in the sandbox
# and the resulting "could not open" dialog would block the (headless) UI.
if cp.has_section("MainWindow"):
    cp.remove_option("MainWindow", "LastSessionDocuments")
with open(sys.argv[2], "w") as f:
    cp.write(f, space_around_delimiters=False)
EOF

LOG="$SANDBOX/brickstore.log"
echo "Sandbox: $SANDBOX"
echo "Log:     $LOG"

# The offscreen platform cannot be used: the Quick3D-based LDraw renderer
# asserts on non-RHI scenegraphs. Xvfb provides GL via Mesa instead.
command -v xvfb-run >/dev/null || { echo "xvfb-run is required"; exit 1; }

XDG_CONFIG_HOME="$SANDBOX/config" \
XDG_DATA_HOME="$SANDBOX/data" \
TMPDIR="$SANDBOX/tmp" \
setsid xvfb-run -a "$BRICKSTORE_BIN" --new-instance >"$LOG" 2>&1 &
APP_PID=$!

cleanup() {
    pkill -TERM -g "$APP_PID" 2>/dev/null || true
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        kill -0 "$APP_PID" 2>/dev/null || break
        sleep 1
    done
    pkill -KILL -g "$APP_PID" 2>/dev/null || true
    if [ "${1:-}" != "--keep" ]; then
        rm -rf "$SANDBOX"
    fi
}
trap 'cleanup "${1:-}"' EXIT INT TERM

# Wait for the MCP server to come up (a database update can delay this).
i=0
until grep -q "Started MCP server" "$LOG" 2>/dev/null; do
    kill -0 "$APP_PID" 2>/dev/null || { echo "BrickStore exited early:"; tail "$LOG"; exit 1; }
    i=$((i + 1))
    [ "$i" -le 120 ] || { echo "Timeout waiting for the MCP server:"; tail "$LOG"; exit 1; }
    sleep 1
done

# Exercise both transports against the same running instance. Each run gets its
# own workdir, so the documents one run saves/opens don't clash with the other.
python3 "$(dirname "$0")/mcp-smoke-test.py" --port "$MCP_PORT" --workdir "$SANDBOX/work-http" --transport http
python3 "$(dirname "$0")/mcp-smoke-test.py" --port "$MCP_PORT" --workdir "$SANDBOX/work-sse" --transport sse
