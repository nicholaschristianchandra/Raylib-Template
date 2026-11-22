#!/usr/bin/env bash
# Run by Build.bat in WSL; works on a Linux machine too.
#   bash Linux.sh run <build folder> <program name>      copies the build into WSL's files, where it can run, and runs it
#   bash Linux.sh appimage <build folder> <program name> <icon .png> <output file>   packs the build as an AppImage
#   bash Linux.sh raylib <raylib source> [<output folder>] ["<SUPPORT_ flags>"]   builds libraylib.a with zig against glibc 2.31, like the game
set -euo pipefail

if [ "${1:-}" = run ]; then
    # The libraries raylib opens for windows, graphics and sound: library, then its package (newer Ubuntu names first)
    missing=()
    for entry in libX11.so.6:libx11-6 libXext.so.6:libxext6 libXcursor.so.1:libxcursor1 libXrandr.so.2:libxrandr2 \
                 libXi.so.6:libxi6 libXinerama.so.1:libxinerama1 libGL.so.1:libgl1,libgl1-mesa-dri \
                 libasound.so.2:libasound2t64/libasound2 libpulse.so.0:libpulse0; do
        ldconfig -p | awk '{ print $1 }' | grep -Fx "${entry%%:*}" >/dev/null || missing+=("${entry#*:}")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        echo "Installing the libraries raylib opens for windows, graphics and sound (asks for your WSL password)..."
        sudo apt-get update -qq
        packages=()
        for choices in "${missing[@]}"; do
            for package in ${choices//,/ }; do
                for name in ${package//\// }; do
                    if apt-cache show "$name" >/dev/null 2>&1; then packages+=("$name"); break; fi
                done
            done
        done
        sudo apt-get install -y -qq "${packages[@]}"
    fi
    # WSL: software OpenGL, since its GPU OpenGL can leave the window black and frozen
    if grep -qi microsoft /proc/version; then export LIBGL_ALWAYS_SOFTWARE=1; fi
    RUN="$(mktemp -d)"
    trap 'rm -rf "$RUN"' EXIT
    cp -r "$2"/. "$RUN"/ && cd "$RUN" && chmod +x "$3" && ./"$3"
    exit $?
fi
if [ "${1:-}" = appimage ]; then
    # One file that runs on its own: AppImage's runtime, then the build folder as a SquashFS image
    BUILD="$2" NAME="$3" ICON="$4" OUTPUT="$5"
    command -v mksquashfs >/dev/null || {
        echo "Installing squashfs-tools, which packs the AppImage (asks for your WSL password)..."
        sudo apt-get update -qq && sudo apt-get install -y -qq squashfs-tools >/dev/null
    }
    WORK="$(mktemp -d)"
    trap 'rm -rf "$WORK"' EXIT
    RUNTIME="$WORK/runtime-x86_64"
    curl -fsSL -o "$RUNTIME" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 || {
        echo "[AppImage][Error] Could not download AppImage's runtime."; exit 1; }
    APPDIR="$WORK/AppDir"
    ID="$(printf '%s' "$NAME" | tr -c 'A-Za-z0-9._-' '-')"
    mkdir -p "$APPDIR" && chmod 755 "$APPDIR"
    cp -r "$BUILD"/. "$APPDIR"/
    chmod +x "$APPDIR/$NAME"
    ln -s "$NAME" "$APPDIR/AppRun"
    printf '[Desktop Entry]\nType=Application\nName=%s\nExec=AppRun\nIcon=%s\nCategories=Game;\n' "$NAME" "$ID" > "$APPDIR/$ID.desktop"
    if [ -f "$ICON" ]; then cp "$ICON" "$APPDIR/$ID.png"; ln -s "$ID.png" "$APPDIR/.DirIcon"; fi
    mksquashfs "$APPDIR" "$WORK/Image" -root-owned -noappend -quiet >/dev/null
    cat "$RUNTIME" "$WORK/Image" > "$OUTPUT"
    chmod +x "$OUTPUT" 2>/dev/null || true
    exit 0
fi
[ "${1:-}" = raylib ] || { echo "Usage: bash Linux.sh run|appimage|raylib ..."; exit 1; }

GLIBC=2.31
SOURCE="$(cd "${2:?Usage: bash Linux.sh raylib <raylib source folder> [<output folder>]}" && pwd)/src"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT="${3:-$HERE/../../../../Libraries/Raylib/lib/Linux/x64}"
FORMATS=""
for flag in ${4:-}; do FORMATS="$FORMATS -D$flag=1"; done
WORK="$(mktemp -d)"
trap '[ -n "${KEEP_WORK:-}" ] || rm -rf "$WORK"' EXIT
export ZIG_GLOBAL_CACHE_DIR="$WORK/zig-cache"

if command -v zig >/dev/null; then
    ZIG=zig
elif python3 -m ziglang version >/dev/null 2>&1; then
    ZIG="python3 -m ziglang"
else
    echo "[BuildRaylib][Error] zig was not found. Install it: sudo snap install zig --classic --beta, or pip install ziglang, or from ziglang.org."
    exit 1
fi

if command -v apt-get >/dev/null && [ "${SKIP_PACKAGES:-}" != "1" ]; then
    echo "[BuildRaylib] Installing the headers raylib needs (sudo)..."
    sudo apt-get install -y libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev >/dev/null
fi

# zig brings its own glibc 2.31 headers. Only X11's headers are taken from /usr/include, through a link,
# so the system's newer glibc headers never mix in. raylib opens X11, OpenGL and the sound libraries while it runs,
# so its clipboard image, which calls X11 directly, is off.
HEADERS="$WORK/Headers"
mkdir -p "$HEADERS"
ln -s /usr/include/X11 "$HEADERS/X11"

echo "[BuildRaylib] Building libraylib.a for x64 (glibc $GLIBC)..."
for unit in rcore rshapes rtextures rtext rmodels raudio rglfw; do
    $ZIG cc -target x86_64-linux-gnu.$GLIBC -std=gnu99 -Os -fno-sanitize=all -ffunction-sections -fdata-sections \
        -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33 -D_GLFW_X11 -D_GNU_SOURCE -DSUPPORT_CLIPBOARD_IMAGE=0 $FORMATS \
        -I"$SOURCE" -I"$SOURCE/external/glfw/include" -idirafter "$HEADERS" -w \
        -c "$SOURCE/$unit.c" -o "$WORK/$unit.o" &
done
wait
for unit in rcore rshapes rtextures rtext rmodels raudio rglfw; do [ -f "$WORK/$unit.o" ] || { echo "[BuildRaylib][Error] $unit.c failed."; exit 1; }; done
$ZIG ar rcs "$WORK/libraylib.a" "$WORK"/*.o

mkdir -p "$OUTPUT"
cp "$WORK/libraylib.a" "$OUTPUT/libraylib.a"
echo "[BuildRaylib] Done: $OUTPUT/libraylib.a"
