#!/bin/bash

NDK_PATH="" # Your Android NDK PATH
DIR="$(pwd)" #Directory where abx2xml.xpp and xml2abx.cpp are located
OUTPUT_DIR="$DIR/build"

# Create output directory
mkdir -p "$OUTPUT_DIR"
ARCHS=("armv7a-linux-androideabi" "aarch64-linux-android" "i686-linux-android" "x86_64-linux-android")
API=21 

for ARCH in "${ARCHS[@]}"; do
    if [[ "$ARCH" == "armv7a-linux-androideabi" ]]; then
        TARGET="armv7a-linux-androideabi$API"
    else
        TARGET="$ARCH$API"
    fi

    COMPILER="$NDK_PATH/$TARGET-clang++"

    echo "Compiling for $ARCH..."

    # Compile abx2xml
    $COMPILER -Os -static -ffunction-sections -fdata-sections -fvisibility=hidden \
        -flto -Wl,--gc-sections -o "$OUTPUT_DIR/abx2xml-$ARCH" "$DIR/abx2xml.cpp"

    # Compile xml2abx
    $COMPILER -Os -static -ffunction-sections -fdata-sections -fvisibility=hidden \
        -flto -Wl,--gc-sections -o "$OUTPUT_DIR/xml2abx-$ARCH" "$DIR/xml2abx.cpp"
    "$NDK_PATH/llvm-strip" --strip-all "$OUTPUT_DIR/abx2xml-$ARCH"
    "$NDK_PATH/llvm-strip" --strip-all "$OUTPUT_DIR/xml2abx-$ARCH"

    echo "Finished compiling for $ARCH"
done

echo "Binaries are stored in: $OUTPUT_DIR"
