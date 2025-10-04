#!/bin/bash

# Live Text System - System Launcher
# This script helps launch the complete system with proper configuration

set -e

echo "Live Text System - Launcher"
echo "============================"

# Configuration
MEDIA_DRIVER_JAR="third_party/aeron/build/lib/aeron-all-*-SNAPSHOT.jar"
BUILD_DIR="build"

# Check if built
if [ ! -f "$BUILD_DIR/sender" ] || [ ! -f "$BUILD_DIR/receiver" ]; then
    echo "Error: Applications not built. Please run 'make' in the build directory first."
    exit 1
fi

# Function to start media driver
start_media_driver() {
    echo "Starting Aeron Media Driver..."

    # Find the JAR file
    JAR_FILE=$(ls $MEDIA_DRIVER_JAR 2>/dev/null | head -1)

    if [ -z "$JAR_FILE" ]; then
        echo "Warning: Aeron JAR not found. Using embedded driver mode."
        return 1
    fi

    java -cp "$JAR_FILE" \
         -Daeron.threading.mode=DEDICATED \
         -Daeron.dir.delete.on.start=true \
         -Daeron.term.buffer.sparse.file=false \
         io.aeron.driver.MediaDriver &

    MEDIA_DRIVER_PID=$!
    echo "Media Driver started with PID: $MEDIA_DRIVER_PID"
    sleep 2
    return 0
}

# Function to start receiver
start_receiver() {
    echo "Starting Live Text Receiver..."
    cd "$BUILD_DIR"
    ./receiver &
    RECEIVER_PID=$!
    echo "Receiver started with PID: $RECEIVER_PID"
    cd ..
}

# Function to start sender
start_sender() {
    echo "Starting Live Text Sender..."
    cd "$BUILD_DIR"
    ./sender &
    SENDER_PID=$!
    echo "Sender started with PID: $SENDER_PID"
    cd ..
}

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down Live Text System..."

    if [ ! -z "$SENDER_PID" ]; then
        echo "Stopping Sender (PID: $SENDER_PID)..."
        kill $SENDER_PID 2>/dev/null || true
    fi

    if [ ! -z "$RECEIVER_PID" ]; then
        echo "Stopping Receiver (PID: $RECEIVER_PID)..."
        kill $RECEIVER_PID 2>/dev/null || true
    fi

    if [ ! -z "$MEDIA_DRIVER_PID" ]; then
        echo "Stopping Media Driver (PID: $MEDIA_DRIVER_PID)..."
        kill $MEDIA_DRIVER_PID 2>/dev/null || true
    fi

    echo "System shutdown complete."
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

# Check command line arguments
case ${1:-"full"} in
    "full")
        echo "Starting complete system (Media Driver + Receiver + Sender)..."
        start_media_driver || echo "Using embedded driver mode"
        sleep 1
        start_receiver
        sleep 1
        start_sender
        ;;
    "receiver")
        echo "Starting Receiver only..."
        start_receiver
        ;;
    "sender")
        echo "Starting Sender only..."
        start_sender
        ;;
    "driver")
        echo "Starting Media Driver only..."
        start_media_driver
        ;;
    *)
        echo "Usage: $0 [full|receiver|sender|driver]"
        echo ""
        echo "  full     - Start complete system (default)"
        echo "  receiver - Start receiver only"
        echo "  sender   - Start sender only"
        echo "  driver   - Start media driver only"
        exit 1
        ;;
esac

echo ""
echo "System started successfully!"
echo ""
echo "Live Text System Status:"
echo "========================"
[ ! -z "$MEDIA_DRIVER_PID" ] && echo "Media Driver: Running (PID $MEDIA_DRIVER_PID)"
[ ! -z "$RECEIVER_PID" ] && echo "Receiver:     Running (PID $RECEIVER_PID)"
[ ! -z "$SENDER_PID" ] && echo "Sender:       Running (PID $SENDER_PID)"
echo ""
echo "Controls:"
echo "- Press Ctrl+C to stop all processes"
echo "- Receiver controls: H=health, S=stats, ESC=exit"
echo "- Check console output for connection status"
echo ""
echo "Network Configuration:"
echo "- Primary Feed:   224.0.1.1:9999"
echo "- Secondary Feed: 224.0.1.2:9999"
echo "- Stream ID:      1001"
echo ""
echo "Spout Output: 'LiveText' (Windows only)"
echo ""

# Wait for processes
wait