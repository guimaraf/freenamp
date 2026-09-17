# Freenamp - Especificação Técnica e Documento de Arquitetura

Reprodutor de áudio headless para YouTube, com arquitetura ultraleve (~30 a 50 MB de RAM), modular e totalmente portátil, inspirado na interface clássica do Winamp (Player Principal, Equalizador e Playlist).

---

## 1. Visão Geral e Princípios de Design

1. **Audio-Only (Sem Vídeo)**: O software consome apenas fluxos de áudio (DASH Opus ~160kbps ou AAC ~128kbps) do YouTube. Zero decodificação de vídeo na CPU/GPU e economia drástica de largura de banda.
2. **Portabilidade Absoluta**: Nenhuma dependência é instalada no sistema operacional global. Todas as ferramentas, bibliotecas C++ e utilitários auxiliares residem na pasta `compile/`.
3. **Desacoplamento Rigoroso**: O projeto é dividido de forma estrita entre Interface Visual (`frontend/`), Núcleo de Processamento (`backend/`), Ambiente Portátil (`compile/`) e Artefatos de Compilação (`build/`).
4. **Design Inspirado (Sem Quebra de Copyright)**: Utiliza a consagrada ergonomia de três janelas magnéticas do Winamp (Main, EQ e Playlist), porém com conjunto de assets e identidade próprios (*Freenamp*).

---

## 2. Estrutura de Diretórios

```text
winampTube/
├── freenamp.md               # Este documento de especificação
├── CMakeLists.txt            # Script de compilação central CMake (C++20)
│
├── frontend/                 # Camada de Interface Gráfica e Apresentação
│   ├── include/              # Cabeçalhos da UI (SDL2, Views, Docking)
│   ├── src/                  # Implementações do renderer, sprites e eventos
│   └── assets/               # Spritesheets, ícones e fontes bitmap do Freenamp
│
├── backend/                  # Camada de Áudio, Rede e Lógica de Negócio
│   ├── include/              # Cabeçalhos de áudio, DSP, YouTube e Playlist
│   └── src/                  # Implementação do pipeline de playback e IPC
│
├── compile/                  # Ambiente Isolado e Dependências Portáteis
│   ├── bin/                  # Executáveis auxiliares portáteis (ex: yt-dlp.exe local)
│   ├── venv/                 # Ambiente virtual local (se necessário para scripts)
│   ├── libs/                 # Headers e bibliotecas estáticas/dinâmicas C++ (SDL2, etc.)
│   └── scripts/              # Scripts de suporte e download de dependências locais
│
├── build/                    # Diretório de geração do CMake e binários finais
└── ref/                      # Imagens de referência visual
```

---

## 3. Pilha Tecnológica Adotada

| Componente | Tecnologia | Papel e Justificativa |
| :--- | :--- | :--- |
| **Linguagem Principal** | **C++20** | Performance nativa, controle fino de memória, `std::jthread` e estruturas atômicas lock-free. |
| **Build System** | **CMake 4.x + MinGW-w64** | Compilação nativa padronizada, apontando para bibliotecas locais em `compile/libs/`. |
| **Renderização 2D (Frontend)** | **SDL2 + SDL2_image** (Portátil) | Consumo mínimo de memória (<15 MB de RAM na UI), renderização pixel-art precisa a 60 FPS com zero latência. |
| **Engine de Áudio (Backend)** | **Headless Audio Pipeline (miniaudio / FFmpeg)** | Decodificação e saída de áudio de baixa latência, controle de ganho para equalizador e extração de dados PCM para o Spectrum Analyzer. |
| **Resolução YouTube** | **`yt-dlp` portátil** (`compile/bin/`) | Executável standalone chamado de forma assíncrona para extração de URLs de áudio direto e parsing de playlists. Isola mudanças de segurança do YouTube do código nativo. |

---

## 4. Decisões de Arquitetura

### 4.1. Comunicação Assíncrona Frontend $\leftrightarrow$ Backend
Para evitar qualquer bloqueio na interface do usuário (UI Freeze) durante requisições de rede ou buffering de áudio:
* A thread da interface gráfica executa a 60 FPS cuidando apenas de eventos de entrada (mouse/teclado) e renderização.
* Operações de resolução de links do YouTube rodam em workers assíncronos em background.
* O fluxo de dados do visualizador de espectro (FFT) é alimentado através de um *Ring Buffer* circular atômico, sem necessidade de locks pesados (`mutex`).

### 4.2. Estratégia para Latência Mínima e Playlists Gapless
1. **Primeira Música**: Resolução focada exclusivamente nos formatos de áudio `251` (Opus 160k) ou `140` (AAC 128k), descartando metadados de vídeo desnecessários. Tempo médio de início: ~1.5s a 2.5s.
2. **Músicas Subsequentes (Playlists)**: O `playlist_manager` faz *pre-fetching* da URL da próxima faixa quando a atual atingir os últimos 15 segundos de reprodução. O buffer da próxima faixa é pré-carregado, garantindo **0 segundos de espera entre faixas**.

### 4.3. Ergonomia Visual (Três Janelas Modulares)
* **Janela 1 (Main Player)**: Display digital retro, contador de tempo (decorrido/restante), visualizador de barras de frequência (Spectrum Analyzer), botões de transporte (Play, Pause, Stop, Prev, Next), controle deslizante de volume, pan e barra de progresso (seek).
* **Janela 2 (Equalizador)**: 10 bandas independentes (60Hz, 170Hz, 310Hz, 600Hz, 1kHz, 3kHz, 6kHz, 12kHz, 14kHz, 16kHz) + controle de Preamp e chave liga/desliga.
* **Janela 3 (Playlist Editor)**: Lista expansível de itens, duração individual, tempo total da lista, rolagem suave e suporte a adicionar links diretos de vídeos ou URLs completas de playlists do YouTube.
* **Sistema Magnético (Snapping)**: As janelas se atraem ao se aproximarem (distância de atração de ~10 a 15 pixels) e movem-se juntas quando conectadas.

---

## 5. Roteiro de Implementação em Fases

### Fase 1: Fundação do Projeto e Ambiente Portátil
- Estruturação dos diretórios `frontend/`, `backend/`, `compile/`, `build/`.
- Download e acomodação dos binários e bibliotecas locais em `compile/`.
- Configuração do `CMakeLists.txt` raiz vinculado às dependências locais de `compile/`.

### Fase 2: Backend - Resolução de Links e Playlists YouTube
- Implementação do wrapper de subprocesso para invocar o `yt-dlp` portátil.
- Extração de playlists planas via `--flat-playlist -J`.
- Parser das URLs diretas de áudio (`format 251/140`).
- Sistema de cache local de links para evitar reconsultas na mesma sessão.

### Fase 3: Backend - Motor de Áudio Headless & DSP
- Inicialização do motor de áudio portátil para reprodução de streams HTTPS.
- Implementação dos controles: Play, Pause, Resume, Stop, Seek e Volume.
- Implementação do filtro de equalização de 10 bandas.
- Geração de amostras PCM/FFT em buffer circular para o visualizador.

### Fase 4: Backend - Gerenciador de Playlist & Pre-fetching
- Estruturação da lista de reprodução (fila, shuffle, repeat).
- Sistema de look-ahead assíncrono para pré-resolver a próxima faixa antes do fim da atual.

### Fase 5: Frontend - Engine Gráfica SDL2 e Gerenciador de Skins
- Inicialização do SDL2 em modo portátil.
- Implementação do carregador de spritesheets e texturas recortadas.
- Renderizador de fontes bitmap (marquise de texto e display numérico).

### Fase 6: Frontend - Janela Principal e Visualizador
- Renderização e interatividade da Janela Principal.
- Sliders de Seek, Volume e Balance.
- Renderização do Spectrum Analyzer com barras e queda de picos em tempo real.

### Fase 7: Frontend - Janela de Playlist
- Renderização da tabela de faixas com suporte a scroll e seleção.
- Modal/campo de entrada para inserção de links ou playlists do YouTube.
- Integração da seleção com o backend de reprodução.

### Fase 8: Frontend - Janela de Equalizador
- Renderização dos faders de equalização e preamp.
- Associação direta entre os controles visuais e os ganhos do DSP no backend.

### Fase 9: Sistema de Encaixe Magnético (Docking)
- Algoritmo de atração magnética de bordas (snapping).
- Sincronização de movimento conjunto de janelas ancoradas.

### Fase 10: Integração Final, Testes e Distribuição Portátil
- Testes de consumo de memória (<50 MB) e uso de CPU (<1%).
- Geração do executável portátil final na pasta `build/` pronto para uso direto.
