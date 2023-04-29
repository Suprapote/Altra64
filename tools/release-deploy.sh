#!/bin/bash
EXEC_DIR=$(pwd)
RELEASE_ARTIFACT="ALTRA64.zip"
docker run --rm -v "$EXEC_DIR:/build" ${REGISTRY}/${IMAGE_NAME}:master make

# make the release dirs

if [ ! -f "$EXEC_DIR/bin/OS64P.v64" ]; then
    echo "$EXEC_DIR/bin/OS64P.v64 doesnt exists, build failed."
    exit 1
fi

mkdir -p "$EXEC_DIR/release/ED64P/CFG" "$EXEC_DIR/release/ED64P/CHEATS" "$EXEC_DIR/release/ED64P/SDSAVE" "$EXEC_DIR/release/ED64P/WALLPAPER" "$EXEC_DIR/release/ED64P/MEMPAKS" "$EXEC_DIR/release/EMULATORS" "$EXEC_DIR/release/ROMS/"
cp "$EXEC_DIR/bin/OS64P.v64" "$EXEC_DIR/release/ED64P/"
cp "$EXEC_DIR/res/ALT64.INI" "$EXEC_DIR/release/ED64P/"
cp "$EXEC_DIR/res/WALLPAPER/bg.bmp" "$EXEC_DIR/release/ED64P/WALLPAPER"
cp "$EXEC_DIR/res/emulators/gb.v64" "$EXEC_DIR/release/EMULATORS"
cp "$EXEC_DIR/res/emulators/neon64bu.rom" "$EXEC_DIR/release/EMULATORS"
cp "$EXEC_DIR/res/emulators/ultraMSX2.z64" "$EXEC_DIR/release/EMULATORS"
cp "$EXEC_DIR/res/emulators/UltraSMS.z64" "$EXEC_DIR/release/EMULATORS"
cd "$EXEC_DIR/release"

zip -9 -r $RELEASE_ARTIFACT .
