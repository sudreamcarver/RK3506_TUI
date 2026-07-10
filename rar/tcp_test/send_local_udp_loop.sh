#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

TYPE=${1:-imu}
COUNT=${2:-500}
INTERVAL_MS=${3:-0}
MODE=${4:-unique}
PORT=${5:-35060}
HOST=${6:-127.0.0.1}

SENDER=${UDP_SENSOR_SENDER:-${TCP_SENSOR_SENDER:-"$SCRIPT_DIR/build/tcp_sensor_sender"}}

if [ ! -x "$SENDER" ]; then
    echo "UDP sensor sender not found or not executable: $SENDER" >&2
    echo "Build it first:" >&2
    echo "  cmake -S tcp_test -B tcp_test/build" >&2
    echo "  cmake --build tcp_test/build -j4" >&2
    exit 1
fi

case "$TYPE" in
    voltage|current|temperature|imu|motor|battery)
        ;;
    *)
        echo "Unsupported sensor type: $TYPE" >&2
        echo "Supported types: voltage current temperature imu motor battery" >&2
        exit 1
        ;;
esac

case "$MODE" in
    unique|same)
        ;;
    *)
        echo "Unsupported mode: $MODE" >&2
        echo "Supported modes: unique same" >&2
        exit 1
        ;;
esac

echo "Sending $COUNT UDP sensor-data frames to $HOST:$PORT"
echo "type=$TYPE interval=${INTERVAL_MS}ms mode=$MODE"

case "$TYPE" in
    voltage)
        SENSOR_ID_BASE=1001
        ;;
    current)
        SENSOR_ID_BASE=2001
        ;;
    temperature)
        SENSOR_ID_BASE=3001
        ;;
    imu)
        SENSOR_ID_BASE=10001
        ;;
    motor)
        SENSOR_ID_BASE=20001
        ;;
    battery)
        SENSOR_ID_BASE=30001
        ;;
esac

if [ "$MODE" = "same" ]; then
    "$SENDER" \
        --host "$HOST" \
        --port "$PORT" \
        --message sensor-data \
        --type "$TYPE" \
        --count "$COUNT" \
        --interval-ms "$INTERVAL_MS"
else
    "$SENDER" \
        --host "$HOST" \
        --port "$PORT" \
        --message sensor-data \
        --type "$TYPE" \
        --sensor-id "$SENSOR_ID_BASE" \
        --sensor-id-step 1 \
        --count "$COUNT" \
        --interval-ms "$INTERVAL_MS"
fi

echo "done"
