#!/usr/bin/env bash
# test-apps/M3TestApp/build.sh
# Compila M3TestApp.apk desde fuentes Java.
# Requiere: JDK ≥ 8, Android SDK (ANDROID_SDK o ~/Library/Android/sdk)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ─── Auto-detectar Android SDK ────────────────────────────────────────────────
if [[ -z "${ANDROID_SDK:-}" ]]; then
    ANDROID_SDK="$HOME/Library/Android/sdk"
fi
if [[ ! -d "$ANDROID_SDK" ]]; then
    echo "[ERROR] Android SDK no encontrado en $ANDROID_SDK"
    echo "        Exporta ANDROID_SDK=/ruta/al/sdk"
    exit 1
fi

# Buscar la versión más reciente de build-tools
BUILD_TOOLS_DIR=$(ls -d "$ANDROID_SDK/build-tools"/*/ 2>/dev/null | sort -V | tail -1)
if [[ -z "$BUILD_TOOLS_DIR" ]]; then
    echo "[ERROR] No se encontraron build-tools en $ANDROID_SDK/build-tools/"
    exit 1
fi
BUILD_TOOLS="${BUILD_TOOLS_DIR%/}"

# Buscar android.jar (plataforma más reciente)
ANDROID_JAR=$(ls "$ANDROID_SDK/platforms/"android-*/android.jar 2>/dev/null | sort -V | tail -1)
if [[ -z "$ANDROID_JAR" ]]; then
    echo "[ERROR] android.jar no encontrado en $ANDROID_SDK/platforms/"
    exit 1
fi

AAPT2="$BUILD_TOOLS/aapt2"
D8="$BUILD_TOOLS/d8"
APKSIGNER="$BUILD_TOOLS/apksigner"
ZIPALIGN="$BUILD_TOOLS/zipalign"

echo "[M3] Build tools: $BUILD_TOOLS"
echo "[M3] android.jar: $ANDROID_JAR"

# ─── Directorios de build ─────────────────────────────────────────────────────
BUILD="$SCRIPT_DIR/build"
CLASSES="$BUILD/classes"
DEX="$BUILD/dex"
APK_OUT="$SCRIPT_DIR/M3TestApp.apk"

mkdir -p "$CLASSES" "$DEX"

# ─── 1. Compilar Java ─────────────────────────────────────────────────────────
echo "[M3] 1/5 Compilando Java..."
javac \
    -classpath "$ANDROID_JAR" \
    --release 8 \
    -d "$CLASSES" \
    $(find src -name "*.java")
echo "     OK — $(find "$CLASSES" -name "*.class" | wc -l | tr -d ' ') archivos .class"

# ─── 2. Compilar DEX ──────────────────────────────────────────────────────────
echo "[M3] 2/5 Compilando DEX (d8)..."
"$D8" \
    --classpath "$ANDROID_JAR" \
    --output "$DEX" \
    "$CLASSES/com/aine/testapp/MainActivity.class"
echo "     OK — $(du -sh "$DEX/classes.dex" | cut -f1) classes.dex"

# ─── 3. Empaquetar con aapt2 link ─────────────────────────────────────────────
echo "[M3] 3/5 Empaquetando con aapt2..."
UNSIGNED="$BUILD/M3TestApp-unsigned.apk"
"$AAPT2" link \
    --manifest AndroidManifest.xml \
    -I "$ANDROID_JAR" \
    --min-sdk-version 26 \
    --target-sdk-version 35 \
    --version-code 1 \
    --version-name "1.0" \
    -o "$UNSIGNED"
echo "     OK — APK sin DEX: $(du -sh "$UNSIGNED" | cut -f1)"

# ─── 4. Añadir classes.dex al APK ────────────────────────────────────────────
echo "[M3] 4/5 Añadiendo classes.dex al APK..."
cp "$DEX/classes.dex" "$BUILD/"
(cd "$BUILD" && zip -q "$UNSIGNED" classes.dex)
echo "     OK"

# ─── 5. Alinear y firmar ──────────────────────────────────────────────────────
echo "[M3] 5/5 Alineando y firmando..."
ALIGNED="$BUILD/M3TestApp-aligned.apk"
"$ZIPALIGN" -f -v 4 "$UNSIGNED" "$ALIGNED" > /dev/null

# Usar debug keystore. Si no existe, crearlo.
DEBUG_KS="$HOME/.android/debug.keystore"
if [[ ! -f "$DEBUG_KS" ]]; then
    echo "     Creando debug keystore..."
    keytool -genkeypair \
        -keystore "$DEBUG_KS" \
        -storepass android \
        -alias androiddebugkey \
        -keypass android \
        -keyalg RSA \
        -keysize 2048 \
        -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" 2>/dev/null
fi

"$APKSIGNER" sign \
    --ks "$DEBUG_KS" \
    --ks-pass pass:android \
    --ks-key-alias androiddebugkey \
    --key-pass pass:android \
    --out "$APK_OUT" \
    "$ALIGNED"

echo ""
echo "✅  M3TestApp.apk listo: $APK_OUT ($(du -sh "$APK_OUT" | cut -f1))"
