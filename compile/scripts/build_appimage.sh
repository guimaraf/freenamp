#!/usr/bin/env bash
# Script de empacotamento AppImage para o Freenamp
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
APPDIR="${BUILD_DIR}/AppDir"
OUTPUT_APPIMAGE="${BUILD_DIR}/Freenamp-x86_64.AppImage"

echo "=========================================================="
echo " Freenamp - Empacotamento AppImage Standalone"
echo "=========================================================="

# 1. Certificar que o binário foi compilado
if [ ! -f "${BUILD_DIR}/frontend/freenamp" ] && [ ! -f "${BUILD_DIR}/freenamp" ]; then
    echo "[Freenamp] Compilando projeto antes de empacotar AppImage..."
    cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}" --config Release -j"$(nproc 2>/dev/null || echo 2)"
fi

# 2. Criar estrutura do AppDir
echo "[Freenamp] Montando AppDir em: ${APPDIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/share/freenamp/assets"

# Executável principal
if [ -f "${BUILD_DIR}/frontend/freenamp" ]; then
    cp -f "${BUILD_DIR}/frontend/freenamp" "${APPDIR}/usr/bin/freenamp"
else
    cp -f "${BUILD_DIR}/freenamp" "${APPDIR}/usr/bin/freenamp"
fi
chmod +x "${APPDIR}/usr/bin/freenamp"

# Biblioteca central freenamp_core
if [ -f "${BUILD_DIR}/frontend/core/libfreenamp_core.so" ]; then
    cp -f "${BUILD_DIR}/frontend/core/libfreenamp_core.so" "${APPDIR}/usr/lib/libfreenamp_core.so"
elif [ -f "${BUILD_DIR}/frontend/libfreenamp_core.so" ]; then
    cp -f "${BUILD_DIR}/frontend/libfreenamp_core.so" "${APPDIR}/usr/lib/libfreenamp_core.so"
fi

# Extrator yt-dlp
if [ -f "${ROOT_DIR}/compile/bin/yt-dlp" ]; then
    cp -f "${ROOT_DIR}/compile/bin/yt-dlp" "${APPDIR}/usr/bin/yt-dlp"
    chmod +x "${APPDIR}/usr/bin/yt-dlp"
fi

# Desktop file e ícone
cp -f "${ROOT_DIR}/assets/freenamp.desktop" "${APPDIR}/freenamp.desktop"
if [ -f "${ROOT_DIR}/assets/freenamp.png" ]; then
    cp -f "${ROOT_DIR}/assets/freenamp.png" "${APPDIR}/freenamp.png"
    cp -f "${ROOT_DIR}/assets/freenamp.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/freenamp.png"
elif [ -f "${ROOT_DIR}/assets/frenamp.png" ]; then
    cp -f "${ROOT_DIR}/assets/frenamp.png" "${APPDIR}/freenamp.png"
    cp -f "${ROOT_DIR}/assets/frenamp.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/freenamp.png"
fi

# Assets gerais
cp -rf "${ROOT_DIR}/assets/"* "${APPDIR}/usr/share/freenamp/assets/" 2>/dev/null || true

# Empacotar bibliotecas dinâmicas essenciais (SDL2, MPV) para garantir portabilidade entre distros
echo "[Freenamp] Copiando bibliotecas dinâmicas para AppDir/usr/lib..."
for lib in $(ldd "${APPDIR}/usr/lib/libfreenamp_core.so" | grep -E 'libmpv|libSDL2' | awk '{print $3}'); do
    if [ -f "$lib" ]; then
        cp -f "$lib" "${APPDIR}/usr/lib/" 2>/dev/null || true
    fi
done

# Criar script de inicialização AppRun
cat << 'EOF' > "${APPDIR}/AppRun"
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/freenamp" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# 3. Baixar appimagetool se não estiver disponível
APPIMAGETOOL="${ROOT_DIR}/compile/bin/appimagetool-x86_64.AppImage"
if [ ! -f "${APPIMAGETOOL}" ]; then
    echo "[Freenamp] Baixando appimagetool..."
    curl -L -o "${APPIMAGETOOL}" "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "${APPIMAGETOOL}"
fi

# 4. Gerar o AppImage final
echo "[Freenamp] Gerando arquivo AppImage..."
ARCH=x86_64 "${APPIMAGETOOL}" --appimage-extract-and-run "${APPDIR}" "${OUTPUT_APPIMAGE}"

echo ""
echo "=========================================================="
echo " APPIMAGE GERADO COM SUCESSO!"
echo " Arquivo: ${OUTPUT_APPIMAGE}"
echo "=========================================================="
ls -lh "${OUTPUT_APPIMAGE}"
