#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

LOG_FILE=${RK3506_UDP_LOG:-${RK3506_TCP_LOG:-/tmp/rk3506_udp_received.log}}
STATE_FILE=${RK3506_UDP_STATE:-${RK3506_TCP_STATE:-/tmp/rk3506_udp_nodes.tsv}}
DELETE_FILE=${RK3506_UDP_DELETE:-${RK3506_TCP_DELETE:-/tmp/rk3506_udp_delete.tsv}}
PORT=${RK3506_UDP_PORT:-${RK3506_TCP_PORT:-35060}}
SERVICE_LOG=${RK3506_UDP_SERVICE_LOG:-${RK3506_TCP_SERVICE_LOG:-/tmp/rk3506_udp_service.log}}

UDP_BIN=${RK3506_UDP_BIN:-${RK3506_TCP_BIN:-"$SCRIPT_DIR/rk3506_tcp/build/rk3506_tcp_receiver"}}
TUI_BIN=${HELLOWORLD_BIN:-"$SCRIPT_DIR/helloworld/build_x86/hellowWorld"}

if [ ! -x "$UDP_BIN" ] && [ -x "$SCRIPT_DIR/rk3506_udp_receiver" ]; then
    UDP_BIN="$SCRIPT_DIR/rk3506_udp_receiver"
fi

if [ ! -x "$UDP_BIN" ] && [ -x "$SCRIPT_DIR/rk3506_tcp_receiver" ]; then
    UDP_BIN="$SCRIPT_DIR/rk3506_tcp_receiver"
fi

if [ ! -x "$TUI_BIN" ] && [ -x "$SCRIPT_DIR/hellowWorld" ]; then
    TUI_BIN="$SCRIPT_DIR/hellowWorld"
fi

if [ ! -x "$UDP_BIN" ]; then
    echo "UDP receiver not found or not executable: $UDP_BIN" >&2
    exit 1
fi

if [ ! -x "$TUI_BIN" ]; then
    echo "hellowWorld not found or not executable: $TUI_BIN" >&2
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

"$UDP_BIN" --port "$PORT" --log-file "$LOG_FILE" --state-file "$STATE_FILE" --delete-file "$DELETE_FILE" >"$SERVICE_LOG" 2>&1 &
UDP_PID=$!

sleep 1

"$TUI_BIN" --log-file "$LOG_FILE" --state-file "$STATE_FILE" --delete-file "$DELETE_FILE"
