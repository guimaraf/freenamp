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


