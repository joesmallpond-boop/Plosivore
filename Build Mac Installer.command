#!/bin/bash
# Double-click to build Plosivore and its Mac installer on this Mac.
# The finished Plosivore-<version>-macOS.dmg is copied to your Downloads folder.

cd "$(dirname "$0")" || exit 1
PROJECT="$(pwd)"

fail () {
    echo
    echo "Build stopped: $1"
    echo "Copy the text above and send it to Claude if you need help."
    read -r -p "Press Return to close this window."
    exit 1
}
trap 'fail "a step failed (see the error above)."' ERR
set -eE -o pipefail

clear
echo "Building the Plosivore Mac installer. This takes about 5–10 minutes."
echo

# ---- 1. Apple's command line tools (compiler, git, codesign)
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Apple's free command line tools are needed. An install window is opening now."
    xcode-select --install || true
    echo
    echo "When that install finishes, double-click this file again."
    read -r -p "Press Return to close this window."
    exit 0
fi
echo "[1/5] Apple command line tools: found"

# ---- 2. CMake (uses yours if installed, otherwise a private copy inside this folder)
if command -v cmake >/dev/null 2>&1; then
    CMAKE="$(command -v cmake)"
else
    CMAKE="$PROJECT/tools/CMake.app/Contents/bin/cmake"
    if [[ ! -x "$CMAKE" ]]; then
        echo "[2/5] Downloading CMake (build tool, about 80 MB)..."
        mkdir -p tools
        curl -fL --progress-bar -o tools/cmake.tar.gz \
            "https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-macos-universal.tar.gz"
        tar -xzf tools/cmake.tar.gz -C tools
        mv tools/cmake-3.30.5-macos-universal/CMake.app tools/
        rm -rf tools/cmake.tar.gz tools/cmake-3.30.5-macos-universal
    fi
fi
echo "[2/5] CMake: $("$CMAKE" --version | head -1)"

# ---- 3. Configure + build (downloads JUCE the first time)
echo "[3/5] Building the plugin (VST3, Audio Unit, standalone app)..."
"$CMAKE" -S . -B build -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build build --config Release --parallel "$(sysctl -n hw.ncpu)"

# ---- 4. Sign for this Mac
echo "[4/5] Signing..."
bash Installer/macOS/sign_plugins.sh build/Plosivore_artefacts/Release

# ---- 5. Installer
echo "[5/5] Making the installer..."
bash Installer/macOS/build_pkg.sh build/Plosivore_artefacts/Release installer
DMG="$(ls -t installer/*.dmg | head -1)"
cp "$DMG" "$HOME/Downloads/"

trap - ERR
echo
echo "Done. $(basename "$DMG") is in your Downloads folder."
open -R "$HOME/Downloads/$(basename "$DMG")"
read -r -p "Press Return to close this window."
