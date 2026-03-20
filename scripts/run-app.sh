#!/usr/bin/env bash
# =============================================================================
# AINE — run-app.sh
# Ejecuta un APK con AINE.
#
# Uso:
#   ./scripts/run-app.sh <ruta.apk>
#   ./scripts/run-app.sh <ruta.apk> --debug
#   ./scripts/run-app.sh --list                  # listar apps instaladas
#   ./scripts/run-app.sh --install <ruta.apk>    # instalar sin ejecutar
#
# En M0/M1 (etapas iniciales) este script prueba el runtime básico:
#   ./scripts/run-app.sh --test-art              # test ART con HelloWorld.dex
#   ./scripts/run-app.sh --test-binder           # test Binder básico
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
AINE_HOME="$HOME/Library/AINE"
PACKAGES_DIR="$AINE_HOME/packages"
LOGS_DIR="$AINE_HOME/logs"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; RESET='\033[0m'
log()  { echo -e "${BLUE}[run]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()  { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }

mkdir -p "$PACKAGES_DIR" "$LOGS_DIR"

# =============================================================================
# Verificar que AINE está compilado
# =============================================================================
check_build() {
  if [[ ! -f "$BUILD_DIR/src/aine-shim/libaine-shim.dylib" ]]; then
    warn "aine-shim.dylib no encontrado. Compilando..."
    "$SCRIPT_DIR/build.sh" aine-shim
  fi
}

# =============================================================================
# --test-art: probar ART con un DEX mínimo
# =============================================================================
cmd_test_art() {
  log "Test de ART Runtime..."

  # Crear HelloWorld.java minimal
  TMPDIR=$(mktemp -d)
  cat > "$TMPDIR/HelloWorld.java" << 'JAVA_EOF'
public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("AINE: ART Runtime funcional");
        System.out.println("Java version: " + System.getProperty("java.version"));
        System.out.println("Architecture: " + System.getProperty("os.arch"));
    }
}
JAVA_EOF

  # Usar HelloWorld.dex precompilado del repo si existe
  PREBUILT_DEX="$ROOT_DIR/test-apps/HelloWorld/HelloWorld.dex"
  if [[ -f "$PREBUILT_DEX" ]]; then
    log "Usando HelloWorld.dex precompilado: $PREBUILT_DEX"
    cp "$PREBUILT_DEX" "$TMPDIR/classes.dex"
  else
    # Buscar d8 en Android SDK
    D8=""
    if command -v d8 &>/dev/null; then
      D8="d8"
    else
      # Buscar en ubicaciones comunes del Android SDK
      for sdk_path in \
          "$HOME/Library/Android/sdk/build-tools" \
          "$ANDROID_HOME/build-tools" \
          "$ANDROID_SDK_ROOT/build-tools"; do
        D8=$(find "$sdk_path" -name "d8" 2>/dev/null | sort -V | tail -1)
        [[ -n "$D8" ]] && break
      done
    fi
    [[ -z "$D8" ]] && err "d8 no encontrado. Instala Android SDK o usa: brew install android-commandlinetools"

    log "Compilando HelloWorld.java → HelloWorld.dex con $D8..."
    javac "$TMPDIR/HelloWorld.java" -d "$TMPDIR/"
    "$D8" --output "$TMPDIR/HelloWorld.zip" "$TMPDIR/HelloWorld.class"
    unzip -o "$TMPDIR/HelloWorld.zip" classes.dex -d "$TMPDIR/" >/dev/null
  fi

  # Buscar dalvikvm (B6: requiere ART compilado para macOS)
  DALVIKVM=$(find "$BUILD_DIR" -name "dalvikvm" 2>/dev/null | head -1)
  if [[ -z "$DALVIKVM" ]]; then
    warn "dalvikvm no encontrado — BLOQUEANTE B6: necesita ART standalone para macOS"
    warn "Ver: docs/blockers.md#b6-art-standalone-macos"
    warn "HelloWorld.dex está listo en $TMPDIR/classes.dex — esperando dalvikvm"
    err "dalvikvm no disponible. Compila ART para macOS ARM64 primero."
  fi

  log "Ejecutando con ART (modo JIT — workaround page size 16KB activo)..."
  DYLD_INSERT_LIBRARIES="$BUILD_DIR/src/aine-shim/libaine-shim.dylib" \
  AINE_LOG_LEVEL="${AINE_LOG_LEVEL:-info}" \
  "$DALVIKVM" \
    -Xnoimage-dex2oat \
    -Xusejit:false \
    -cp "$TMPDIR/classes.dex" \
    HelloWorld

  ok "Test ART completado"
  rm -rf "$TMPDIR"
}

# =============================================================================
# --test-binder: probar Binder básico
# =============================================================================
cmd_test_binder() {
  log "Test de Binder IPC..."

  BINDER_TEST=$(find "$BUILD_DIR" -name "aine-binder-test" 2>/dev/null | head -1)
  [[ -z "$BINDER_TEST" ]] && err "aine-binder-test no compilado."

  DYLD_INSERT_LIBRARIES="$BUILD_DIR/src/aine-shim/libaine-shim.dylib" \
  "$BINDER_TEST"

  ok "Test Binder completado"
}

# =============================================================================
# --list: listar apps instaladas
# =============================================================================
cmd_list() {
  echo ""
  log "Apps AINE instaladas en $PACKAGES_DIR:"
  echo ""

  if [[ -z "$(ls -A "$PACKAGES_DIR" 2>/dev/null)" ]]; then
    warn "No hay apps instaladas. Instala con: ./scripts/run-app.sh --install <app.apk>"
    return
  fi

  for pkg_dir in "$PACKAGES_DIR"/*/; do
    PKG=$(basename "$pkg_dir")
    MANIFEST="$pkg_dir/AndroidManifest.xml"
    SIZE=$(du -sh "$pkg_dir" 2>/dev/null | cut -f1)
    echo "  $PKG ($SIZE)"
  done
  echo ""
}

# =============================================================================
# --install: instalar APK
# =============================================================================
cmd_install() {
  local APK="$1"
  [[ ! -f "$APK" ]] && err "APK no encontrado: $APK"

  log "Instalando $APK..."

  # Extraer package name del APK
  PKG_NAME=$(unzip -p "$APK" AndroidManifest.xml 2>/dev/null | \
    strings | grep -o 'com\.[a-z][a-z0-9.]*' | head -1 || \
    echo "com.unknown.$(basename "$APK" .apk)")

  INSTALL_DIR="$PACKAGES_DIR/$PKG_NAME"
  mkdir -p "$INSTALL_DIR"

  # Extraer APK
  unzip -q -o "$APK" -d "$INSTALL_DIR/apk/"

  # Extraer libs ARM64
  if [[ -d "$INSTALL_DIR/apk/lib/arm64-v8a" ]]; then
    mkdir -p "$INSTALL_DIR/lib"
    cp "$INSTALL_DIR/apk/lib/arm64-v8a/"*.so "$INSTALL_DIR/lib/" 2>/dev/null || true
    ok "Libs ARM64 extraídas"
  elif [[ -d "$INSTALL_DIR/apk/lib/arm64" ]]; then
    mkdir -p "$INSTALL_DIR/lib"
    cp "$INSTALL_DIR/apk/lib/arm64/"*.so "$INSTALL_DIR/lib/" 2>/dev/null || true
  fi

  # Guardar metadata
  cat > "$INSTALL_DIR/aine.json" << JSON_EOF
{
  "package": "$PKG_NAME",
  "apk": "$(realpath "$APK")",
  "installed": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "aine_version": "0.1.0"
}
JSON_EOF

  ok "Instalado: $PKG_NAME → $INSTALL_DIR"
}

# =============================================================================
# Ejecutar APK
# =============================================================================
cmd_run() {
  local APK="$1"
  local DEBUG=false

  [[ "${2:-}" == "--debug" ]] && DEBUG=true
  [[ ! -f "$APK" ]] && err "APK no encontrado: $APK"

  check_build

  # Instalar si no está instalado
  PKG_NAME=$(basename "$APK" .apk)
  if [[ ! -d "$PACKAGES_DIR/$PKG_NAME" ]]; then
    cmd_install "$APK"
  fi

  INSTALL_DIR="$PACKAGES_DIR/$PKG_NAME"
  LOG_FILE="$LOGS_DIR/${PKG_NAME}-$(date +%Y%m%d-%H%M%S).log"

  log "Lanzando $PKG_NAME..."
  [[ "$DEBUG" == true ]] && warn "Modo debug activo — logs verbosos en $LOG_FILE"

  # Variables de entorno para la app
  AINE_ENV=(
    "DYLD_INSERT_LIBRARIES=$BUILD_DIR/src/aine-shim/libaine-shim.dylib"
    "AINE_PACKAGE_DIR=$INSTALL_DIR"
    "AINE_LOG_FILE=$LOG_FILE"
    "ANDROID_DATA=$AINE_HOME/data"
    "ANDROID_ROOT=$BUILD_DIR"
  )
  [[ "$DEBUG" == true ]] && AINE_ENV+=("AINE_LOG_LEVEL=debug")

  DALVIKVM=$(find "$BUILD_DIR" -name "dalvikvm" 2>/dev/null | head -1)
  [[ -z "$DALVIKVM" ]] && err "dalvikvm no encontrado. Compila AINE primero."

  env "${AINE_ENV[@]}" \
    "$DALVIKVM" \
      -Xnoimage-dex2oat \
      -Xusejit:false \
      -Xbootclasspath:"$BUILD_DIR/lib/core.jar" \
      -cp "$INSTALL_DIR/apk/classes.dex" \
      android.app.ActivityThread \
      2>&1 | tee "$LOG_FILE"
}

# =============================================================================
# Punto de entrada
# =============================================================================
case "${1:-}" in
  --test-art)    cmd_test_art ;;
  --test-binder) cmd_test_binder ;;
  --list)        cmd_list ;;
  --install)     [[ -z "${2:-}" ]] && err "Uso: --install <ruta.apk>"; cmd_install "$2" ;;
  --help|-h)
    echo "Uso: $0 <app.apk> [--debug]"
    echo "     $0 --list"
    echo "     $0 --install <app.apk>"
    echo "     $0 --test-art"
    echo "     $0 --test-binder"
    ;;
  "")
    err "Especifica un APK o usa --help"
    ;;
  *)
    cmd_run "$1" "${2:-}"
    ;;
esac
