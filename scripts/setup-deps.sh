#!/usr/bin/env bash
# =============================================================================
# AINE — setup-deps.sh
# Instala todas las dependencias del sistema necesarias para AINE.
# Usa Homebrew. Si no está instalado, lo instala.
# =============================================================================
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; RESET='\033[0m'
log()  { echo -e "${BLUE}[deps]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }

# Verificar/instalar Homebrew
if ! command -v brew &>/dev/null; then
  log "Instalando Homebrew..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  eval "$(/opt/homebrew/bin/brew shellenv)"
else
  ok "Homebrew: $(brew --version | head -1)"
fi

# Actualizar Homebrew
log "Actualizando Homebrew..."
brew update --quiet

# =============================================================================
# Dependencias core del build system
# =============================================================================
BREW_PACKAGES=(
  # Build tools
  "cmake"           # Build system
  "ninja"           # Fast build
  "llvm"            # LLVM/Clang explícito (Apple clang puede quedarse corto)
  "ccache"          # Cache de compilación (builds ~5x más rápidos tras el primero)

  # Python (para scripts AOSP y ATL)
  "python@3.11"

  # Control de versiones y submodules
  "git"
  "git-lfs"

  # Dependencias de ART / libcore
  "openjdk@17"      # Para compilar Java del framework Android
  "zlib"
  "libunwind"

  # Para ANGLE (OpenGL ES → Metal)
  # ANGLE se compila desde fuente — depot_tools necesario
  "depot_tools"     # via tap, ver abajo

  # Utilidades de desarrollo
  "ripgrep"         # Búsqueda rápida en código
  "jq"              # Para parsear JSON de build configs
  "xxd"             # Para debug de binarios
)

log "Instalando paquetes Homebrew..."
for pkg in "${BREW_PACKAGES[@]}"; do
  if brew list "$pkg" &>/dev/null 2>&1; then
    ok "$pkg (ya instalado)"
  else
    log "Instalando $pkg..."
    brew install "$pkg" --quiet || warn "Falló instalación de $pkg — continuar..."
  fi
done

# depot_tools para compilar ANGLE
if ! command -v gclient &>/dev/null; then
  log "Instalando depot_tools para ANGLE..."
  if [[ ! -d "$HOME/.depot_tools" ]]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$HOME/.depot_tools"
  fi
  export PATH="$HOME/.depot_tools:$PATH"
  echo 'export PATH="$HOME/.depot_tools:$PATH"' >> ~/.zshrc 2>/dev/null || true
  ok "depot_tools instalado en ~/.depot_tools"
fi

# =============================================================================
# Configurar JAVA_HOME
# =============================================================================
JAVA_HOME_PATH=$(brew --prefix openjdk@17)
if [[ ! -f ~/.zshrc ]] || ! grep -q "JAVA_HOME" ~/.zshrc; then
  echo "export JAVA_HOME=\"$JAVA_HOME_PATH\"" >> ~/.zshrc
  echo 'export PATH="$JAVA_HOME/bin:$PATH"' >> ~/.zshrc
  ok "JAVA_HOME configurado en ~/.zshrc"
fi
export JAVA_HOME="$JAVA_HOME_PATH"

# =============================================================================
# Configurar ccache
# =============================================================================
if command -v ccache &>/dev/null; then
  ccache --max-size=10G
  ok "ccache configurado (10GB cache)"
fi

# =============================================================================
# Verificar herramientas requeridas
# =============================================================================
log "Verificando herramientas..."
MISSING=()
for tool in cmake ninja clang clang++ python3 java git; do
  if command -v "$tool" &>/dev/null; then
    ok "$tool: $(command -v $tool)"
  else
    MISSING+=("$tool")
    warn "$tool: NO ENCONTRADO"
  fi
done

if [[ ${#MISSING[@]} -gt 0 ]]; then
  warn "Herramientas faltantes: ${MISSING[*]}"
  warn "Algunas funcionalidades de AINE pueden no estar disponibles"
else
  ok "Todas las herramientas requeridas están disponibles"
fi

# =============================================================================
# Resumen de versiones
# =============================================================================
echo ""
log "Versiones instaladas:"
echo "  cmake:   $(cmake --version | head -1 | awk '{print $3}')"
echo "  ninja:   $(ninja --version)"
echo "  clang:   $(clang --version | head -1)"
echo "  python3: $(python3 --version)"
echo "  java:    $(java --version 2>&1 | head -1)"
echo "  git:     $(git --version)"
echo ""
ok "Dependencias instaladas. Ejecuta: ./scripts/init.sh"
