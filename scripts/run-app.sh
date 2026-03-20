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
  log "Test de ART Runtime (nativo — aine-dalvik)..."

  # Usar HelloWorld.dex precompilado del repo
  PREBUILT_DEX="$ROOT_DIR/test-apps/HelloWorld/HelloWorld.dex"
  DEX_PATH=""

  if [[ -f "$PREBUILT_DEX" ]]; then
    log "Usando HelloWorld.dex precompilado: $PREBUILT_DEX"
    DEX_PATH="$PREBUILT_DEX"
  else
    # Compilar HelloWorld.dex si no existe precompilado
    TMPDIR_LOCAL=$(mktemp -d)
    cat > "$TMPDIR_LOCAL/HelloWorld.java" << 'JAVA_EOF'
public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("AINE: ART Runtime funcional");
        System.out.println("java.version: " + System.getProperty("java.version"));
        System.out.println("os.arch: " + System.getProperty("os.arch"));
    }
}
JAVA_EOF
    D8=$(find "${ANDROID_HOME:-$HOME/Library/Android/sdk}/build-tools" -name "d8" 2>/dev/null | sort -V | tail -1)
    [[ -z "$D8" ]] && err "HelloWorld.dex no encontrado y d8 no disponible. Ejecuta: git lfs pull o instala Android SDK."
    javac "$TMPDIR_LOCAL/HelloWorld.java" -d "$TMPDIR_LOCAL/"
    "$D8" --output "$TMPDIR_LOCAL/HelloWorld.zip" "$TMPDIR_LOCAL/HelloWorld.class"
    unzip -o "$TMPDIR_LOCAL/HelloWorld.zip" classes.dex -d "$TMPDIR_LOCAL/" >/dev/null
    DEX_PATH="$TMPDIR_LOCAL/classes.dex"
  fi

  # Buscar dalvikvm nativo (aine-dalvik — Mach-O ARM64, sin emulador)
  DALVIKVM=$(find "$BUILD_DIR" -name "dalvikvm" -type f 2>/dev/null | head -1)
  if [[ -z "$DALVIKVM" ]]; then
    err "dalvikvm no encontrado en $BUILD_DIR. Compila primero: ./scripts/build.sh"
  fi

  log "Ejecutando HelloWorld.dex con aine-dalvik nativo (sin emulador)..."
  DYLD_INSERT_LIBRARIES="$BUILD_DIR/src/aine-shim/libaine-shim.dylib" \
  "$DALVIKVM" -cp "$DEX_PATH" HelloWorld
  local rc=$?

  if [[ $rc -eq 0 ]]; then
    ok "M1 completado — aine-dalvik ejecuta DEX nativo en macOS ARM64 (sin emulador)"
  else
    err "dalvikvm salió con código $rc"
  fi
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
# --m3-build: compilar M3TestApp.apk
# =============================================================================
cmd_m3_build() {
  M3_DIR="$ROOT_DIR/test-apps/M3TestApp"
  [[ ! -d "$M3_DIR" ]] && err "Directorio M3TestApp no encontrado: $M3_DIR"
  log "Compilando M3TestApp.apk..."
  bash "$M3_DIR/build.sh"
  ok "M3TestApp.apk listo"
}

# =============================================================================
# --m3-install <apk>: instalar APK via aine-launcher
# =============================================================================
cmd_m3_install() {
  local APK="${1:-$ROOT_DIR/test-apps/M3TestApp/M3TestApp.apk}"
  [[ ! -f "$APK" ]] && {
    warn "APK no encontrado. Compilando M3TestApp primero..."
    cmd_m3_build
  }

  LAUNCHER=$(find "$BUILD_DIR" -name "aine-launcher" -type f 2>/dev/null | head -1)
  if [[ -z "$LAUNCHER" ]]; then
    warn "aine-launcher no compilado. Compilando..."
    "$SCRIPT_DIR/build.sh" 2>&1 | tail -5
    LAUNCHER=$(find "$BUILD_DIR" -name "aine-launcher" -type f 2>/dev/null | head -1)
    [[ -z "$LAUNCHER" ]] && err "aine-launcher no encontrado tras compilar"
  fi

  log "Instalando $APK via aine-launcher..."
  "$LAUNCHER" install "$APK"
  ok "APK instalado"
}

# =============================================================================
# --m3-launch <apk>: lanzar Activity via aine-launcher
# =============================================================================
cmd_m3_launch() {
  local APK="${1:-$ROOT_DIR/test-apps/M3TestApp/M3TestApp.apk}"
  [[ ! -f "$APK" ]] && err "APK no encontrado: $APK. Usa --m3-build primero."

  LAUNCHER=$(find "$BUILD_DIR" -name "aine-launcher" -type f 2>/dev/null | head -1)
  [[ -z "$LAUNCHER" ]] && err "aine-launcher no compilado. Ejecuta build.sh primero."

  log "Lanzando Activity de $APK..."
  "$LAUNCHER" launch "$APK"
}

# =============================================================================
# --m3 [apk]: ciclo de vida completo M3 — build + install + lifecycle
# =============================================================================
cmd_m3() {
  local APK="${1:-$ROOT_DIR/test-apps/M3TestApp/M3TestApp.apk}"

  # Compilar APK si no existe
  if [[ ! -f "$APK" ]]; then
    warn "M3TestApp.apk no encontrado. Compilando..."
    cmd_m3_build
  fi

  # Compilar aine-launcher si no existe
  LAUNCHER=$(find "$BUILD_DIR" -name "aine-launcher" -type f 2>/dev/null | head -1)
  if [[ -z "$LAUNCHER" ]]; then
    warn "aine-launcher no compilado. Compilando AINE..."
    "$SCRIPT_DIR/build.sh" 2>&1 | tail -5
    LAUNCHER=$(find "$BUILD_DIR" -name "aine-launcher" -type f 2>/dev/null | head -1)
    [[ -z "$LAUNCHER" ]] && err "aine-launcher no encontrado tras compilar"
  fi

  log "=== AINE M3 — Test de ciclo de vida ==="
  log "APK:      $APK"
  log "Launcher: $LAUNCHER"
  echo ""

  "$LAUNCHER" lifecycle "$APK"
  local rc=$?

  if [[ $rc -eq 0 ]]; then
    echo ""
    ok "M3 completado — ciclo de vida Android funcional ✓"
    ok "Criterio M3: 'una app Android completa su ciclo de vida sin crash'"
  else
    echo ""
    err "M3 FALLÓ — el ciclo de vida no se completó correctamente"
  fi
  return $rc
}

# =============================================================================
# Punto de entrada
# =============================================================================
case "${1:-}" in
  --test-art)    cmd_test_art ;;
  --test-binder) cmd_test_binder ;;
  --list)        cmd_list ;;
  --install)     [[ -z "${2:-}" ]] && err "Uso: --install <ruta.apk>"; cmd_install "$2" ;;
  --m3-build)    cmd_m3_build ;;
  --m3-install)  cmd_m3_install "${2:-}" ;;
  --m3-launch)   cmd_m3_launch "${2:-}" ;;
  --m3)          cmd_m3 "${2:-}" ;;
  --help|-h)
    echo "Uso: $0 <app.apk> [--debug]"
    echo "     $0 --list"
    echo "     $0 --install <app.apk>"
    echo "     $0 --test-art"
    echo "     $0 --test-binder"
    echo ""
    echo "M3 — ciclo de vida Android:"
    echo "     $0 --m3-build               # compilar M3TestApp.apk"
    echo "     $0 --m3-install [apk]       # instalar APK en emulador"
    echo "     $0 --m3-launch [apk]        # lanzar Activity"
    echo "     $0 --m3 [apk]               # test completo de ciclo de vida"
    ;;
  "")
    err "Especifica un APK o usa --help"
    ;;
  *)
    cmd_run "$1" "${2:-}"
    ;;
esac
