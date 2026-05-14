#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/../MilesEdgeworth-release-build"}"

if [[ -z "${QT_PREFIX:-}" ]]; then
  if [[ -d /opt/homebrew/opt/qt ]]; then
    QT_PREFIX=/opt/homebrew/opt/qt
  elif [[ -d /usr/local/opt/qt ]]; then
    QT_PREFIX=/usr/local/opt/qt
  else
    echo "Qt was not found. Set QT_PREFIX to your Qt install path." >&2
    exit 1
  fi
fi

APP="$BUILD_DIR/MilesEdgeworth.app"
ZIP="$BUILD_DIR/MilesEdgeworth-macOS-arm64.zip"

rm -rf "$BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

"$QT_PREFIX/bin/macdeployqt" "$APP" \
  -libpath="$(brew --prefix)/Frameworks" \
  -libpath="$(brew --prefix)/lib" \
  -no-codesign \
  -always-overwrite

rm -rf \
  "$APP/Contents/PlugIns/iconengines" \
  "$APP/Contents/PlugIns/networkinformation" \
  "$APP/Contents/PlugIns/platforminputcontexts" \
  "$APP/Contents/PlugIns/tls"

find "$APP/Contents/PlugIns/imageformats" -type f ! -name 'libqgif.dylib' -delete

rm -rf \
  "$APP/Contents/Frameworks/QtQuick.framework" \
  "$APP/Contents/Frameworks/QtQml.framework" \
  "$APP/Contents/Frameworks/QtQmlMeta.framework" \
  "$APP/Contents/Frameworks/QtQmlModels.framework" \
  "$APP/Contents/Frameworks/QtQmlWorkerScript.framework" \
  "$APP/Contents/Frameworks/QtOpenGL.framework" \
  "$APP/Contents/Frameworks/libjasper.7.dylib" \
  "$APP/Contents/Frameworks/liblcms2.2.dylib" \
  "$APP/Contents/Frameworks/libmng.2.dylib" \
  "$APP/Contents/Frameworks/libjpeg.8.dylib" \
  "$APP/Contents/Frameworks/libtiff.6.dylib" \
  "$APP/Contents/Frameworks/libwebp.7.dylib" \
  "$APP/Contents/Frameworks/libwebpdemux.2.dylib" \
  "$APP/Contents/Frameworks/libwebpmux.3.dylib" \
  "$APP/Contents/Frameworks/libsharpyuv.0.dylib" \
  "$APP/Contents/Frameworks/liblzma.5.dylib"

find "$APP" -name '._*' -delete
xattr -cr "$APP" || true

codesign --force --deep --sign "${CODESIGN_IDENTITY:--}" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

rm -f "$ZIP"
COPYFILE_DISABLE=1 ditto -c -k --norsrc --noextattr --keepParent "$APP" "$ZIP"

du -sh "$APP" "$ZIP"
