#!/bin/bash
# Removes Plosivore from this Mac. Double-click to run.
echo
echo "This removes the Plosivore plugins and app from this Mac."
echo "Projects that used Plosivore keep their settings if you reinstall later."
echo
read -r -p "Remove Plosivore? (y/n) " answer
[[ "$answer" == [yY]* ]] || { echo "Nothing was removed."; exit 0; }

echo "Your Mac password is needed to remove files from /Library and /Applications."
sudo rm -rf \
    "/Library/Audio/Plug-Ins/VST3/Plosivore.vst3" \
    "/Library/Audio/Plug-Ins/Components/Plosivore.component" \
    "/Applications/Plosivore.app"
rm -rf \
    "$HOME/Library/Audio/Plug-Ins/VST3/Plosivore.vst3" \
    "$HOME/Library/Audio/Plug-Ins/Components/Plosivore.component"
for id in vst3 au app; do
    sudo pkgutil --forget "com.plosivore.plosivore.$id" >/dev/null 2>&1
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo
echo "Plosivore has been removed. You can close this window."
