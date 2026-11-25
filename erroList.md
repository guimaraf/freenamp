# Erro ao reproduzir faixa após renovar o link do YouTube

**Status:** registrado em 25/11/2026. Correção incluída na tag `v1.1.13`. Validação da faixa original pendente.

## Como acontece

1. Abra uma playlist salva quando o Freenamp ainda usava o resolvedor antigo do YouTube.
2. Mande tocar uma faixa dessa playlist.
3. O quadro **Freenamp Info** exibe **Renovando link de áudio...** com **25%** na primeira captura.
4. Em seguida, o quadro exibe **Erro ao reproduzir faixa**, como mostra a segunda captura. A faixa não começa a tocar.

O usuário observou que a tela de renovação terminou antes do erro. As capturas mostram a troca de estado da interface. Ainda falta confirmar se a renovação retornou uma URL utilizável e se o libmpv conseguiu abri-la.

## Comportamento esperado

Ao renovar um link antigo, o Freenamp deve aguardar o resultado da resolução, carregar a nova URL e começar a tocar automaticamente a mesma faixa que o usuário escolheu.

## Hipótese para retomar

O problema pode estar na passagem do resultado assíncrono para a reprodução. O código atual já resolve o stream em uma tarefa de fundo e chama `load_url(..., true)` na conclusão se a solicitação ainda for a atual e a faixa selecionada corresponder. Vale conferir esse caminho antes de acrescentar outra chamada assíncrona.

No fluxo atual, **Erro ao reproduzir faixa** é a mensagem usada quando o libmpv sinaliza um erro de reprodução após o controlador já ter consumido sua tentativa de renovação. Esse caminho é compatível com as capturas e ainda precisa ser confirmado com a faixa afetada.

- Verificar se o resolvedor retornou uma URL nova e se ela corresponde à faixa solicitada. [Resolução do stream](backend/src/yt_resolver.cpp#L580) · [Conclusão da renovação](backend/src/core_controller.cpp#L318)
- Confirmar se a conclusão chegou ao `load_url` e se o libmpv emitiu um erro depois de aceitar a solicitação de carregamento. Registrar o código ou motivo retornado pelo libmpv. [Carregamento e eventos de áudio](backend/src/audio_engine.cpp#L110) · [Tratamento de erro e nova tentativa](backend/src/core_controller.cpp#L494)
- Comparar a mesma faixa selecionada na playlist antiga e adicionada pelo link original. Isso ajuda a identificar se o erro depende dos dados salvos ou também ocorre com uma entrada nova.

## Correção na versão 1.1.13

O Freenamp passou a carregar a URL junto com os cabeçalhos HTTP devolvidos pelo yt-dlp. Quando uma playlist antiga guarda somente a URL, o aplicativo resolve a faixa novamente antes de tocar. Os cabeçalhos ficam em memória e as faixas carregadas de uma sessão anterior são resolvidas de novo quando necessário.

Os testes confirmaram a renovação de uma entrada no formato antigo e o envio dos cabeçalhos a um servidor HTTP de teste pelo libmpv. Nesse cenário, a reprodução avançou. A faixa da playlist original ficou fora do teste.
