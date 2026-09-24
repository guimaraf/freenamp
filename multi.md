# Plano de Implementação: Suporte a Teclas Multimídia em Segundo Plano (Minimizado)

Este documento detalha o plano arquitetural e técnico para habilitar o controle do **Freenamp** através das teclas multimídia de teclados físicos (e atalhos de fones de ouvido) mesmo quando a janela principal estiver **minimizada** ou **sem foco**.

---

## 1. Contexto Técnico e Desafio do SDL2

### O Problema do SDL2 com Janelas Minimizadas
Por design arquitetural, o **SDL2 é focado no contexto da janela ativa**:
- Eventos de teclado (`SDL_KEYDOWN`, `SDL_KEYUP`) e scancodes multimídia (`SDLK_AUDIOPLAY`, `SDLK_AUDIONEXT`, etc.) só são entregues na fila de eventos (`SDL_PollEvent`) quando a janela possui **foco de entrada ativo** (`SDL_WINDOW_INPUT_FOCUS`).
- Quando a janela é minimizada (`SDL_WINDOWEVENT_MINIMIZED`) ou perde o foco para outro aplicativo, o sistema operacional para de encaminhar eventos de teclado para a janela do SDL2.

### Solução Arquitetural
Para contornar essa limitação sem adicionar dependências pesadas, implementaremos uma camada de abstração de plataforma (HAL) dedicada a atalhos globais de sistema:
- **Windows**: API nativa Win32 (`RegisterHotKey` + gancho de mensagens do SDL2 via `SDL_SetWindowsMessageHook`).
- **Linux**: Protocolo padrão de desktop **MPRIS v2 via D-Bus** (`org.mpris.MediaPlayer2`), com carregamento dinâmico (`dlopen`) da `libdbus-1` para manter a garantia de **100% de portabilidade** do Freenamp.

```
                   +---------------------------+
                   |        GuiEngine          |
                   +-------------+-------------+
                                 |
                     +-----------v-----------+
                     |   SystemMediaKeys     | (Interface HAL)
                     +-----+-----------+-----+
                           |           |
            +--------------v---+   +---v---------------+
            |  Win32 Backend   |   |   Linux Backend   |
            | (RegisterHotKey) |   |   (D-Bus MPRIS)   |
            +------------------+   +-------------------+
```

---

## 2. Nova Arquitetura de Módulos

### 2.1 Novo Módulo no Frontend: `SystemMediaKeys`
Criaremos um subsistema isolado em `frontend/` composto por:
1. `frontend/include/system_media_keys.hpp`: Interface base abstrata e factory multiplataforma.
2. `frontend/src/system_media_keys_win.cpp`: Implementação Win32 nativa.
3. `frontend/src/system_media_keys_linux.cpp`: Implementação Linux MPRIS v2 / D-Bus.

### 2.2 Contrato da Interface C++20 (`system_media_keys.hpp`)

```cpp
#pragma once
#include <memory>
#include <string>
#include <SDL.h>
#include "core_controller.hpp"

namespace freenamp::frontend {

class ISystemMediaKeys {
public:
    virtual ~ISystemMediaKeys() = default;

    // Inicializa a captura global vinculada à janela SDL e ao CoreController
    virtual bool init(SDL_Window* window, backend::CoreController& core) = 0;

    // Tick periódico chamado no loop do GuiEngine (necessário para dispatch D-Bus no Linux)
    virtual void update() = 0;

    // Notificação de atualização de metadados para o sistema operacional (MPRIS / OS overlay)
    virtual void update_metadata(const std::string& title, 
                                 const std::string& artist, 
                                 int duration_sec, 
                                 backend::PlaybackState state) = 0;

    // Libera atalhos globais e desconecta listeners
    virtual void shutdown() = 0;
};

// Factory para instanciar a implementação correta de acordo com a plataforma
std::unique_ptr<ISystemMediaKeys> create_system_media_keys();

} // namespace freenamp::frontend
```

---

## 3. Especificação Detalhada por Plataforma

---

### 3.1 Backend Windows: Win32 API (`RegisterHotKey`)

#### Mecanismo Operacional
1. **Obtenção do HWND Nativo**:
   O SDL2 disponibiliza a estrutura `SDL_SysWMinfo` para extrair o handle da janela Win32 (`HWND`):
   ```cpp
   SDL_SysWMinfo wmInfo;
   SDL_VERSION(&wmInfo.version);
   if (SDL_GetWindowWMInfo(window, &wmInfo)) {
       HWND hwnd = wmInfo.info.win.window;
   }
   ```

2. **Registro das Teclas Globais**:
   Utilização de `RegisterHotKey` com flag `MOD_NOREPEAT` (para evitar disparo em rajada ao segurar a tecla):
   - `VK_MEDIA_PLAY_PAUSE` (código `0xB3`) $\rightarrow$ ID `1001`
   - `VK_MEDIA_NEXT_TRACK` (código `0xB0`) $\rightarrow$ ID `1002`
   - `VK_MEDIA_PREV_TRACK` (código `0xB1`) $\rightarrow$ ID `1003`
   - `VK_MEDIA_STOP`       (código `0xB2`) $\rightarrow$ ID `1004`

3. **Interceptação Nativa com `SDL_SetWindowsMessageHook`**:
   O SDL2 possui uma função de gancho nativa para interceptar a fila de mensagens do Windows antes que o SDL as processe:
   ```cpp
   static int SDLCALL win32_msg_hook(void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam) {
       if (message == WM_HOTKEY) {
           int hotkey_id = static_cast<int>(wParam);
           auto* self = static_cast<SystemMediaKeysWin*>(userdata);
           self->handle_hotkey(hotkey_id);
           return 0; // Mensagem tratada
       }
       if (message == WM_APPCOMMAND) {
           int cmd = GET_APPCOMMAND_LPARAM(lParam);
           auto* self = static_cast<SystemMediaKeysWin*>(userdata);
           if (self->handle_app_command(cmd)) {
               return 1;
           }
       }
       return 0;
   }
   ```

4. **Tratamento de Ações**:
   - `ID 1001` ou `APPCOMMAND_MEDIA_PLAY_PAUSE`: `core.toggle_pause();`
   - `ID 1002` ou `APPCOMMAND_MEDIA_NEXTTRACK`: `core.next();`
   - `ID 1003` ou `APPCOMMAND_MEDIA_PREVIOUSTRACK`: `core.previous();`
   - `ID 1004` ou `APPCOMMAND_MEDIA_STOP`: `core.stop();`

5. **Encerramento**:
   Chamada de `UnregisterHotKey(hwnd, ID)` para cada hotkey e remoção do gancho do SDL.

---

### 3.2 Backend Linux: D-Bus MPRIS v2 (Padrão de Desktop Moderno)

#### Por que MPRIS v2 e não captura crua via X11 (`XGrabKey`)?
1. **Compatibilidade com Wayland**: Em ambientes modernos sob Wayland (Ubuntu moderno, Fedora, etc.), o servidor de exibição proíbe expressamente que aplicações façam "sniffing" de teclas globais por motivos de segurança.
2. **Conflito de Atalhos**: No X11 tradicional (como Lubuntu/LXQt, XFCE, GNOME), o próprio ambiente desktop já registra as teclas multimídia do teclado. Uma chamada a `XGrabKey` falha imediatamente com o erro `BadAccess` se a tecla já estiver registrada pelo gerenciador de janelas.
3. **Padrão Unificado**: Quando o usuário aperta a tecla `Play/Pause` em qualquer ambiente Linux moderno, o desktop traduz esse pressionamento em uma chamada D-Bus **MPRIS** direcionada ao player em execução.
4. **Benefícios Adicionais Gratuitos**:
   - Funciona com fones de ouvido Bluetooth (botão de play/pause do fone).
   - Integração com controles da tela de bloqueio do Linux.
   - Suporte ao utilitário de linha de comando `playerctl` (ex: `playerctl play-pause`).

#### Arquitetura D-Bus do Freenamp:
1. **Nome de Barramento**:
   O Freenamp solicita o nome de barramento na sessão:
   `org.mpris.MediaPlayer2.freenamp`
2. **Objeto Exportado**:
   `/org/mpris/MediaPlayer2`
3. **Interfaces Implementadas**:
   - `org.mpris.MediaPlayer2`:
     - Métodos: `Raise()`, `Quit()`
     - Propriedades: `CanQuit` (true), `CanRaise` (true), `Identity` ("Freenamp")
   - `org.mpris.MediaPlayer2.Player`:
     - Métodos:
       - `PlayPause()` $\rightarrow$ `core.toggle_pause()`
       - `Play()` $\rightarrow$ `core.play()`
       - `Pause()` $\rightarrow$ `core.pause()`
       - `Stop()` $\rightarrow$ `core.stop()`
       - `Next()` $\rightarrow$ `core.next()`
       - `Previous()` $\rightarrow$ `core.previous()`
     - Propriedades:
       - `PlaybackStatus`: `"Playing"`, `"Paused"`, `"Stopped"`
       - `Metadata`: Dicionário contendo título, uploader/artista e duração.

#### Garantia de 100% de Portabilidade (Dynamic Loading via `dlopen`)
Para não introduzir dependências duras de bibliotecas compartilhadas no pacote portátil Linux:
- O módulo tentará carregar `libdbus-1.so.3` via `dlopen()`.
- Se a biblioteca estiver disponível (caso de 100% dos desktops com Lubuntu/Ubuntu/Debian/Arch/Fedora), o serviço MPRIS é registrado.
- Se o D-Bus não estiver presente (ex: modo headless ou terminal puro), o módulo se desativa silenciosamente com um fallback limpo, sem falhar a inicialização do Freenamp.

---

## 4. Integração no `GuiEngine`

### 4.1 Modificações em `frontend/include/gui_engine.hpp`
Adição do ponteiro para o subsistema de teclas multimídia:
```cpp
#include "system_media_keys.hpp"
// ...
private:
    std::unique_ptr<ISystemMediaKeys> m_system_media_keys;
```

### 4.2 Modificações em `frontend/src/gui_engine.cpp`

1. **No método `GuiEngine::init()`**:
   Após criar a janela SDL (`m_window`):
   ```cpp
   m_system_media_keys = create_system_media_keys();
   if (m_system_media_keys) {
       m_system_media_keys->init(m_window, core);
   }
   ```

2. **No loop principal `GuiEngine::run()`**:
   A cada iteração de frame (~60 FPS):
   ```cpp
   while (m_running) {
       process_events(core);
       core.update();
       if (m_system_media_keys) {
           m_system_media_keys->update(); // Processa mensagens D-Bus pendentes
       }
       render(core);
       SDL_Delay(16);
   }
   ```

3. **No callback de eventos do `CoreController`**:
   Sincronizar mudanças de faixa e estado com o sistema operacional:
   ```cpp
   core.set_event_callback([this, &core](const std::string& event_name) {
       // ... código existente ...
       if (m_system_media_keys) {
           auto tr = core.get_playlist().get_current_track();
           std::string title = tr ? tr->title : "Freenamp";
           std::string artist = tr ? tr->uploader : "";
           int dur = tr ? tr->duration_seconds : 0;
           m_system_media_keys->update_metadata(title, artist, dur, core.get_state());
       }
   });
   ```

4. **No destrutor `GuiEngine::~GuiEngine()`**:
   ```cpp
   if (m_system_media_keys) {
       m_system_media_keys->shutdown();
       m_system_media_keys.reset();
   }
   ```

---

## 5. Modificações no Sistema de Build (CMake)

### 5.1 `frontend/CMakeLists.txt`
Condicionar os arquivos de código-fonte conforme a plataforma:

```cmake
if(WIN32)
    set(SYSTEM_MEDIA_KEYS_SRC src/system_media_keys_win.cpp)
else()
    set(SYSTEM_MEDIA_KEYS_SRC src/system_media_keys_linux.cpp)
endif()

set(FRONTEND_COMMON_SRCS
    src/retro_font.cpp
    src/retro_widgets.cpp
    src/window_dock.cpp
    src/gui_engine.cpp
    src/views/main_view.cpp
    src/views/info_view.cpp
    src/views/eq_view.cpp
    src/views/playlist_view.cpp
    src/views/input_modal.cpp
    ${SYSTEM_MEDIA_KEYS_SRC}
)
```

No Linux, como o carregamento da `libdbus-1` será feito dinamicamente em runtime com `dlopen` / `dlsym`, **não é necessário adicionar linkagem estática ou dinâmica obrigatória de D-Bus no CMake**, mantendo o executável livre de dependências extras não portáteis.

---

## 6. Plano de Fases e Execução

### Fase 1: Interface Base e Implementação Windows
- [ ] Criar `frontend/include/system_media_keys.hpp`.
- [ ] Implementar `frontend/src/system_media_keys_win.cpp` com `RegisterHotKey` e `SDL_SetWindowsMessageHook`.
- [ ] Integrar chamada em `GuiEngine::init` e `shutdown`.
- [ ] **Validação Windows**: Minimizar a janela do Freenamp e testar teclas físicas de Play/Pause, Stop, Next e Prev do teclado.

### Fase 2: Implementação Linux (D-Bus MPRIS v2)
- [ ] Implementar `frontend/src/system_media_keys_linux.cpp` com registro de serviço D-Bus e métodos MPRIS.
- [ ] Adicionar despacho de mensagens D-Bus não-bloqueante no `update()` do `GuiEngine`.
- [ ] Atualizar metadata de faixa no D-Bus quando a música mudar.
- [ ] **Validação Linux**:
  1. No Lubuntu, minimizar o player e pressionar as teclas multimídia do teclado físico.
  2. Testar via terminal com `playerctl -p freenamp play-pause` e `playerctl -p freenamp next`.

### Fase 3: CI/CD e Pacotes Portáteis
- [ ] Compilar no pipeline de CI/CD do GitHub Actions para Windows e Linux.
- [ ] Validar compatibilidade do pacote portátil `freenamp_portable_linux` sem regressões.

---

## 7. Matriz de Riscos e Mitigações

| Risco | Impacto | Mitigação Técnica |
| :--- | :---: | :--- |
| **Outro aplicativo já registrou a hotkey no Windows** | Baixo | `RegisterHotKey` retorna `FALSE`. O software continuará funcionando normalmente através dos atalhos locais e interface. Adicionar log de aviso. |
| **Ambiente Linux sem servidor D-Bus ativo** | Muito Baixo | O carregamento dinâmico via `dlopen` falha graciosamente sem causar crash, operando apenas com a interface gráfica. |
| **Disparo múltiplo de tecla segurada** | Médio | Usar flag `MOD_NOREPEAT` no Windows e debounce de timestamp de 200ms no dispatcher de comandos. |
| **Thread-Safety** | Nulo | O `CoreController`, `AudioEngine` e `PlaylistManager` já possuem seus próprios `std::mutex` internos para todas as chamadas de reprodução. |
