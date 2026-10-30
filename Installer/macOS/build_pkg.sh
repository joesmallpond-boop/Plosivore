#!/bin/bash
# Builds the Plosivore macOS installer: a disk image containing "Install Plosivore.pkg"
# and "Uninstall Plosivore.command".
#
#   Installer/macOS/build_pkg.sh <build/Plosivore_artefacts/Release> <output dir>
#
# Run sign_plugins.sh first. Optional environment (all needed together for an installer
# that opens with no security warning; see README):
#   MACOS_DEV_ID_INSTALLER   "Developer ID Installer: Your Name (TEAMID)"
#   MACOS_DEV_ID_APPLICATION "Developer ID Application: Your Name (TEAMID)"  (signs the disk image)
#   APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD   for notarisation
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ART="$(cd "${1:?artefacts dir}" && pwd)"
mkdir -p "${2:?output dir}"; OUT="$(cd "$2" && pwd)"
VERSION="${VERSION:-$(sed -n 's/^project(Plosivore VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")}"
[[ -n "$VERSION" ]] || { echo "Could not read the version from CMakeLists.txt"; exit 1; }

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/pkgs" "$WORK/res" "$WORK/dmg"

# ---------------------------------------------------------------- component packages
# Uploading through GitHub's website drops executable flags; install scripts must keep them.
chmod +x "$HERE"/scripts/*/*
make_component () # <name> <bundle> <install location> <identifier>
{
    local name="$1" bundle="$2" location="$3" id="$4"
    [[ -d "$bundle" ]] || { echo "Missing $bundle - did the build finish?"; exit 1; }
    local root="$WORK/root-$name"
    mkdir -p "$root"
    ditto "$bundle" "$root/$(basename "$bundle")"

    # Never "relocate" to wherever an old copy happens to live: always install to the standard folder.
    local plist="$WORK/$name.plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    local count
    count=$(/usr/libexec/PlistBuddy -c "Print" "$plist" | grep -c "BundleIsRelocatable" || true)
    for ((i = 0; i < count; i++)); do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist"
    done

    pkgbuild --root "$root" \
             --component-plist "$plist" \
             --install-location "$location" \
             --identifier "$id" \
             --version "$VERSION" \
             --scripts "$HERE/scripts/$(echo "$name" | tr '[:upper:]' '[:lower:]')" \
             "$WORK/pkgs/Plosivore-$name.pkg"
}

make_component VST3 "$ART/VST3/Plosivore.vst3"            "/Library/Audio/Plug-Ins/VST3"       com.plosivore.plosivore.vst3
make_component AU   "$ART/AU/Plosivore.component"         "/Library/Audio/Plug-Ins/Components" com.plosivore.plosivore.au
make_component App  "$ART/Standalone/Plosivore.app"       "/Applications"                      com.plosivore.plosivore.app

# ---------------------------------------------------------------- resources
cp "$HERE/resources/"*.html "$WORK/res/"
sed -i '' "s/__VERSION__/$VERSION/g" "$WORK/res/welcome.html"
for bg in background background-dark; do   # one TIFF holding 1x + 2x, so Retina screens stay sharp
    tiffutil -cathidpicheck "$HERE/resources/$bg.png" "$HERE/resources/$bg@2x.png" -out "$WORK/res/$bg.tiff" >/dev/null
done
# Only offer the installer on Mac types the plugin was actually built for.
ARCHS="$(lipo -archs "$ART/VST3/Plosivore.vst3/Contents/MacOS/Plosivore" 2>/dev/null || echo "x86_64 arm64")"
HOST_ARCHS="$(echo "$ARCHS" | tr ' ' ',')"
echo "Plugin architectures: $ARCHS"
sed -e "s/__VERSION__/$VERSION/g" -e "s/hostArchitectures=\"[^\"]*\"/hostArchitectures=\"$HOST_ARCHS\"/" \
    "$HERE/distribution.xml" > "$WORK/distribution.xml"

# ---------------------------------------------------------------- product archive
PKG="$WORK/dmg/Install Plosivore.pkg"
SIGN_ARGS=()
if [[ -n "${MACOS_DEV_ID_INSTALLER:-}" ]]; then SIGN_ARGS=(--sign "$MACOS_DEV_ID_INSTALLER" --timestamp); fi
productbuild --distribution "$WORK/distribution.xml" \
             --resources "$WORK/res" \
             --package-path "$WORK/pkgs" \
             ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} \
             "$PKG"

notarise () # <file>
{
    if [[ -n "${MACOS_DEV_ID_INSTALLER:-}" && -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
        echo "Notarising $(basename "$1")..."
        xcrun notarytool submit "$1" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
                                     --password "$APPLE_APP_PASSWORD" --wait
        xcrun stapler staple "$1"
    fi
}
notarise "$PKG"

# ---------------------------------------------------------------- disk image
cp "$HERE/uninstall.command" "$WORK/dmg/Uninstall Plosivore.command"
chmod +x "$WORK/dmg/Uninstall Plosivore.command"
DMG="$OUT/Plosivore-$VERSION-macOS.dmg"
rm -f "$DMG"
hdiutil create -volname "Plosivore $VERSION" -srcfolder "$WORK/dmg" -fs HFS+ -format UDZO -ov "$DMG" >/dev/null
if [[ -n "${MACOS_DEV_ID_APPLICATION:-}" && -n "${MACOS_DEV_ID_INSTALLER:-}" ]]; then
    codesign --sign "$MACOS_DEV_ID_APPLICATION" --timestamp "$DMG"
    notarise "$DMG"
fi

# Also leave the bare .pkg next to the disk image.
cp "$PKG" "$OUT/Plosivore-$VERSION-macOS.pkg"

echo "Built $DMG"
pkgutil --check-signature "$OUT/Plosivore-$VERSION-macOS.pkg" || true
