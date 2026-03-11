#!/bin/bash
# Artifact v1.0.0 — Linux Installer Script
# Installs the VST3 plugin and optional standalone build
#
# HOW TO USE:
#   1. Build Artifact on Linux:
#      - Install JUCE dependencies: sudo apt install libasound2-dev libx11-dev
#        libxinerama-dev libxext-dev libfreetype6-dev libwebkit2gtk-4.0-dev
#        libglu1-mesa-dev
#      - Open Projucer, add Linux Makefile exporter, save
#      - cd into the Builds/LinuxMakefile folder
#      - make CONFIG=Release
#   2. Copy this script and the manual PDF to the same folder as the build output
#   3. Run: chmod +x install_linux.sh && sudo ./install_linux.sh

set -e

APP_NAME="Artifact"
APP_VERSION="1.0.0"
PUBLISHER="YourName"

# ── Colour output ──────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
DIM='\033[2m'
NC='\033[0m'

info()    { echo -e "${CYAN}==> $1${NC}"; }
success() { echo -e "${GREEN}==> $1${NC}"; }
warn()    { echo -e "${RED}WARNING: $1${NC}"; }

# ── Locate build output ───────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VST3_SRC="$SCRIPT_DIR/Artifact.vst3"
STANDALONE_SRC="$SCRIPT_DIR/Artifact"
MANUAL_SRC="$SCRIPT_DIR/Artifact_Manual_v1.0.0.pdf"

# ── Install locations ─────────────────────────────────────────────────────────
# VST3: system-wide or per-user
if [ "$(id -u)" = "0" ]; then
    VST3_DEST="/usr/lib/vst3"
    APP_DEST="/usr/local/bin"
    DOC_DEST="/usr/local/share/$PUBLISHER/$APP_NAME"
    DESKTOP_DEST="/usr/share/applications"
    INSTALL_SCOPE="system-wide"
else
    VST3_DEST="$HOME/.vst3"
    APP_DEST="$HOME/.local/bin"
    DOC_DEST="$HOME/.local/share/$PUBLISHER/$APP_NAME"
    DESKTOP_DEST="$HOME/.local/share/applications"
    INSTALL_SCOPE="user ($HOME)"
fi

echo ""
echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     ARTIFACT v$APP_VERSION — INSTALLER     ║${NC}"
echo -e "${CYAN}║     Digital Degradation Plugin       ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""
echo -e "Install scope: ${DIM}$INSTALL_SCOPE${NC}"
echo ""

# ── Check for build output ────────────────────────────────────────────────────
if [ ! -d "$VST3_SRC" ] && [ ! -f "$STANDALONE_SRC" ]; then
    warn "No build output found in $SCRIPT_DIR"
    warn "Expected: Artifact.vst3 or Artifact (standalone)"
    echo ""
    echo "Please build Artifact first:"
    echo "  cd \"Builds/LinuxMakefile\""
    echo "  make CONFIG=Release"
    echo "  cp -r build/Artifact.vst3 $SCRIPT_DIR/"
    echo "  cp build/Artifact $SCRIPT_DIR/"
    exit 1
fi

# ── Install VST3 ──────────────────────────────────────────────────────────────
if [ -d "$VST3_SRC" ]; then
    info "Installing VST3 plugin to $VST3_DEST..."
    mkdir -p "$VST3_DEST"
    rm -rf "$VST3_DEST/Artifact.vst3"
    cp -r "$VST3_SRC" "$VST3_DEST/"
    success "VST3 installed: $VST3_DEST/Artifact.vst3"
else
    warn "Artifact.vst3 not found — skipping VST3 install"
fi

# ── Install Standalone ────────────────────────────────────────────────────────
if [ -f "$STANDALONE_SRC" ]; then
    info "Installing Standalone application to $APP_DEST..."
    mkdir -p "$APP_DEST"
    cp "$STANDALONE_SRC" "$APP_DEST/Artifact"
    chmod +x "$APP_DEST/Artifact"
    success "Standalone installed: $APP_DEST/Artifact"

    # Desktop entry
    info "Creating desktop entry..."
    mkdir -p "$DESKTOP_DEST"
    cat > "$DESKTOP_DEST/artifact.desktop" << DESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=Artifact
Comment=Lossy Digital Degradation Plugin
Exec=$APP_DEST/Artifact
Icon=artifact
Categories=AudioVideo;Audio;
Terminal=false
StartupNotify=false
DESKTOP
    success "Desktop entry created"
else
    warn "Artifact standalone binary not found — skipping"
fi

# ── Install Manual ────────────────────────────────────────────────────────────
if [ -f "$MANUAL_SRC" ]; then
    info "Installing user manual to $DOC_DEST..."
    mkdir -p "$DOC_DEST"
    cp "$MANUAL_SRC" "$DOC_DEST/"
    success "Manual installed: $DOC_DEST/Artifact_Manual_v1.0.0.pdf"
fi

# ── Create uninstall script ───────────────────────────────────────────────────
UNINSTALL_DEST="$DOC_DEST/uninstall.sh"
info "Creating uninstall script at $UNINSTALL_DEST..."
cat > "$UNINSTALL_DEST" << UNINSTALL
#!/bin/bash
# Artifact Uninstaller
echo "Removing Artifact..."
rm -rf "$VST3_DEST/Artifact.vst3"
rm -f  "$APP_DEST/Artifact"
rm -f  "$DESKTOP_DEST/artifact.desktop"
rm -rf "$DOC_DEST"
echo "Artifact has been removed."
UNINSTALL
chmod +x "$UNINSTALL_DEST"

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}╔══════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     Installation Complete!           ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════╝${NC}"
echo ""
echo "Installed locations:"
[ -d "$VST3_DEST/Artifact.vst3" ] && echo "  VST3:       $VST3_DEST/Artifact.vst3"
[ -f "$APP_DEST/Artifact" ]        && echo "  Standalone: $APP_DEST/Artifact"
[ -f "$DOC_DEST/Artifact_Manual_v1.0.0.pdf" ] && echo "  Manual:     $DOC_DEST/Artifact_Manual_v1.0.0.pdf"
echo "  Uninstall:  $UNINSTALL_DEST"
echo ""
echo "Please rescan your plugin folders in your DAW to detect Artifact."
echo ""
