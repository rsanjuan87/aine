#!/usr/bin/env bash
# =============================================================================
# AINE — fork-setup.sh
# Configura AINE como fork de ATL en GitHub con la estructura de branches correcta.
#
# Uso:
#   ./scripts/fork-setup.sh                    # Setup completo interactivo
#   ./scripts/fork-setup.sh --repo user/aine   # Setup con repo ya creado
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; BOLD='\033[1m'; RESET='\033[0m'
log()    { echo -e "${BLUE}[fork]${RESET} $1"; }
ok()     { echo -e "${GREEN}[OK]${RESET} $1"; }
warn()   { echo -e "${YELLOW}[WARN]${RESET} $1"; }
err()    { echo -e "${RED}[ERROR]${RESET} $1"; exit 1; }
header() { echo -e "\n${BOLD}$1${RESET}"; }

ATL_UPSTREAM="https://gitlab.com/android_translation_layer/android_translation_layer.git"
GITHUB_REPO="${1:-}"

# =============================================================================
# 1. Verificar git
# =============================================================================
if ! command -v git &>/dev/null; then
    err "git no encontrado"
fi

cd "$ROOT_DIR"

# =============================================================================
# 2. Configurar remotes
# =============================================================================
header "Configurando remotes de git"

# Remote origin (AINE en GitHub)
if git remote get-url origin &>/dev/null; then
    CURRENT_ORIGIN=$(git remote get-url origin)
    ok "Remote 'origin' ya configurado: $CURRENT_ORIGIN"
else
    if [[ -z "$GITHUB_REPO" ]]; then
        echo ""
        read -p "  URL de tu repo AINE en GitHub (ej: https://github.com/usuario/aine.git): " GITHUB_URL
    else
        GITHUB_URL="https://github.com/$GITHUB_REPO.git"
    fi
    git remote add origin "$GITHUB_URL"
    ok "Remote 'origin' configurado: $GITHUB_URL"
fi

# Remote atl-upstream (ATL en GitLab — solo lectura, referencia)
if git remote get-url atl-upstream &>/dev/null; then
    ok "Remote 'atl-upstream' ya existe"
else
    git remote add atl-upstream "$ATL_UPSTREAM"
    ok "Remote 'atl-upstream' añadido: $ATL_UPSTREAM"
fi

echo ""
log "Remotes configurados:"
git remote -v

# =============================================================================
# 3. Estructura de branches
# =============================================================================
header "Configurando estructura de branches"

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")

# Asegurar que estamos en main
if [[ "$CURRENT_BRANCH" != "main" ]]; then
    if git show-ref --verify --quiet refs/heads/main; then
        git checkout main
    else
        git checkout -b main
    fi
fi
ok "Branch 'main' activo"

# Crear develop si no existe
if ! git show-ref --verify --quiet refs/heads/develop; then
    git checkout -b develop
    git checkout main
    ok "Branch 'develop' creado"
else
    ok "Branch 'develop' ya existe"
fi

# Crear atl-upstream tracking branch
if ! git show-ref --verify --quiet refs/heads/atl-upstream; then
    log "Creando branch 'atl-upstream' para tracking de ATL..."
    # Fetch ATL (solo la rama main, shallow)
    git fetch atl-upstream main --depth=1 2>/dev/null || \
        warn "No se pudo fetch de ATL ahora — hazlo manualmente con sync-atl.sh --fetch"

    git checkout -b atl-upstream
    git checkout main
    ok "Branch 'atl-upstream' creado"
else
    ok "Branch 'atl-upstream' ya existe"
fi

# =============================================================================
# 4. Submódulo ATL
# =============================================================================
header "Configurando submódulo ATL"

if [[ -f "$ROOT_DIR/.gitmodules" ]] && grep -q "atl" "$ROOT_DIR/.gitmodules" 2>/dev/null; then
    ok "Submódulo ATL ya configurado en .gitmodules"
    git submodule update --init vendor/atl 2>/dev/null || \
        warn "No se pudo inicializar el submódulo ahora — ejecuta: git submodule update --init"
else
    log "Añadiendo ATL como submódulo en vendor/atl/..."
    mkdir -p "$ROOT_DIR/vendor"
    git submodule add --depth=1 "$ATL_UPSTREAM" vendor/atl 2>/dev/null || {
        warn "git submodule add falló — puede que vendor/atl/ ya exista"
        warn "Si es un directorio vacío: rm -rf vendor/atl && vuelve a ejecutar"
    }
    ok "Submódulo ATL añadido"
fi

# =============================================================================
# 5. GitHub branch protection (via gh CLI si está disponible)
# =============================================================================
header "Configurando protección de branches (GitHub)"

if command -v gh &>/dev/null; then
    REPO_NAME=$(git remote get-url origin | sed 's/.*github.com[:/]//' | sed 's/\.git$//')

    if [[ -n "$REPO_NAME" ]]; then
        log "Configurando protección en: $REPO_NAME"

        # Proteger main: requiere PR + CI verde
        gh api \
            --method PUT \
            -H "Accept: application/vnd.github+json" \
            "/repos/$REPO_NAME/branches/main/protection" \
            -f required_status_checks='{"strict":true,"contexts":["build (linux)","build (macos-arm64)"]}' \
            -f enforce_admins=false \
            -f required_pull_request_reviews='{"required_approving_review_count":1}' \
            -f restrictions=null \
            2>/dev/null && ok "Branch 'main' protegido" || \
            warn "No se pudo configurar protección (¿permisos insuficientes?)"
    fi
else
    warn "gh CLI no encontrado — configura la protección de branches manualmente en GitHub:"
    echo "    Settings → Branches → Add rule → 'main' → Require PR + status checks"
fi

# =============================================================================
# 6. Labels de GitHub para issues/PRs
# =============================================================================
header "Configurando labels de GitHub"

if command -v gh &>/dev/null && [[ -n "${REPO_NAME:-}" ]]; then
    declare -A LABELS=(
        ["platform:macos"]="0075ca"
        ["platform:linux"]="e4e669"
        ["milestone:M0"]="d93f0b"
        ["milestone:M1"]="e99695"
        ["milestone:M2"]="f9d0c4"
        ["milestone:M3"]="fef2c0"
        ["blocker"]="b60205"
        ["sync:atl"]="0052cc"
        ["shim"]="5319e7"
        ["binder"]="1d76db"
        ["hal"]="0e8a16"
    )

    for label in "${!LABELS[@]}"; do
        gh label create "$label" --color "${LABELS[$label]}" \
            --repo "$REPO_NAME" 2>/dev/null || true
    done
    ok "Labels creados"
else
    warn "Crea los labels manualmente en GitHub → Issues → Labels"
fi

# =============================================================================
# 7. Primer push
# =============================================================================
header "Push inicial"

echo ""
log "¿Hacer push de la estructura inicial a GitHub? (main + develop)"
read -p "  [y/N] " -n 1 choice
echo ""

if [[ "$choice" =~ ^[Yy]$ ]]; then
    git push -u origin main
    git push -u origin develop
    ok "Push completado"
else
    log "Puedes hacer push manualmente cuando estés listo:"
    echo "  git push -u origin main"
    echo "  git push -u origin develop"
fi

# =============================================================================
# Resumen
# =============================================================================
echo ""
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo -e "${GREEN}${BOLD}  AINE fork configurado correctamente${RESET}"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo ""
echo "  Branches:"
echo -e "    ${BLUE}main${RESET}          — estable, protegido"
echo -e "    ${BLUE}develop${RESET}       — integración, CI completo"
echo -e "    ${BLUE}atl-upstream${RESET}  — tracking de ATL"
echo ""
echo "  Remotes:"
echo -e "    ${BLUE}origin${RESET}        — tu repo AINE en GitHub"
echo -e "    ${BLUE}atl-upstream${RESET}  — ATL en GitLab (solo lectura)"
echo ""
echo "  Próximos pasos:"
echo -e "  1. ${BLUE}git checkout develop${RESET}"
echo -e "  2. ${BLUE}git checkout -b feature/m0-toolchain${RESET}"
echo -e "  3. ${BLUE}./scripts/build.sh${RESET} — documentar errores de compilación"
echo ""
