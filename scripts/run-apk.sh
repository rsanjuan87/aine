#!/usr/bin/env bash
# =============================================================================
# AINE — run-apk.sh
# Lanza una APK de Android en AINE (aine-dalvik + sistema de layouts).
#
# Uso:
#   ./scripts/run-apk.sh <ruta/al/app.apk>
#   ./scripts/run-apk.sh <ruta/al/app.apk> --no-window
#   ./scripts/run-apk.sh <ruta/al/app.apk> --res-dir ruta/al/src/main/res
#   ./scripts/run-apk.sh <ruta/al/app.apk> --activity com.paquete.MainActivity
#
# Pasos que realiza automáticamente:
#   1. Verifica que dalvikvm está compilado (cmake --build si no)
#   2. Extrae los archivos DEX del APK
#   3. Obtiene la actividad principal (aapt2 dump badging)
#   4. Auto-detecta el directorio res/ fuente para los XMLs de layout
#   5. Genera aine-res.txt con los mapeos de recursos
#   6. Lanza build/dalvikvm --window -cp classes.dex <MainActivity>
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

# Colores
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

log()  { echo -e "${BLUE}[aine]${RESET} $1"; }
ok()   { echo -e "${GREEN}[OK]${RESET}  $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()  { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }
step() { echo -e "\n${BOLD}${CYAN}▶ $1${RESET}"; }

# =============================================================================
# Argumentos
# =============================================================================
APK=""
WINDOW_FLAG="--window"
RES_DIR_OVERRIDE=""
ACTIVITY_OVERRIDE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-window)  WINDOW_FLAG=""; shift ;;
    --res-dir)    RES_DIR_OVERRIDE="$2"; shift 2 ;;
    --activity)   ACTIVITY_OVERRIDE="$2"; shift 2 ;;
    --help|-h)
      sed -n '3,18p' "$0" | sed 's/^# \?//'
      exit 0
      ;;
    -*)  err "Opción desconocida: $1. Usa --help." ;;
    *)
      [[ -z "$APK" ]] || err "Solo se puede pasar un APK."
      APK="$1"; shift
      ;;
  esac
done

[[ -z "$APK" ]] && { sed -n '3,18p' "$0" | sed 's/^# \?//'; exit 1; }
[[ -f "$APK" ]] || err "APK no encontrado: $APK"

APK="$(cd "$(dirname "$APK")" && pwd)/$(basename "$APK")"

# =============================================================================
# Paso 0: Verificar / compilar dalvikvm
# =============================================================================
step "Paso 0 — Verificar build de AINE"

DALVIKVM=$(find "$BUILD_DIR" -maxdepth 2 -name "dalvikvm" -type f 2>/dev/null | head -1)
if [[ -z "$DALVIKVM" ]]; then
  log "dalvikvm no encontrado. Compilando..."
  cmake --build "$BUILD_DIR" --target dalvikvm
  DALVIKVM=$(find "$BUILD_DIR" -maxdepth 2 -name "dalvikvm" -type f | head -1)
  [[ -n "$DALVIKVM" ]] || err "Falló la compilación. Ejecuta: cmake --build build"
fi
ok "dalvikvm: $DALVIKVM"

# Buscar aapt2
AAPT2=""
for bt in "$HOME/Library/Android/sdk/build-tools"/*/aapt2; do
  [[ -x "$bt" ]] && AAPT2="$bt"
done
[[ -n "$AAPT2" ]] || err "aapt2 no encontrado. Instala Android SDK Build Tools."
ok "aapt2:    $AAPT2"

# =============================================================================
# Paso 1: Extraer DEX del APK
# =============================================================================
step "Paso 1 — Extraer DEX de $(basename "$APK")"

# Directorio de trabajo: <directorio-del-apk>/aine-run/<nombre-sin-ext>/
APK_DIR="$(dirname "$APK")"
APK_STEM="$(basename "$APK" .apk)"
WORK_DIR="$APK_DIR/aine-run/$APK_STEM"
mkdir -p "$WORK_DIR"

# Solo extraer si el APK es más nuevo que los DEX ya extraídos
NEEDS_EXTRACT=true
if [[ -f "$WORK_DIR/classes.dex" && "$WORK_DIR/classes.dex" -nt "$APK" ]]; then
  NEEDS_EXTRACT=false
  log "DEX ya extraídos y actualizados, omitiendo extracción."
fi

if [[ "$NEEDS_EXTRACT" == true ]]; then
  log "Extrayendo DEX de $APK..."
  # Extraer todos los classes*.dex del APK (es un ZIP)
  unzip -o "$APK" "classes*.dex" -d "$WORK_DIR" > /dev/null
  N_DEX=$(ls "$WORK_DIR"/classes*.dex 2>/dev/null | wc -l | tr -d ' ')
  ok "Extraídos $N_DEX archivos DEX → $WORK_DIR"
else
  N_DEX=$(ls "$WORK_DIR"/classes*.dex 2>/dev/null | wc -l | tr -d ' ')
  ok "$N_DEX archivos DEX en $WORK_DIR"
fi

# =============================================================================
# Paso 2: Obtener actividad principal
# =============================================================================
step "Paso 2 — Obtener actividad principal"

if [[ -n "$ACTIVITY_OVERRIDE" ]]; then
  MAIN_ACTIVITY="$ACTIVITY_OVERRIDE"
  log "Usando actividad especificada: $MAIN_ACTIVITY"
else
  MAIN_ACTIVITY=$("$AAPT2" dump badging "$APK" 2>/dev/null \
    | grep '^launchable-activity:' \
    | sed "s/.*name='//;s/'.*//")
  [[ -n "$MAIN_ACTIVITY" ]] || err "No se pudo obtener launchable-activity del APK. Usa --activity."
fi
ok "Actividad principal: $MAIN_ACTIVITY"

PACKAGE=$(echo "$MAIN_ACTIVITY" | sed 's/\.[^.]*$//')
ok "Paquete: $PACKAGE"

# =============================================================================
# Paso 3: Auto-detectar directorio res/ fuente
# =============================================================================
step "Paso 3 — Detectar directorio res/ fuente"

RES_DIR_AUTO=""
if [[ -n "$RES_DIR_OVERRIDE" ]]; then
  RES_DIR_AUTO="$RES_DIR_OVERRIDE"
  log "Usando --res-dir especificado: $RES_DIR_AUTO"
else
  # Heurística: si el APK está en <proj>/app/build/outputs/apk/<variant>/
  # entonces buscar <proj>/app/src/main/res
  CAND="$(cd "$APK_DIR" && cd ../../../../../ 2>/dev/null && pwd)/app/src/main/res"
  [[ -d "$CAND" ]] && RES_DIR_AUTO="$CAND"

  # Alternativa: subir 4 niveles (outputs/apk/<variant>/<apk>)
  if [[ -z "$RES_DIR_AUTO" ]]; then
    CAND="$(cd "$APK_DIR/../../../../" 2>/dev/null && pwd)/src/main/res"
    [[ -d "$CAND" ]] && RES_DIR_AUTO="$CAND"
  fi

  if [[ -n "$RES_DIR_AUTO" ]]; then
    ok "res/ detectado: $RES_DIR_AUTO"
  else
    warn "No se encontró fuente res/. Los XMLs de layout se leerán del APK si están disponibles."
    warn "Para mejores resultados usa: --res-dir <ruta/al/src/main/res>"
  fi
fi

# =============================================================================
# Paso 4: Generar aine-res.txt
# =============================================================================
step "Paso 4 — Generar aine-res.txt"

GEN_SCRIPT="$SCRIPT_DIR/gen-aine-res.py"
[[ -f "$GEN_SCRIPT" ]] || err "gen-aine-res.py no encontrado en $SCRIPT_DIR"

# Regenear si no existe o si el APK es más nuevo
AINE_RES="$WORK_DIR/aine-res.txt"
NEEDS_GEN=true
if [[ -f "$AINE_RES" && "$AINE_RES" -nt "$APK" ]]; then
  NEEDS_GEN=false
  log "aine-res.txt ya actualizado, omitiendo generación."
fi

if [[ "$NEEDS_GEN" == true ]]; then
  GEN_ARGS=("$APK" "--out-dir" "$WORK_DIR")
  [[ -n "$RES_DIR_AUTO" ]] && GEN_ARGS+=("--res-dir" "$RES_DIR_AUTO")
  python3 "$GEN_SCRIPT" "${GEN_ARGS[@]}"
fi

if [[ -f "$AINE_RES" ]]; then
  NIDS=$(grep -c '^id:' "$AINE_RES" 2>/dev/null || echo 0)
  NLAYOUTS=$(grep -c '^layout:' "$AINE_RES" 2>/dev/null || echo 0)
  ok "aine-res.txt: $NLAYOUTS layout(s), $NIDS IDs"
else
  warn "aine-res.txt no generado. La app puede no mostrar la UI."
fi

# =============================================================================
# Paso 5: Lanzar AINE
# =============================================================================
step "Paso 5 — Lanzar en AINE"

echo ""
echo -e "  ${BOLD}App:${RESET}       $(basename "$APK")"
echo -e "  ${BOLD}Actividad:${RESET} $MAIN_ACTIVITY"
echo -e "  ${BOLD}DEX:${RESET}       $WORK_DIR/classes.dex"
echo -e "  ${BOLD}Ventana:${RESET}   ${WINDOW_FLAG:-(headless)}"
echo ""

CMD=("$DALVIKVM")
[[ -n "$WINDOW_FLAG" ]] && CMD+=("$WINDOW_FLAG")
CMD+=("-cp" "$WORK_DIR/classes.dex" "$MAIN_ACTIVITY")

log "Ejecutando: ${CMD[*]}"
echo ""

exec "${CMD[@]}"
