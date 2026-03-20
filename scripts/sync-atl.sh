#!/usr/bin/env bash
# =============================================================================
# AINE — sync-atl.sh
# Sincroniza cambios relevantes desde ATL upstream hacia AINE.
#
# Modos:
#   --check         Ver qué commits nuevos hay en ATL (sin aplicar nada)
#   --fetch         Actualizar vendor/atl/ con los últimos cambios
#   --analyze N     Analizar los últimos N commits de ATL y clasificarlos
#   --cherry N      Guía interactiva para cherry-pick de los últimos N commits
#   --report        Generar reporte en docs/atl-sync-report.md
#
# Flujo recomendado (ejecutar mensualmente o al salir una versión de ATL):
#   1. ./scripts/sync-atl.sh --fetch
#   2. ./scripts/sync-atl.sh --analyze 20
#   3. ./scripts/sync-atl.sh --cherry 20   (interactivo)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ATL_DIR="$ROOT_DIR/vendor/atl"
REPORT_FILE="$ROOT_DIR/docs/atl-sync-report.md"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
log()    { echo -e "${BLUE}[sync-atl]${RESET} $1"; }
ok()     { echo -e "${GREEN}[OK]${RESET} $1"; }
warn()   { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()    { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }
header() { echo -e "\n${CYAN}${BOLD}$1${RESET}"; }

ATL_UPSTREAM="https://gitlab.com/android_translation_layer/android_translation_layer.git"

# =============================================================================
# Clasificación de paths — determina relevancia para AINE
# =============================================================================

# Paths de ATL que son ALTA PRIORIDAD para AINE (siempre sincronizar)
HIGH_PRIORITY_PATTERNS=(
  "^art/"
  "^libcore/"
  "^frameworks/base/"
  "^bionic_translation/"
  "^binder/"
  "^libs/binder/"
)

# Paths que son MEDIA PRIORIDAD (evaluar caso por caso)
MEDIUM_PRIORITY_PATTERNS=(
  "^system/"
  "^services/"
  "^libs/"
  "^CMakeLists"
)

# Paths que NO aplican a AINE (Linux-específicos, ignorar)
IGNORE_PATTERNS=(
  "wayland"
  "x11"
  "mesa"
  "glib"
  "gtk"
  "dbus"
  "linux/"
  "\.service$"
  "flatpak"
  "xdg"
)

classify_commit() {
  local hash="$1"
  local files
  files=$(git -C "$ATL_DIR" diff-tree --no-commit-id -r --name-only "$hash" 2>/dev/null || echo "")

  local has_high=false
  local has_ignore=false

  for pattern in "${IGNORE_PATTERNS[@]}"; do
    if echo "$files" | grep -qE "$pattern"; then
      has_ignore=true
      break
    fi
  done

  for pattern in "${HIGH_PRIORITY_PATTERNS[@]}"; do
    if echo "$files" | grep -qE "$pattern"; then
      has_high=true
      break
    fi
  done

  if [[ "$has_high" == true && "$has_ignore" == false ]]; then
    echo "HIGH"
  elif [[ "$has_ignore" == true && "$has_high" == false ]]; then
    echo "SKIP"
  else
    echo "MEDIUM"
  fi
}

# =============================================================================
# --fetch: actualizar vendor/atl/
# =============================================================================
cmd_fetch() {
  if [[ ! -d "$ATL_DIR/.git" ]]; then
    log "Clonando ATL por primera vez..."
    git clone "$ATL_UPSTREAM" "$ATL_DIR" --depth=50
  else
    log "Actualizando ATL desde upstream..."
    git -C "$ATL_DIR" fetch origin main --depth=50
    git -C "$ATL_DIR" checkout FETCH_HEAD 2>/dev/null || true
  fi

  ATL_HASH=$(git -C "$ATL_DIR" rev-parse HEAD | head -c 8)
  ATL_DATE=$(git -C "$ATL_DIR" log -1 --format="%cd" --date=short)
  ok "ATL actualizado: commit ${ATL_HASH} (${ATL_DATE})"

  # Guardar hash en archivo para tracking
  mkdir -p "$ROOT_DIR/docs"
  echo "$ATL_HASH" > "$ROOT_DIR/docs/.atl-last-synced"
}

# =============================================================================
# --check: mostrar commits nuevos sin aplicar nada
# =============================================================================
cmd_check() {
  [[ ! -d "$ATL_DIR/.git" ]] && err "ATL no encontrado. Ejecuta: ./scripts/sync-atl.sh --fetch"

  LAST_SYNCED=""
  [[ -f "$ROOT_DIR/docs/.atl-last-synced" ]] && LAST_SYNCED=$(cat "$ROOT_DIR/docs/.atl-last-synced")

  header "Estado de ATL upstream"
  echo ""
  ATL_HEAD=$(git -C "$ATL_DIR" rev-parse HEAD | head -c 8)
  ATL_DATE=$(git -C "$ATL_DIR" log -1 --format="%cd" --date=short)
  echo "  ATL HEAD:       ${ATL_HEAD} (${ATL_DATE})"
  echo "  Último sync:    ${LAST_SYNCED:-"nunca"}"
  echo ""

  header "Últimos 10 commits de ATL"
  echo ""
  git -C "$ATL_DIR" log --oneline -10 --format="  %C(yellow)%h%Creset %s %C(dim)(%cr)%Creset"
  echo ""

  if [[ -n "$LAST_SYNCED" ]]; then
    COUNT=$(git -C "$ATL_DIR" rev-list "${LAST_SYNCED}..HEAD" --count 2>/dev/null || echo "?")
    log "Commits desde el último sync: ${COUNT}"
    log "Para analizar: ./scripts/sync-atl.sh --analyze 20"
  fi
}

# =============================================================================
# --analyze N: clasificar commits por relevancia para AINE
# =============================================================================
cmd_analyze() {
  local N="${1:-20}"
  [[ ! -d "$ATL_DIR/.git" ]] && err "ATL no encontrado. Ejecuta: --fetch primero."

  header "Análisis de los últimos ${N} commits de ATL"
  echo ""
  printf "  %-10s %-8s %-50s\n" "HASH" "PRIO" "MENSAJE"
  printf "  %-10s %-8s %-50s\n" "----------" "--------" "--------------------------------------------------"

  local high_count=0 medium_count=0 skip_count=0

  while IFS= read -r line; do
    HASH=$(echo "$line" | cut -d' ' -f1)
    MSG=$(echo "$line" | cut -d' ' -f2-)

    PRIO=$(classify_commit "$HASH")

    case "$PRIO" in
      HIGH)
        printf "  ${GREEN}%-10s %-8s${RESET} %-50s\n" "$HASH" "[HIGH]" "${MSG:0:50}"
        ((high_count++)) || true
        ;;
      MEDIUM)
        printf "  ${YELLOW}%-10s %-8s${RESET} %-50s\n" "$HASH" "[MEDIUM]" "${MSG:0:50}"
        ((medium_count++)) || true
        ;;
      SKIP)
        printf "  ${RED}%-10s %-8s${RESET} %-50s\n" "$HASH" "[SKIP]" "${MSG:0:50}"
        ((skip_count++)) || true
        ;;
    esac
  done < <(git -C "$ATL_DIR" log --oneline -"$N")

  echo ""
  echo "  Resumen: ${GREEN}${high_count} alta prioridad${RESET} | ${YELLOW}${medium_count} media${RESET} | ${RED}${skip_count} ignorar${RESET}"
  echo ""
  log "Para cherry-pick interactivo: ./scripts/sync-atl.sh --cherry ${N}"
}

# =============================================================================
# --cherry N: guía interactiva de cherry-pick
# =============================================================================
cmd_cherry() {
  local N="${1:-10}"
  [[ ! -d "$ATL_DIR/.git" ]] && err "ATL no encontrado."

  header "Cherry-pick interactivo (últimos ${N} commits de ATL)"
  echo ""
  warn "Esto modifica el branch actual. Asegúrate de estar en develop o en un feature branch."
  echo ""

  while IFS= read -r line; do
    HASH=$(echo "$line" | cut -d' ' -f1)
    MSG=$(echo "$line" | cut -d' ' -f2-)
    PRIO=$(classify_commit "$HASH")

    # Mostrar info del commit
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
    echo -e "  Hash: ${YELLOW}${HASH}${RESET}"
    echo -e "  Mensaje: ${BOLD}${MSG}${RESET}"
    echo -e "  Prioridad AINE: $(
      case $PRIO in
        HIGH)   echo -e "${GREEN}ALTA — probablemente aplicar${RESET}" ;;
        MEDIUM) echo -e "${YELLOW}MEDIA — evaluar${RESET}" ;;
        SKIP)   echo -e "${RED}IGNORAR — Linux-específico${RESET}" ;;
      esac
    )"
    echo ""
    echo "  Archivos modificados:"
    git -C "$ATL_DIR" diff-tree --no-commit-id -r --name-only "$HASH" 2>/dev/null | \
      head -10 | sed 's/^/    /'
    echo ""

    # Mostrar el diff resumido
    echo "  [v] ver diff completo | [a] aplicar cherry-pick | [s] saltar | [q] salir"
    read -p "  Acción: " -n 1 choice
    echo ""

    case "$choice" in
      v|V)
        git -C "$ATL_DIR" show "$HASH" | head -80
        echo ""
        read -p "  Ahora: [a] aplicar | [s] saltar: " -n 1 choice2
        echo ""
        ;&  # fallthrough a procesamiento de a/s
      a|A)
        log "Aplicando ${HASH}..."
        # Cherry-pick desde el repo ATL al repo AINE actual
        # Usamos patch para no mezclar el historial directamente
        PATCH_FILE="/tmp/atl-${HASH}.patch"
        git -C "$ATL_DIR" format-patch -1 "$HASH" --stdout > "$PATCH_FILE"

        if git apply --check "$PATCH_FILE" 2>/dev/null; then
          git apply "$PATCH_FILE"
          git add -A
          git commit -m "sync(atl): ${MSG}

Cherry-picked from ATL ${HASH}
Source: ${ATL_UPSTREAM}

AINE-note: revisar si requiere adaptaciones para macOS (aine-shim, bionic-translation)"
          ok "Aplicado: ${HASH}"
        else
          warn "El patch no aplica limpio. Aplicando con --3way..."
          if git apply --3way "$PATCH_FILE" 2>/dev/null; then
            git add -A
            git commit -m "sync(atl): ${MSG} [ADAPTED]

Cherry-picked (con conflictos resueltos) from ATL ${HASH}
Requiere revisión manual de la adaptación."
            ok "Aplicado con adaptaciones: ${HASH}"
          else
            warn "Conflictos al aplicar ${HASH}. El patch está en: ${PATCH_FILE}"
            warn "Resuelve los conflictos manualmente y haz commit."
          fi
        fi
        ;;
      s|S)
        log "Saltando ${HASH}"
        ;;
      q|Q)
        log "Saliendo del cherry-pick interactivo"
        exit 0
        ;;
      *)
        warn "Opción no reconocida, saltando"
        ;;
    esac
  done < <(git -C "$ATL_DIR" log --oneline -"$N")

  ok "Cherry-pick completado"
  log "Recuerda: ./scripts/build.sh para verificar que AINE sigue compilando"
}

# =============================================================================
# --report: generar reporte markdown
# =============================================================================
cmd_report() {
  [[ ! -d "$ATL_DIR/.git" ]] && err "ATL no encontrado."

  local N=30
  local DATE=$(date +"%Y-%m-%d")
  local ATL_HEAD=$(git -C "$ATL_DIR" rev-parse HEAD | head -c 12)

  log "Generando reporte de sincronización ATL..."

  cat > "$REPORT_FILE" << REPORT_EOF
# ATL Sync Report — ${DATE}

ATL HEAD: \`${ATL_HEAD}\`
Fecha: ${DATE}

## Últimos ${N} commits clasificados

| Hash | Prioridad | Mensaje | Archivos |
|------|-----------|---------|----------|
REPORT_EOF

  while IFS= read -r line; do
    HASH=$(echo "$line" | cut -d' ' -f1)
    MSG=$(echo "$line" | cut -d' ' -f2-)
    PRIO=$(classify_commit "$HASH")
    FILES=$(git -C "$ATL_DIR" diff-tree --no-commit-id -r --name-only "$HASH" 2>/dev/null | \
      head -3 | tr '\n' ',' | sed 's/,$//')
    echo "| \`${HASH}\` | ${PRIO} | ${MSG:0:60} | ${FILES} |" >> "$REPORT_FILE"
  done < <(git -C "$ATL_DIR" log --oneline -"$N")

  cat >> "$REPORT_FILE" << 'REPORT_EOF'

## Criterios de clasificación

- **HIGH**: Cambios en art/, libcore/, frameworks/base/, binder/ — aplicar siempre
- **MEDIUM**: Cambios en libs/, system/, services/ — evaluar caso por caso
- **SKIP**: Cambios wayland/x11/mesa/glib — no aplican a macOS

## Instrucciones

1. Revisar commits HIGH primero
2. Para cada commit HIGH: evaluar si requiere adaptación para macOS
3. Aplicar via: `./scripts/sync-atl.sh --cherry N`
4. Tras aplicar: `./scripts/build.sh` para verificar compilación
REPORT_EOF

  ok "Reporte generado: ${REPORT_FILE}"
}

# =============================================================================
# Punto de entrada
# =============================================================================
MODE="${1:-}"
N="${2:-20}"

case "$MODE" in
  --fetch)   cmd_fetch ;;
  --check)   cmd_check ;;
  --analyze) cmd_analyze "$N" ;;
  --cherry)  cmd_cherry "$N" ;;
  --report)  cmd_report ;;
  "")
    echo "Uso: $0 [--check|--fetch|--analyze N|--cherry N|--report]"
    echo ""
    echo "  --check         Ver qué hay de nuevo en ATL (sin cambios)"
    echo "  --fetch         Actualizar vendor/atl/ con los últimos cambios"
    echo "  --analyze N     Clasificar los últimos N commits por relevancia"
    echo "  --cherry N      Cherry-pick interactivo de los últimos N commits"
    echo "  --report        Generar docs/atl-sync-report.md"
    echo ""
    echo "Flujo recomendado (mensual):"
    echo "  ./scripts/sync-atl.sh --fetch"
    echo "  ./scripts/sync-atl.sh --analyze 20"
    echo "  ./scripts/sync-atl.sh --cherry 20"
    ;;
  *)
    err "Modo desconocido: $MODE"
    ;;
esac
