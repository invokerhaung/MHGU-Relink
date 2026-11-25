#!/bin/bash
# Prepares deps/src/ (gitignored) for deps/build_deps.sh + deps/build_ffmpeg.sh.
# Extracts the tarballs bundled in third_party/ and downloads FFmpeg (the version the Makefile pins).
# Run from anywhere:  bash deps/setup_src.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
SRC="$HERE/src"
TP="$ROOT/third_party"
FFMPEG_VER=$(grep -E '^FFMPEG_VER[[:space:]]*:=' "$ROOT/Makefile" | sed -E 's/.*:=[[:space:]]*//; s/[[:space:]]*$//')
[ -n "$FFMPEG_VER" ] || { echo "setup_src: cannot read FFMPEG_VER from Makefile"; exit 1; }
mkdir -p "$SRC"

# extract SRC/<tarball> only if the target dir is not already there
extract() { # tarball  targetdir
  local tb="$1" target="$2"
  if [ -d "$SRC/$target" ]; then echo "  [skip] $target exists"; return 0; fi
  [ -f "$tb" ] || { echo "  [miss] $tb"; return 1; }
  echo "  [tar ] $(basename "$tb") -> src/$target"
  tar xf "$tb" -C "$SRC"
}

echo "== bundled tarballs -> deps/src"
extract "$TP/sdl3/SDL3-3.4.18.tar.gz"                 SDL3-3.4.18
extract "$TP/shaderc/shaderc-2026.4.tar.gz"           shaderc-2026.4
extract "$TP/shaderc/glslang-16.6.0.tar.gz"           glslang-16.6.0
extract "$TP/shaderc/SPIRV-Tools-1.4.363.0.tar.gz"    SPIRV-Tools-vulkan-sdk-1.4.363.0
extract "$TP/shaderc/SPIRV-Headers-1.4.363.0.tar.gz"  SPIRV-Headers-vulkan-sdk-1.4.363.0
# opus: build_deps.sh auto-downloads/extracts from third_party/opus, but place it if present
extract "$TP/opus/opus-1.5.2.tar.gz"                  opus-1.5.2

echo "== FFmpeg $FFMPEG_VER -> deps/src/ffmpeg"
if [ -d "$SRC/ffmpeg" ]; then
  echo "  [skip] src/ffmpeg exists"
else
  TAR_XZ="$TP/ffmpeg/ffmpeg-$FFMPEG_VER.tar.xz"
  if [ ! -f "$TAR_XZ" ]; then
    mkdir -p "$(dirname "$TAR_XZ")"
    echo "  [dl  ] https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VER.tar.xz"
    curl -fL --retry 3 -o "$TAR_XZ" "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VER.tar.xz"
  fi
  echo "  [tar ] ffmpeg-$FFMPEG_VER.tar.xz"
  tar xf "$TAR_XZ" -C "$SRC"
  # build_ffmpeg.sh configures from deps/src/ffmpeg
  [ -d "$SRC/ffmpeg" ] || mv "$SRC/ffmpeg-$FFMPEG_VER" "$SRC/ffmpeg"
  [ -d "$SRC/ffmpeg-$FFMPEG_VER" ] && [ ! -e "$SRC/ffmpeg" ] && ln -s "ffmpeg-$FFMPEG_VER" "$SRC/ffmpeg" || true
fi

echo
echo "deps/src is ready:"
ls -1 "$SRC"
