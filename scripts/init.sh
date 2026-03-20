#!/usr/bin/env bash
# AINE — init.sh (macOS + Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; BOLD='\033[1m'; RESET='\033[0m'
log()  { echo -e "${BLUE}[AINE]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()  { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }

SKIP_DEPS=false; SKIP_ATL=false
for arg in "$@"; do case $arg in --skip-deps) SKIP_DEPS=true ;; --skip-atl) SKIP_ATL=true ;; esac; done

# Detectar plataforma
OS=$(uname -s); ARCH=$(uname -m)
case "$OS" in
  Darwin)
    PLATFORM="macos"
    [[ "$ARCH" == "arm64" ]] && ok "macOS Apple Silicon — plataforma primaria" || warn "macOS x86_64 — soporte limitado"
    ;;
  Linux)
    PLATFORM="linux"
    [[ "$ARCH" == "aarch64" ]] && ok "Linux aarch64 — ARM64 nativo" || ok "Linux x86_64 — modo ATL-compatible"
    ;;
  *) err "Plataforma no soportada: $OS" ;;
esac

# Dependencias
if [[ "$SKIP_DEPS" == false ]]; then
  [[ "$PLATFORM" == "macos" ]] && "$SCRIPT_DIR/setup-deps.sh" || "$SCRIPT_DIR/setup-deps-linux.sh"
fi

# Verificar herramientas
for tool in cmake ninja git python3; do
  command -v "$tool" &>/dev/null && ok "$tool" || warn "$tool: no encontrado"
done
[[ "$PLATFORM" == "macos" ]] && { xcode-select -p &>/dev/null && ok "Xcode CLT" || err "xcode-select --install"; }

# Submódulos
cd "$ROOT_DIR"
git submodule update --init --recursive && ok "Submódulos inicializados"

# ATL
if [[ "$SKIP_ATL" == false ]]; then
  ATL_DIR="$ROOT_DIR/vendor/atl"
  if [[ ! -d "$ATL_DIR/.git" ]]; then
    log "Clonando ATL..."
    git clone https://gitlab.com/android_translation_layer/android_translation_layer.git "$ATL_DIR" --depth=1
    ok "ATL clonado"
  else
    git -C "$ATL_DIR" fetch origin main --depth=10 && ok "ATL actualizado ($(git -C "$ATL_DIR" rev-parse --short HEAD))"
  fi
  git -C "$ATL_DIR" rev-parse --short HEAD > "$ROOT_DIR/docs/.atl-last-synced" 2>/dev/null || true
fi

# Estructura
mkdir -p "$ROOT_DIR/src/aine-shim"/{common,include/{linux,sys},macos,linux}
mkdir -p "$ROOT_DIR/src/aine-binder"/{common,macos,linux}
mkdir -p "$ROOT_DIR/src/aine-hals"/{macos/{audio,graphics,input,camera},linux/{audio,graphics,input,camera}}
mkdir -p "$ROOT_DIR/tests"/{shared,linux,macos}
mkdir -p "$ROOT_DIR/build"
ok "Estructura creada"

# Git hooks
mkdir -p "$ROOT_DIR/.git/hooks"
cat > "$ROOT_DIR/.git/hooks/pre-commit" << 'HOOK'
#!/bin/bash
ROOT=$(git rev-parse --show-toplevel)
if grep -r "__APPLE__\|__linux__" "$ROOT/src/aine-shim/common/" "$ROOT/src/aine-binder/common/" 2>/dev/null; then
  echo "[AINE] ERROR: código platform-específico en common/ — mueve a macos/ o linux/"; exit 1
fi
exit 0
HOOK
chmod +x "$ROOT_DIR/.git/hooks/pre-commit"
ok "Git hooks configurados"

[[ ! -f "$ROOT_DIR/.env" ]] && printf "AINE_PLATFORM=%s\nAINE_LOG_LEVEL=info\n" "$PLATFORM" > "$ROOT_DIR/.env"

echo ""
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo -e "${GREEN}${BOLD}  AINE — entorno $PLATFORM listo${RESET}"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo "  1. ./scripts/fork-setup.sh   # configurar GitHub"
echo "  2. ./scripts/build.sh        # compilar"
echo "  3. ./scripts/sync-atl.sh --check"
echo ""
