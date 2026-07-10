#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

LOG_FILE=${RK3506_HOST_UDP_LOG:-/tmp/rk3506_host_udp_received.log}
STATE_FILE=${RK3506_HOST_UDP_STATE:-/tmp/rk3506_host_udp_nodes.tsv}
DELETE_FILE=${RK3506_HOST_UDP_DELETE:-/tmp/rk3506_host_udp_delete.tsv}
SERVICE_LOG=${RK3506_HOST_UDP_SERVICE_LOG:-/tmp/rk3506_host_udp_service.log}
PORT=${RK3506_HOST_UDP_PORT:-35060}

UDP_BIN=${RK3506_HOST_UDP_BIN:-"$SCRIPT_DIR/rk3506_tcp/build_x86/rk3506_tcp_receiver"}
TUI_BIN=${HELLOWORLD_HOST_BIN:-"$SCRIPT_DIR/helloworld/build_x86/hellowWorld"}

if [ ! -x "$UDP_BIN" ]; then
    echo "Host UDP receiver not found or not executable: $UDP_BIN" >&2
    echo "Build it first or set RK3506_HOST_UDP_BIN." >&2
    exit 1
fi

if [ ! -x "$TUI_BIN" ]; then
    echo "Host hellowWorld not found or not executable: $TUI_BIN" >&2
    echo "Build it first or set HELLOWORLD_HOST_BIN." >&2
    exit 1
fi

cleanup()
{
    if [ -n "${UDP_PID:-}" ]; then
        kill "$UDP_PID" 2>/dev/null || true
        wait "$UDP_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

rm -f "$DELETE_FILE"

"$UDP_BIN" \
    --port "$PORT" \
    --log-file "$LOG_FILE" \
    --state-file "$STATE_FILE" \
    --delete-file "$DELETE_FILE" \
    >"$SERVICE_LOG" 2>&1 &
UDP_PID=$!

sleep 1

"$TUI_BIN" \
    --log-file "$LOG_FILE" \
    --state-file "$STATE_FILE" \
    --delete-file "$DELETE_FILE"
