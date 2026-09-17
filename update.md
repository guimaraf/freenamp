# Freenamp - Registro de Atualizações (update.md)

Este documento registra em detalhes todas as modificações visuais, ergonômicas e arquiteturais implementadas no **Freenamp**.

---

## 1. Quadro Principal (`MainView`) - Limpeza e Reorganização Visual

### Limpeza do Display LCD
- **Remoção do quadro preto vazio**: A área abaixo do tempo decorrido (antigo visualizador de espectro não populado) foi completamente removida da janela principal.
- **Remoção de especificações técnicas do display central**: Os indicadores de `kbps`, `kHz`, `STEREO` e o texto de status foram desacoplados e transferidos para o novo quadro dedicado de Informações (`InfoView`).
- **Layout Minimalista e Autêntico**:
  - Topo: Letreiro deslizante (*marquee*) com o título da música atual a 60 FPS.
  - Lado esquerdo: Dígito LED verde indicador do número da faixa (`01`, `02`, etc.).
  - Centro: Relógio digital LED verde com minutos e segundos (`00:00`).
  - O display LCD agora possui espaçamento limpo e elegante no estilo clássico do Winamp 2.x.

### Correção de Ícones e Botoeira de Transporte
- **Botão Play (`>`)**: A geometria de desenho do triângulo foi corrigida. O vértice agora aponta para a direita (reprodução à frente), eliminando a inversão anterior.
- **Botão Próxima Faixa (`>|`)**: A geometria foi corrigida para desenhar o triângulo voltado para a direita seguido da barra vertical delimitadora, corrigindo a inversão.
- **Botão Faixa Anterior (`|<`)**: Desenha a barra vertical seguida do triângulo voltado para a esquerda.

### Botões SHUFFLE e REPEAT
- **Espaçamento e Dimensões**: Eliminado o posicionamento colado. Os botões agora contam com larguras balanceadas (`SHUF`: 50px, `REP`: 40px, altura 18px uniforme com a barra de transporte) e espaçamento de respiro.
- **Indicadores LED Verdes Estilo Winamp**: Implementado `RetroWidgets::draw_button_with_led`. Cada botão possui uma lâmpada/LED quadrada de 4x4 pixels à esquerda do rótulo:
  - **Acesa em verde brilhante** quando a função está ligada.
  - **Apagada em cinza escuro** quando desligada.
  - O botão `REP` cicla entre `REP` (repetir tudo) e `REP 1` (repetir faixa atual).

---

## 2. Novo Quadro: Informações da Música e Carregamento (`InfoView`)

- **Nova Janela Retrô Autônoma**: Criada a janela `FREENAMP INFO` (`275 x 70` px), totalmente integrada ao sistema de encaixe magnético (`WindowDock`), posicionada por padrão logo abaixo do player principal.
- **Modo de Carregamento / Requisição (`core.is_loading()`)**:
  - **Mensagens dinâmicas de progresso**: Identifica a etapa corrente ("Conectando ao YouTube...", "Extraindo metadados...", "Resolvendo stream Opus/AAC...", "Carregando buffer de áudio...").
  - **Barra de Progresso Retrô**: Barra segmentada com blocos LED verdes/amarelos desenhada em tempo real.
  - **Porcentagem da Requisição**: Exibição numérica destacada em LED verde (`10%`, `35%`, `70%`, `85%`, `100%`).
- **Modo de Reprodução / Repouso**:
  - **Taxa de Bits (Bitrate)**: Exibição em destaque (ex: `160 kbps`).
  - **Frequência de Amostragem**: Exibição em destaque (ex: `48.0 kHz`).
  - **Modo de Canais**: `STEREO`.
  - **Codec de Áudio**: `Opus Audio` / `AAC`.
  - **Estado**: Exibição do status atual (`Tocando`, `Pausado`, `Pronto`).

---

## 3. Quadro de Playlist (`PlaylistView`) - Escalonamento e Rolagem Contínua

### Escalonamento Redimensionável (Resize)
- **Grip de Redimensionamento Retrô**: Adicionadas 3 ranhuras chanfradas diagonais clássicas (`///`) no canto inferior direito da janela de playlist (`RetroWidgets::draw_resize_grip`).
- **Redimensionamento Interativo com o Mouse**: O usuário pode clicar e arrastar o canto inferior direito ou bordas para expandir ou contrair a playlist em largura e altura.
- **Recálculo Proporcional**: Ao redimensionar, a caixa de listagem de músicas, a barra de rolagem vertical e a barra inferior de botões (`+ URL`, `- REM`, `CLEAR`, `^`, `v` e o contador de faixas) recalculam seus limites e pontos de ancoragem dinamicamente.
- **Dimensões Mínimas de Segurança**: Restrição para manter usabilidade mínima (`275 x 140` px).

### Rolagem Contínua por Arrasto (Drag-to-Scroll)
- A barra de rolagem lateral agora responde a clique e arrasto contínuo (`m_dragging_scrollbar`): segurar o botão do mouse sobre a barra e movimentar o cursor verticalmente rola a lista de faixas de maneira contínua e suave em tempo real.

---

## 4. Persistência de Sessão e Buffer / Cache em Disco

- **Diretório Local Portátil `cache/`**: Criado na raiz da aplicação.
- **Persistência da Playlist (`cache/playlist.json`)**:
  - Todas as faixas adicionadas, IDs, títulos, autores, durações, URLs originais e links de streaming são serializados automaticamente em formato JSON.
  - Ao fechar e reabrir o Freenamp, a lista de reprodução é restaurada instantaneamente.
- **Buffer / Cache de Metadados e Streams (`cache/yt_cache.json`)**:
  - As URLs diretas do CDN do YouTube e os metadados já resolvidos ficam cacheados no disco.
  - Músicas já carregadas anteriormente iniciam imediatamente sem a necessidade de reexecutar consultas demoradas via `yt-dlp`.

---

## 5. Validação e Testes

- **Compilação**: Compilado com sucesso via MinGW C++20 em modo Release com o subsistema nativo do Windows (`WIN32`).
- **Testes Automatizados Executados**:
  - `test_audio_playlist.exe`: 4 suítes de teste (Equalizador, DSP, PlaylistManager, AudioEngine) aprovadas com sucesso.
  - `test_url_flow.exe`: Suíte completa de sanitização de URLs, classificação de Mixes e resolução assíncrona aprovada com código de saída 0.
- **Pacote Distribuível**: Atualizado em `build/freenamp_portable/` incluindo a nova estrutura de pastas (`cache/`) e executável atualizado.

---

## 6. Persistência de Volume e Atalho Delete na Playlist

- **Persistência de Volume e Configurações de Áudio (`cache/settings.json`)**:
  - O nível do volume (`0.0` a `100.0`), balanço/pan, estado de shuffle e modo de repetição são gravados em disco ao fechar e a cada alteração.
  - Ao reiniciar a aplicação, o volume anterior é restaurado exatamente no mesmo patamar, evitando que o player volte sempre no volume máximo (100%).
- **Remoção de Faixa via Tecla Delete**:
  - Ao selecionar qualquer item na lista de músicas da Playlist e pressionar a tecla `Delete` do teclado (`SDLK_DELETE`), a faixa é removida da fila e a lista persistida automaticamente.

---

## 7. Despoluição do Quadro Info e Reposicionamento do Contador da Playlist

- **Quadro Info Estritamente Técnico**:
  - Removido qualquer texto de título de música ou mensagem "Faixa Adicionada" durante a reprodução.
  - O quadro exibe estritamente dados técnicos: taxa de bits (`160 kbps`), taxa de amostragem (`48.0 kHz`), canais (`STEREO`), codec (`Opus Audio`) e o estado direto de playback (`Reproduzindo`, `Pausado`, `Parado`).
- **Contador de Faixas e Tempo Abaixo da Playlist**:
  - A informação de contagem de faixas e tempo acumulado foi reposicionada para a barra de ferramentas inferior da janela, completamente fora da caixa preta da listagem, mantendo a grade de faixas 100% limpa.

---

## 8. Persistência de Faixa Ativa, Purga de Cache e Limpeza do Painel Principal

- **Persistência do Índice da Faixa Selecionada**:
  - Ao fechar o software, o número da faixa que estava selecionada/em reprodução é salvo em `cache/settings.json` (`selected_index`).
  - Ao reabrir, a mesma faixa (ex: faixa 5) é restaurada e selecionada na lista, seu título é exibido no letreiro do painel principal, o visor de 7 segmentos exibe seu número correspondente (`05`), a rolagem é ajustada para manter a faixa visível e ela fica pronta para início imediato ao pressionar Play (`X` ou botão de transporte).
- **Limpeza do Quadro Principal e Purga do `yt_cache.json` ao Clicar em CLEAR**:
  - O botão `CLEAR` agora interrompe a reprodução de áudio imediatamente (`core.stop()`), esvazia a playlist, apaga completamente o cache em memória do `YtResolver` e grava `cache/yt_cache.json` zerado em disco.
  - O painel principal é totalmente limpo: o título volta para "Freenamp Ready", o indicador LED de faixa zera para `00` e o cronômetro digital para `00:00`.
- **Remoção de Músicas Individuais do Cache e Sincronização do Painel Principal**:
  - Ao deletar qualquer faixa (seja pelo botão `- REM` ou pela tecla `Delete`), os metadados e streams correspondentes são purgados do `YtResolver` e o arquivo `cache/yt_cache.json` é atualizado em disco, impedindo o acúmulo desnecessário de megabytes.
  - Se a faixa excluída for a que estava em reprodução ou selecionada no painel principal, a reprodução é interrompida e o display do quadro principal é limpo/atualizado instantaneamente (para a próxima faixa disponível ou para "Freenamp Ready" caso a playlist fique vazia).
- **Encerramento Limpo Garantido**:
  - O botão de fechar `[X]` da barra de título do painel principal agora despacha `SDL_QUIT`, garantindo que todo o encerramento do processo passe pelo ciclo padrão de persistência de sessão (`core.save_session()`).

---

## 9. Isolamento de DLLs em `core/`, Eliminação do `.bat`, Modal de URL e Truncamento de Títulos

- **Isolamento de DLLs na Subpasta `core/`**:
  - `SDL2.dll`, `libmpv-2.dll` e a biblioteca compartilhada `libfreenamp_core.dll` foram movidas exclusivamente para o diretório `core/`.
  - A raiz da versão portátil agora fica estritamente limpa contendo apenas o executável `freenamp.exe` e as pastas `core/`, `bin/` e `cache/`.
- **Eliminação Definitiva do Script Batch (`iniciar_freenamp.bat`)**:
  - Criado um inicializador nativo C++ Win32 (`freenamp.exe` launcher) que registra dinamicamente o caminho de busca de DLLs via `SetDllDirectoryW("core")`.
  - O usuário executa `freenamp.exe` diretamente com duplo clique: nenhuma janela de prompt de comando (CMD) é aberta e o processo é identificado como `freenamp.exe` no Gerenciador de Tarefas.
- **Janela Modal de URL (`InputModal`) Aprimorada**:
  - Largura expandida de 360px para 480px com caixa de entrada de 450px.
  - Implementada janela deslizante de texto e máscara de corte por hardware (`SDL_RenderSetClipRect`): links longos do YouTube não vazam da caixa e o cursor piscante no final da URL permanece sempre visível.
- **Truncamento de Nomes Longos na Playlist (`PlaylistView`)**:
  - Os títulos das faixas na lista agora são limitados dinamicamente (~25-30 caracteres) respeitando a distância da coluna de duração.
  - Títulos extensos recebem sufixo `..` (ex: `1. Bohemian Rhapsody -..`), impedindo que o texto se sobreponha à minutagem (`03:45`) e mantendo a lista perfeitamente legível.

---

## 10. Suporte a YouTube Mixes / Rádio e Limitação Segura de 50 Faixas

- **Correção da Sanitização e Classificação de URLs**:
  - Removido o filtro que descartava parâmetros `&list=RD...` (Mix) e `&list=UL...`.
  - O classificador de URLs (`detect_url_type`) agora identifica qualquer endereço contendo `list=` como `UrlType::Playlist`, permitindo que playlists geradas dinamicamente pelo YouTube a partir de uma música semente sejam adicionadas integralmente.
- **Limite Seguro de 50 Faixas no `yt-dlp`**:
  - Inserido o argumento `--playlist-end 50` e proteção de corte no laço de extração do `resolve_playlist`.
  - Garante que mesmo playlists quase infinitas (Mixes com até 1000 faixas) sejam importadas rapidamente em ~2 a 3 segundos com até 50 faixas completas, sem sobrecarregar a memória, o processador ou o cache do Freenamp.

---

## 11. Integração do Ícone de Alta Qualidade (`assets/freenamp.ico`)

- **Embutimento Nativo no Executável (`freenamp.exe`) e DLL Core**:
  - Criado arquivo de recursos Windows (`freenamp.rc.in`) e compilado via GNU `windres`.
  - O ícone `assets/freenamp.ico` (256x256 com canal alfa / transparência) foi incorporado diretamente na seção de recursos PE do executável `freenamp.exe` e da biblioteca `libfreenamp_core.dll`.
  - O Windows Explorer passa a renderizar o ícone personalizado de alta qualidade no arquivo do programa, atalhos e visualizações de ícones grandes/extra grandes.
- **Aplicação Dinâmica na Janela SDL2 e Barra de Tarefas**:
  - No `GuiEngine::init`, a janela do SDL2 recupera o `HWND` nativo via `SDL_GetWindowWMInfo` e aplica o ícone carregado dos recursos (com fallback para o arquivo em disco) nos tamanhos `ICON_BIG` (256x256) e `ICON_SMALL` (32x32/16x16) via mensagem `WM_SETICON`.
  - A barra de tarefas do Windows, o menu Alt+Tab e a barra de título passam a exibir o ícone nativo com transparência preservada.
- **Inclusão no Pacote Portátil**:
  - Atualizado o script `compile/scripts/build_portable_release.ps1` para incluir a pasta `assets/` e o arquivo `freenamp.ico` na distribuição portátil `build/freenamp_portable/`.

---

## 12. Persistência de Playlist e Renovação Automática de Streams de Áudio do YouTube

- **Diagnóstico da Causa Raiz**:
  - As URLs diretas de áudio do CDN do Google/YouTube (`*.googlevideo.com/videoplayback?...`) possuem validade temporária limitada (máximo de 6 horas) através do parâmetro `expire=<unix_timestamp>`.
  - Ao salvar uma playlist com `stream_url` e `is_resolved: true` no arquivo `cache/playlist.json`, as faixas deixavam de tocar no dia seguinte porque o CDN do YouTube retornava erro HTTP 403 Forbidden para os links com token expirado.
- **Detecção Precisa de Expiracão (`YtResolver::is_stream_url_expired`)**:
  - Implementada função estática que analisa o parâmetro `expire=` contido nas URLs da CDN do YouTube e compara com o relógio do sistema (`std::chrono::system_clock`).
  - Adicionada margem de segurança de 120 segundos para evitar expiração durante o carregamento de buffer inicial.
- **Descarte de URLs Expiradas no Cache e Persistência**:
  - `YtResolver`: As funções de leitura e escrita de cache (`resolve_track_info`, `resolve_stream_url`, `save_cache_to_file`, `load_cache_from_file`, `get_cached_stream_url`, `has_cached_stream_url`) descartam automaticamente links expirados, preservando os metadados fixos (título, autor, duração, ID).
  - `PlaylistManager`: Em `save_to_file` e `load_from_file`, faixas com URLs expiradas têm `stream_url` limpa e `is_resolved` redefinido para `false`. Os metadados da playlist permanecem intactos indefinidamente.
  - Em `check_prefetch`, se a próxima faixa tiver URL expirada, o motor dispara automaticamente uma nova resolução em background antes do término da faixa atual.
- **Renovação Sob Demanda Transparente (`CoreController::play_current_playlist_track`)**:
  - Ao iniciar a reprodução de qualquer faixa da playlist salva cuja URL esteja expirada ou não resolvida, o player exibe o status `"Renovando stream de audio..."`, purga o cache obsoleto e obtém um link novo em tempo real via `yt-dlp`.
  - O fluxo é 100% automático e dispensa qualquer ação manual de "refresh" por parte do usuário.
- **Testes Unitários Automatizados**:
  - Criados testes dedicados em `test_yt_resolver.cpp` e `test_audio_playlist.cpp` que validam o descarte de links expirados e a integridade da persistência de metadados.

---

## 13. Persistência de Posição, Tamanho e Visibilidade das Janelas Internas

- **Preservação no Arquivo de Configurações (`cache/settings.json`)**:
  - Implementados os métodos `GuiEngine::save_window_layout()` e `GuiEngine::load_window_layout()`.
  - O arquivo `settings.json` agora armazena de forma hierárquica e unificada tanto as configurações de áudio quanto o estado de layout:
    - **Janela da Aplicação (`app_window`)**: Dimensões `w` e `h` do container principal.
    - **Janela Principal (`windows.main`)**: Posições `x`, `y` e dimensões fixas `w: 275`, `h: 116`.
    - **Janela de Informações (`windows.info`)**: Posições `x`, `y`, dimensões `w: 275`, `h: 70` e estado de exibição `visible`.
    - **Janela de Equalizador (`windows.eq`)**: Posições `x`, `y`, dimensões `w: 275`, `h: 116` e estado de exibição `visible`.
    - **Janela de Playlist (`windows.playlist`)**: Posições `x`, `y`, dimensões dinâmicas redimensionáveis `w`, `h` e estado de exibição `visible`.
- **Salvamento Imediato e Não Destrutivo**:
  - O layout é salvo automaticamente sempre que o usuário solta o mouse após arrastar ou redimensionar (`SDL_MOUSEBUTTONUP`), quando fecha ou reabre qualquer janela interna via botões de título/transporte, e no fechamento da aplicação.
  - Atualizado `CoreController::save_session()` para carregar as chaves preexistentes de `settings.json` antes de gravar, garantindo que o backend de áudio nunca apague os nós de layout da interface gráfica.
- **Validação de Limites (Clamping de Segurança)**:
  - Ao carregar coordenadas salvas, `load_window_layout()` aplica clamping defensivo para assegurar que nenhuma janela apareça fora da área visível do monitor ou do canvas do player.
  - Dimensões mínimas são garantidas (mínimo de 275x140 para a playlist).
- **Janela Externa Redimensionável (`SDL_WINDOW_RESIZABLE`)**:
  - Habilitada a flag `SDL_WINDOW_RESIZABLE` na janela SDL2 principal com tratamento de `SDL_WINDOWEVENT_RESIZED`, permitindo expandir o espaço de trabalho e organizar as janelas lado a lado ou empilhadas sem cortes.
- **Teste Automatizado de Coexistência**:
  - Adicionado teste `test_settings_and_windows_layout_persistence` na suíte `test_audio_playlist.cpp`, validando que tanto os parâmetros de áudio (`volume`, `pan`) quanto os nós de geometria das janelas persistem e coexistem sem sobrescrita mútua.


