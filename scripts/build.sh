#!/usr/bin/env bash
# AINE build.sh — macOS + Linux
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; RESET='\033[0m'
log()  { echo -e "${BLUE}[build]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()  { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }

TARGET=""; CLEAN=false; VERBOSE=false; BUILD_TYPE="Debug"; FORCE_LINUX=false; DEBUG_SHIM=false
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

for arg in "$@"; do
  case $arg in
    --clean)      CLEAN=true ;;
    --verbose|-v) VERBOSE=true ;;
    --release)    BUILD_TYPE="Release" ;;
    --linux)      FORCE_LINUX=true ;;
    --debug-shim) DEBUG_SHIM=true ;;
    -j*)          JOBS="${arg#-j}" ;;
    aine-*)       TARGET="$arg" ;;
  esac
done

BUILD_DIR="$ROOT_DIR/build"

if [[ "$FORCE_LINUX" == true ]]; then
  PLATFORM="linux"; TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-linux.cmake"
elif [[ "$(uname)" == "Darwin" ]]; then
  PLATFORM="macos"; TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos.cmake"
  [[ "$(uname -m)" != "arm64" ]] && warn "No es Apple Silicon — rendimiento limitado"
else
  PLATFORM="linux"; TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-linux.cmake"
fi

log "Plataforma: ${PLATFORM} | Build: ${BUILD_TYPE} | Cores: ${JOBS}"
[[ "$CLEAN" == true ]] && { rm -rf "$BUILD_DIR"; ok "Clean completado"; }
mkdir -p "$BUILD_DIR"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  log "Configurando CMake (${PLATFORM})..."
  EXTRA_FLAGS=""
  [[ "$DEBUG_SHIM" == true ]] && EXTRA_FLAGS="-DAINE_ENABLE_DEBUG_SHIM=ON"
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "$TOOLCHAIN" $EXTRA_FLAGS 2>&1 | tee "$BUILD_DIR/cmake-configure.log"
fi

ln -sf "$BUILD_DIR/compile_commands.json" "$ROOT_DIR/compile_commands.json" 2>/dev/null || true

START=$(date +%s)
if [[ -n "$TARGET" ]]; then
  cmake --build "$BUILD_DIR" --target "$TARGET" --parallel "$JOBS" 2>&1 | tee "$BUILD_DIR/build-${TARGET}.log"
else
  cmake --build "$BUILD_DIR" --parallel "$JOBS" 2>&1 | tee "$BUILD_DIR/build.log"
fi
EXIT=${PIPESTATUS[0]}
ELAPSED=$(( $(date +%s) - START ))

if [[ $EXIT -eq 0 ]]; then
  ok "Build ${PLATFORM} completado en ${ELAPSED}s"
  find "$BUILD_DIR" \( -name "*.dylib" -o -name "*.so" \) -not -path "*/CMakeFiles/*" -exec echo "  artifact: {}" \; 2>/dev/null || true
else
  LAST_ERR=$(tail -30 "$BUILD_DIR/build.log" 2>/dev/null || echo "")
  echo "$LAST_ERR" | grep -qE "sys/epoll.h|linux/memfd.h" && warn "Hint B5: headers Linux faltantes → src/aine-shim/include/"
  echo "$LAST_ERR" | grep -q "eventfd\|ashmem"            && warn "Hint B6: primitivas Linux en Binder → ver docs/blockers.md"
  echo "$LAST_ERR" | grep -q "vendor/atl.*No such"        && warn "Hint: ATL no inicializado → git submodule update --init vendor/atl"
  err "Build falló — ver: ${BUILD_DIR}/build.log"
fi
