#!/bin/sh

# Meson wrapper for Cargo builds

CARGO_TARGET_DIR="$1"
CARGO_MANIFEST_PATH="$2"
BUILD_TYPE="$3"
OUTPUT_DIR="$4"
BIN1="$5"
BIN2="$6"

if [ "$BUILD_TYPE" = "release" ]; then
    CARGO_ARGS="--release"
    TARGET_SUBDIR="release"
else
    CARGO_ARGS=""
    TARGET_SUBDIR="debug"
fi

# Compile using cargo
if [ -n "$OVERRIDE_CARGO_TARGET_DIR" ]; then
    CARGO_TARGET_DIR="$OVERRIDE_CARGO_TARGET_DIR"
fi
export CARGO_TARGET_DIR="$CARGO_TARGET_DIR"
cargo build --manifest-path "$CARGO_MANIFEST_PATH" $CARGO_ARGS

# Ensure the output directory exists
mkdir -p "$OUTPUT_DIR"

# Copy output binaries to target build directory
cp "$CARGO_TARGET_DIR/$TARGET_SUBDIR/$BIN1" "$OUTPUT_DIR/$BIN1"
cp "$CARGO_TARGET_DIR/$TARGET_SUBDIR/$BIN2" "$OUTPUT_DIR/$BIN2"
