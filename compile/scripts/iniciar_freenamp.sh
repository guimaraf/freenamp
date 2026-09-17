#!/usr/bin/env bash
# ==========================================================
#  Freenamp - Launcher Portátil Standalone para Linux
# ==========================================================
set -e

# Obter diretório absoluto onde este script está localizado
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Assegurar permissão de execução nos binários
chmod +x "${SCRIPT_DIR}/freenamp" "${SCRIPT_DIR}/bin/yt-dlp" 2>/dev/null || true

# Priorizar a subpasta core/ local para resolução de bibliotecas (libmpv, libSDL2, codecs)
export LD_LIBRARY_PATH="${SCRIPT_DIR}/core:${LD_LIBRARY_PATH:-}"

# Executar o binário nativo passando todos os argumentos (ex: links do YouTube)
exec "${SCRIPT_DIR}/freenamp" "$@"
