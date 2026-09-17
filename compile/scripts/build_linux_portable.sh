#!/usr/bin/env bash
# Script de automação e empacotamento portátil do Freenamp para Linux
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${BUILD_DIR}/freenamp_portable_linux"
DIST_CORE_DIR="${DIST_DIR}/core"
DIST_BIN_DIR="${DIST_DIR}/bin"
DIST_ASSETS_DIR="${DIST_DIR}/assets"
DIST_CACHE_DIR="${DIST_DIR}/cache"

echo "=========================================================="
echo " Freenamp - Build & Empacotamento Portátil para Linux"
echo "=========================================================="

# 1. Checagem de ferramentas básicas
MISSING_TOOLS=()
for tool in cmake g++ pkg-config curl; do
    if ! command -v "$tool" &>/dev/null; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo "[ERRO] Ferramentas necessárias ausentes: ${MISSING_TOOLS[*]}"
    echo "Instale os pacotes no Ubuntu executando:"
    echo "  sudo apt update && sudo apt install -y build-essential cmake pkg-config libsdl2-dev libmpv-dev curl"
    exit 1
fi

# 2. Restaurar dependências em compile/
bash "${SCRIPT_DIR}/fetch_deps.sh"

# 3. Configurar e compilar com CMake
echo "[Freenamp] Configurando projeto com CMake (Release)..."
cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" -DCMAKE_BUILD_TYPE=Release

NPROC=$(nproc 2>/dev/null || echo 2)
echo "[Freenamp] Compilando com ${NPROC} threads..."
cmake --build "${BUILD_DIR}" --config Release -j"${NPROC}"

# 4. Montar a estrutura da pasta portátil
echo "[Freenamp] Criando pasta de distribuição portátil em: ${DIST_DIR}"
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}" "${DIST_CORE_DIR}" "${DIST_BIN_DIR}" "${DIST_ASSETS_DIR}" "${DIST_CACHE_DIR}"

# Copiar executável ELF principal
if [ -f "${BUILD_DIR}/frontend/freenamp" ]; then
    cp -f "${BUILD_DIR}/frontend/freenamp" "${DIST_DIR}/freenamp"
elif [ -f "${BUILD_DIR}/freenamp" ]; then
    cp -f "${BUILD_DIR}/freenamp" "${DIST_DIR}/freenamp"
else
    echo "[ERRO] Executável freenamp não encontrado no diretório de build!"
    exit 1
fi
chmod +x "${DIST_DIR}/freenamp"

# Copiar libfreenamp_core.so para core/
CORE_SO=""
if [ -f "${BUILD_DIR}/frontend/core/libfreenamp_core.so" ]; then
    CORE_SO="${BUILD_DIR}/frontend/core/libfreenamp_core.so"
elif [ -f "${BUILD_DIR}/frontend/libfreenamp_core.so" ]; then
    CORE_SO="${BUILD_DIR}/frontend/libfreenamp_core.so"
fi

if [ -n "${CORE_SO}" ]; then
    cp -f "${CORE_SO}" "${DIST_CORE_DIR}/libfreenamp_core.so"
else
    echo "[ERRO] libfreenamp_core.so não encontrado no build!"
    exit 1
fi

# Copiar extrator yt-dlp para bin/
if [ -f "${ROOT_DIR}/compile/bin/yt-dlp" ]; then
    cp -f "${ROOT_DIR}/compile/bin/yt-dlp" "${DIST_BIN_DIR}/yt-dlp"
    chmod +x "${DIST_BIN_DIR}/yt-dlp"
else
    echo "[AVISO] yt-dlp não encontrado em compile/bin/yt-dlp. Baixando agora..."
    curl -L -o "${DIST_BIN_DIR}/yt-dlp" "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp"
    chmod +x "${DIST_BIN_DIR}/yt-dlp"
fi

# Copiar assets e arquivos informativos
if [ -d "${ROOT_DIR}/assets" ]; then
    cp -rf "${ROOT_DIR}/assets/"* "${DIST_ASSETS_DIR}/" 2>/dev/null || true
fi
if [ -f "${ROOT_DIR}/README.md" ]; then
    cp -f "${ROOT_DIR}/README.md" "${DIST_DIR}/"
fi
if [ -f "${DIST_ASSETS_DIR}/freenamp.desktop" ]; then
    cp -f "${DIST_ASSETS_DIR}/freenamp.desktop" "${DIST_DIR}/"
fi

# Opcional: copiar runtimes dinâmicos se BUNDLE_LIBS=1 estiver definido
if [ "${BUNDLE_LIBS:-0}" = "1" ]; then
    echo "[Freenamp] Empacotando bibliotecas dinâmicas do sistema em core/..."
    for lib in $(ldd "${DIST_CORE_DIR}/libfreenamp_core.so" | grep -E 'libmpv|libSDL2' | awk '{print $3}'); do
        if [ -f "$lib" ]; then
            cp -f "$lib" "${DIST_CORE_DIR}/"
        fi
    done
fi

echo ""
echo "=========================================================="
echo " PACOTE PORTÁTIL LINUX GERADO COM SUCESSO!"
echo " Localização: ${DIST_DIR}"
echo "=========================================================="
ls -lh "${DIST_DIR}"
ls -lh "${DIST_CORE_DIR}"
ls -lh "${DIST_BIN_DIR}"
echo ""
echo "Para executar o Freenamp:"
echo "  cd ${DIST_DIR}"
echo "  ./freenamp"
