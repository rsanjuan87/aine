#!/usr/bin/env bash
# =============================================================================
# AINE — setup-deps-linux.sh
# Instala dependencias en Linux (Ubuntu/Debian).
# En otras distros, instala los equivalentes manualmente.
# =============================================================================
set -euo pipefail

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; RESET='\033[0m'
log()  { echo -e "${BLUE}[deps/linux]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }

# Detectar package manager
if command -v apt-get &>/dev/null; then
    PKG_MANAGER="apt"
elif command -v dnf &>/dev/null; then
    PKG_MANAGER="dnf"
elif command -v pacman &>/dev/null; then
    PKG_MANAGER="pacman"
else
    warn "Package manager no reconocido — instala las dependencias manualmente"
    PKG_MANAGER="unknown"
fi

log "Package manager: $PKG_MANAGER"

install_apt() {
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        clang \
        clang-format \
        lld \
        ccache \
        openjdk-17-jdk-headless \
        libpipewire-0.3-dev \
        libegl-dev \
        libgles2-mesa-dev \
        libv4l-dev \
        pkg-config \
        git \
        python3 \
        python3-pip \
        zip \
        unzip \
        ripgrep
}

install_dnf() {
    sudo dnf install -y \
        cmake ninja-build clang lld ccache \
        java-17-openjdk-devel \
        pipewire-devel mesa-libEGL-devel \
        libv4l-devel pkg-config git python3 \
        ripgrep
}

install_pacman() {
    sudo pacman -Sy --noconfirm \
        cmake ninja clang lld ccache \
        jdk17-openjdk \
        pipewire mesa \
        v4l-utils pkg-config git python3 \
        ripgrep
}

case "$PKG_MANAGER" in
    apt)    install_apt ;;
    dnf)    install_dnf ;;
    pacman) install_pacman ;;
    *)      warn "Instala manualmente: cmake ninja clang lld ccache openjdk-17 pipewire egl" ;;
esac

# Verificar herramientas
log "Verificando herramientas..."
for tool in cmake ninja clang clang++ java git python3; do
    if command -v "$tool" &>/dev/null; then
        ok "$tool: $(command -v $tool)"
    else
        warn "$tool: no encontrado"
    fi
done

echo ""
ok "Dependencias Linux instaladas"
log "Siguiente: cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-linux.cmake"
