#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

HOST=${1:-127.0.0.1}
PORT=${2:-35060}
TYPE=${3:-imu}
INTERVAL_MS=${4:-0}
COUNT=${5:-500}
MODE=${6:-unique}

SENDER=${UDP_SENSOR_SENDER:-${TCP_SENSOR_SENDER:-"$SCRIPT_DIR/build/tcp_sensor_sender"}}

if [ ! -x "$SENDER" ]; then
    echo "UDP sensor sender not found or not executable: $SENDER" >&2
    echo "Build it first:" >&2
    echo "  cmake -S tcp_test -B tcp_test/build" >&2
    echo "  cmake --build tcp_test/build -j4" >&2
    exit 1
fi

echo "Sending $COUNT UDP frames to $HOST:$PORT, type=$TYPE, interval=${INTERVAL_MS}ms, mode=$MODE"

if [ "$MODE" = "same" ]; then
    "$SENDER" \
        --host "$HOST" \
        --port "$PORT" \
        --type "$TYPE" \
        --count "$COUNT" \
        --interval-ms "$INTERVAL_MS"
else
    "$SENDER" \
        --host "$HOST" \
        --port "$PORT" \
        --type "$TYPE" \
        --sensor-id 1 \
        --sensor-id-step 1 \
        --count "$COUNT" \
        --interval-ms "$INTERVAL_MS"
fi

echo "done"
