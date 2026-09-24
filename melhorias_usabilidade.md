# Especificação e Plano de Implementação: Melhorias de Usabilidade do Freenamp

Este documento apresenta a análise técnica detalhada e o plano de implementação dividido em 4 etapas ordenadas para aprimorar a usabilidade, fidelidade visual e integração do Freenamp com o sistema operacional e a interface gráfica.

---

## Sumário das Etapas de Solução

1. [Etapa 1: Alternância de Modo de Tempo Normal / Regressivo no Relógio LED](#etapa-1-alternância-de-modo-de-tempo-normal--regressivo-no-relógio-led)
2. [Etapa 2: Identificação no Mixer de Volume do Windows ("Freenamp" em vez de URL)](#etapa-2-identificação-no-mixer-de-volume-do-windows-freenamp-em-vez-de-url)
3. [Etapa 3: Aprimoramento da Playlist (Duplo Clique, Seleção Múltipla e Reordenação por Arrasto)](#etapa-3-aprimoramento-da-playlist-duplo-clique-seleção-múltipla-e-reordenação-por-arrasto)
4. [Etapa 4: Desativação do Monitoramento / Hook do RivaTuner (RTSS)](#etapa-4-desativação-do-monitoramento--hook-do-rivatuner-rtss)

---

## Etapa 1: Alternância de Modo de Tempo Normal / Regressivo no Relógio LED

### 1.1 Contexto e Diagnóstico
No reprodutor clássico Winamp, clicar sobre os dígitos do display de tempo alterna entre:
- **Tempo decorrido (Elapsed)**: Exibe minutos e segundos transcorridos (`02:45`).
- **Tempo restante (Remaining)**: Exibe minutos e segundos faltantes precedidos pelo sinal negativo LED (`-01:15`).

Atualmente no Freenamp:
- O método `MainView::render` calcula apenas `double pos = core.get_position();` e passa `is_negative = false` para a rotina de renderização.
- A função de desenho [`RetroFont::draw_led_clock`](frontend/src/retro_font.cpp) já possui o parâmetro `bool is_negative` totalmente implementado e funcional para desenhar o traço horizontal verde do LED de 7 segmentos.
- Falta apenas o estado de controle, a hitbox de clique na interface e a lógica de cálculo da diferença (`duration - position`).

### 1.2 Plano de Implementação Técnica
1. **Modelagem de Estado em `frontend/include/views/main_view.hpp`**:
   - Adicionar o atributo booleano:
     ```cpp
     bool m_time_remaining_mode = false;
     ```
   - Definir a hitbox de clique do visor do relógio digital:
     ```cpp
     // Posição no painel principal: bx + 42 a bx + 115, by + 36 a by + 54
     Rect m_clock_hitbox = { 42, 36, 75, 18 };
     ```

2. **Interceptação de Clique em `frontend/src/views/main_view.cpp`**:
   - No método `MainView::handle_mouse_down`:
     ```cpp
     Rect clock_r = { bx + m_clock_hitbox.x, by + m_clock_hitbox.y, m_clock_hitbox.w, m_clock_hitbox.h };
     if (clock_r.contains(mx, my)) {
         m_time_remaining_mode = !m_time_remaining_mode;
         return true; // Evento tratado; impede arrasto acidental da janela
     }
     ```

3. **Renderização Condicional do Relógio LED em `MainView::render`**:
   - Se `m_time_remaining_mode == true`:
     ```cpp
     double dur = core.get_duration();
     double rem = std::max(0.0, dur - pos);
     int min = static_cast<int>(rem) / 60;
     int sec = static_cast<int>(rem) % 60;
     RetroFont::draw_led_clock(renderer, min, sec, true, bx + 55, by + 38, Palette::LedGreen);
     ```
   - Se `false`: manter o tempo decorrido com `is_negative = false`.

4. **Persistência de Preferência**:
   - Salvar a flag `time_remaining_mode` no arquivo `cache/settings.json` através do `GuiEngine::save_window_layout()` para restaurar a escolha do usuário entre sessões.

---

## Etapa 2: Identificação no Mixer de Volume do Windows ("Freenamp" em vez de URL)

### 2.1 Contexto e Diagnóstico
No Windows Vista/10/11, o Mixer de Volume exibe as sessões de áudio WASAPI ativas.
- O ícone do Freenamp aparece corretamente porque é derivado dos recursos binários do `.exe` compilado (`freenamp.rc`).
- No entanto, o texto exibido ao lado do controle deslizante de volume é a **URL de streaming crua** (ex: `https://rr1---sn-...googlevideo.com/...`).

**Causa Técnica no `libmpv`**:
- O backend de áudio WASAPI do mpv (`ao_wasapi`) chama a API nativa da Microsoft `IAudioSessionControl::SetDisplayName`.
- Por padrão, o mpv formata esse nome utilizando a propriedade interna `media-title` do arquivo em execução.
- Ao tocar uma URL remota, se a opção de título forçado não estiver travada, o mpv assume a própria URL como título de reprodução para a sessão WASAPI.

### 2.2 Plano de Implementação Técnica
1. **Configuração Explícita no `backend/src/audio_engine.cpp`**:
   - No método `AudioEngine::init_mpv()`, travar o nome da sessão de áudio e o título da mídia para que o WASAPI não herde URLs remotas:
     ```cpp
     mpv_set_option_string(m_mpv, "audio-client-name", "Freenamp");
     mpv_set_option_string(m_mpv, "force-media-title", "Freenamp");
     mpv_set_option_string(m_mpv, "title", "Freenamp");
     ```
2. **Atualização do Título de Mídia no `AudioEngine::load_url`**:
   - Ao executar o comando `loadfile`, garantir que a propriedade `force-media-title` continue definida como `"Freenamp"`, forçando o Windows Mixer a manter a assinatura estática da aplicação.
3. **Consistência de Recursos no `frontend/res/freenamp.rc.in`**:
   - Garantir que as chaves `FileDescription`, `ProductName` e `InternalName` sejam unicamente `"Freenamp"`.

---

## Etapa 3: Aprimoramento da Playlist (Duplo Clique, Seleção Múltipla e Reordenação por Arrasto)

Esta etapa abrange três refinamentos cruciais na experiência de manipulação da playlist:

### 3.1 Refinamento da Captura de Duplo Clique
* **Diagnóstico da Falha**: Atualmente, `PlaylistView` calcula o duplo clique via software medindo o delta de tempo com `elapsed_ms < 500`. Se o usuário clica com uma leve oscilação de coordenadas ou dá 3 cliques sucessivos, a variável temporal se reinicia e perde o gatilho.
* **Solução Técnica**:
  1. No `GuiEngine::process_events`: repassar o campo nativo `event.button.clicks` da estrutura `SDL_MouseButtonEvent` do SDL2 para o método de clique da playlist.
  2. O SDL2 já sincroniza com os limiares temporais e de tolerância de pixels oficiais do sistema operacional (`GetDoubleClickTime` no Windows).
  3. No `PlaylistView::handle_mouse_down`:
     ```cpp
     if (clicks >= 2) {
         m_selected_index = clicked_track;
         core.play_track_index(clicked_track);
     }
     ```
  4. Mantém-se o fallback por software com limiar ajustado (600 ms) para garantir resposta imediata em qualquer plataforma.

### 3.2 Seleção Múltipla de Músicas (Shift e Ctrl)
* **Objetivo**:
  - Permitir selecionar múltiplos itens (via clique contíguo com `Shift` ou alternado com `Ctrl`).
  - Ao apertar **Play** (tecla X ou botão Play): apenas a primeira música selecionada (ou a faixa clicada) começa a tocar.
  - Ao apertar **Delete** ou clicar no botão `- REM`: todas as faixas selecionadas são excluídas simultaneamente.
* **Solução Técnica no Backend (`backend/include/playlist_manager.hpp` e `.cpp`)**:
  1. Implementar método para remoção em lote:
     ```cpp
     bool remove_tracks(const std::vector<size_t>& indices);
     ```
     - A remoção deve ordenar os índices de forma decrescente para não invalidar posições subsequentes no vetor de faixas.
     - O índice atual em reprodução (`m_current_index`) deve ser reajustado atômica e corretamente.
* **Solução Técnica no Frontend (`frontend/include/views/playlist_view.hpp` e `.cpp`)**:
  1. Substituir o índice único por um conjunto ordenado:
     ```cpp
     std::set<int> m_selected_indices;
     int m_selection_anchor = -1;
     ```
  2. No tratamento de clique do mouse na lista:
     - **Sem modificador**: Limpa a seleção anterior, seleciona a faixa clicada e define `m_selection_anchor = clicked`.
     - **Com `Shift`** (`SDL_GetModState() & KMOD_SHIFT`): Seleciona todas as faixas no intervalo `[min(anchor, clicked), max(anchor, clicked)]`.
     - **Com `Ctrl`** (`SDL_GetModState() & KMOD_CTRL`): Alterna a presença da faixa clicada no conjunto `m_selected_indices`.
  3. No método `PlaylistView::render`:
     - Renderizar o fundo `Palette::SelectionBg` para todos os itens presentes em `m_selected_indices`.
  4. Na remoção (`remove_selected`):
     - Coletar todos os índices de `m_selected_indices` e invocar `core.get_playlist().remove_tracks(indices)`.

### 3.3 Reordenação de Músicas por Arrasto (Drag & Drop na Lista)
* **Objetivo**: Clicar sobre a faixa (ou bloco de faixas selecionadas), segurar o botão do mouse e arrastar para cima ou para baixo, soltando na posição desejada.
* **Solução Técnica**:
  1. **Estados de Arraste**:
     ```cpp
     bool m_is_dragging_items = false;
     int m_drag_target_index = -1;
     int m_drag_start_y = 0;
     ```
  2. **Detecção do Início do Arraste (`handle_mouse_down`)**:
     - Se o clique ocorrer sobre uma faixa pertencente a `m_selected_indices`, registrar `m_drag_start_y = my`.
  3. **Rastreamento de Movimento (`handle_mouse_move`)**:
     - Se a distância vertical ultrapassar 5 pixels (`abs(my - m_drag_start_y) > 5`), ativar `m_is_dragging_items = true`.
     - Calcular a linha de destino `m_drag_target_index = m_scroll_offset + (my - list_r.y) / line_h`.
  4. **Feedback Visual de Inserção (`render`)**:
     - Desenhar uma linha horizontal destacada (cor `Palette::LedGreen`) entre os itens da lista, indicando com precisão onde as músicas serão posicionadas ao soltar o mouse.
  5. **Finalização do Arraste (`handle_mouse_up`)**:
     - Se `m_is_dragging_items == true`:
       - Invocar no `PlaylistManager` o método de reposicionamento em bloco:
         ```cpp
         void move_tracks(const std::vector<size_t>& from_indices, size_t target_idx);
         ```
       - Reordenar as faixas preservando a integridade da fila e atualizando o `m_current_index`.
       - Desativar a flag de arraste e persistir a nova ordem no `cache/playlist.json`.

---

## Etapa 4: Desativação do Monitoramento / Hook do RivaTuner (RTSS)

### 4.1 Contexto e Diagnóstico
O **RivaTuner Statistics Server (RTSS)** é um software de terceiros amplamente utilizado para sobreposição em tela (OSD) de telemetria de GPU/CPU/FPS em jogos.
- O RTSS instala ganchos globais no sistema operacional (`WH_GETMESSAGE` / `AppInit_DLLs`) e injeta suas bibliotecas (`RTSSHooks64.dll` e `RTSSHooks.dll`) em qualquer processo que inicialize contextos 3D (Direct3D 9/11, Vulkan ou OpenGL).
- Como o SDL2 do Freenamp inicializa um renderizador acelerado por hardware (Direct3D11 no Windows), o RTSS detecta as chamadas de swapchain (`IDXGISwapChain::Present`) e sobrepõe seu OSD de hardware sobre a interface compacta retrô do player.

### 4.2 Solução Técnica Oficial sem Efeitos Colaterais
O criador e desenvolvedor do RTSS (**Unwinder**) implementou um mecanismo de exclusão nativo oficial nas versões modernas do RTSS:
- Quando o RTSS inspeciona um novo processo no Windows, ele lê a **Tabela de Exportação PE** (Export Address Table) do executável principal (`.exe`).
- Se o executável exportar formalmente a variável simbólica `RTSSHooksCompatibility`, o RTSS **aborta imediatamente a injeção do gancho** naquele processo.

### 4.3 Plano de Implementação Técnica
1. **Exportação Simbólica em `frontend/src/launcher.c`**:
   No arquivo de inicialização do executável nativo Windows (`freenamp.exe`), adicionar a exportação:
   ```c
   // Mecanismo nativo de exclusão para o RivaTuner Statistics Server (RTSS)
   // Informa ao RTSSHooks64.dll / RTSSHooks.dll para não injetar hooks neste processo
   __declspec(dllexport) DWORD RTSSHooksCompatibility = 0x00000000;
   ```
2. **Exportação Redundante em `frontend/src/main.cpp`**:
   Exportar o mesmo símbolo também na DLL do core (`libfreenamp_core.dll`):
   ```cpp
   #if defined(_WIN32)
   extern "C" __declspec(dllexport) unsigned long RTSSHooksCompatibility = 0x00000000;
   #endif
   ```
3. **Resultado Esperado**:
   - O RTSS detecta a flag de compatibilidade e ignora o Freenamp completamente.
   - O player continua utilizando renderização acelerada por GPU do SDL2, mantendo vsync e suavidade, mas sem a exibição indesejada do OSD de hardware.
   - Não requer nenhuma configuração manual do usuário dentro do painel do RivaTuner.

---

## Ordem Recomendada de Execução

| Ordem | Etapa | Foco Principal | Módulos Afetados |
| :---: | :--- | :--- | :--- |
| **1** | [Etapa 4: RivaTuner](#etapa-4-desativação-do-monitoramento--hook-do-rivatuner-rtss) | Bloqueio de injeção RTSS | `launcher.c`, `main.cpp` |
| **2** | [Etapa 2: Mixer do Windows](#etapa-2-identificação-no-mixer-de-volume-do-windows-freenamp-em-vez-de-url) | Nome limpo na sessão de áudio WASAPI | `audio_engine.cpp` |
| **3** | [Etapa 1: Tempo Regressivo](#etapa-1-alternância-de-modo-de-tempo-normal--regressivo-no-relógio-led) | Alternância de tempo normal/regressivo | `main_view.hpp`, `main_view.cpp` |
| **4** | [Etapa 3: Playlist](#etapa-3-aprimoramento-da-playlist-duplo-clique-seleção-múltipla-e-reordenação-por-arrasto) | Duplo clique, seleção múltipla e drag & drop | `playlist_view.*`, `playlist_manager.*` |
