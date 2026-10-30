#!/bin/bash
# Code-signs the built plugins and app.
#
#   Installer/macOS/sign_plugins.sh <build/Plosivore_artefacts/Release>
#
# With no certificate this applies an ad-hoc signature, which Apple Silicon requires for
# plugins to load at all. With an Apple Developer ID it signs properly for notarisation:
#   MACOS_CERTS_P12          base64 of a .p12 export holding your Developer ID
#                            Application + Installer certificates
#   MACOS_CERTS_PASSWORD     the .p12's password
#   MACOS_DEV_ID_APPLICATION "Developer ID Application: Your Name (TEAMID)"
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ART="$(cd "${1:?artefacts dir}" && pwd)"
PLUGINS=("$ART/VST3/Plosivore.vst3" "$ART/AU/Plosivore.component")
APP="$ART/Standalone/Plosivore.app"

if [[ -n "${MACOS_CERTS_P12:-}" ]]; then
    : "${MACOS_DEV_ID_APPLICATION:?MACOS_DEV_ID_APPLICATION must be set alongside MACOS_CERTS_P12}"
    KEYCHAIN="${RUNNER_TEMP:-$TMPDIR}/plosivore-signing.keychain-db"
    KC_PASS="$(uuidgen)"
    security create-keychain -p "$KC_PASS" "$KEYCHAIN"
    security set-keychain-settings -lut 21600 "$KEYCHAIN"
    security unlock-keychain -p "$KC_PASS" "$KEYCHAIN"
    CERT="$(mktemp)"; echo "$MACOS_CERTS_P12" | base64 --decode > "$CERT"
    security import "$CERT" -k "$KEYCHAIN" -P "${MACOS_CERTS_PASSWORD:-}" \
        -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign -T /usr/bin/pkgbuild
    rm -f "$CERT"
    security set-key-partition-list -S apple-tool:,apple: -s -k "$KC_PASS" "$KEYCHAIN" >/dev/null
    # shellcheck disable=SC2046
    security list-keychains -d user -s "$KEYCHAIN" $(security list-keychains -d user | tr -d '"')

    for b in "${PLUGINS[@]}"; do
        codesign --force --options runtime --timestamp --sign "$MACOS_DEV_ID_APPLICATION" "$b"
    done
    # The app runs with the hardened runtime, so it must be granted microphone input explicitly.
    codesign --force --options runtime --timestamp --entitlements "$HERE/Standalone.entitlements" \
             --sign "$MACOS_DEV_ID_APPLICATION" "$APP"
    echo "Signed with Developer ID."
else
    for b in "${PLUGINS[@]}"; do codesign --force --sign - "$b"; done
    codesign --force --entitlements "$HERE/Standalone.entitlements" --sign - "$APP"
    echo "Signed ad-hoc (no Developer ID provided)."
fi

for b in "${PLUGINS[@]}" "$APP"; do
    codesign --verify --strict --verbose=1 "$b"
done
