# Plano: otimizações que o usuário percebe

## Diagnóstico

Um quadro a 60 Hz tem 16,6 ms. A CPU do wlroots (scene graph e commit DRM) gasta cerca de 0,2 a 0,4 ms por quadro.
Otimizações de CPU nessa escala não são percebidas. O que o usuário sente é latência entre o movimento do mouse e a
imagem na tela, engasgos e rasgo de imagem (tearing).

Os commits recentes (SAO, tabelas `switch` em `render/pixel_format.c`, realocação na lista de renderização) são higiene
de código. Não entram neste plano.

## Prioridade

| # | Frente | O que o usuário sente | Risco | Pode ser validada aqui (Windows)? |
|---|---|---|---|---|
| 1 | Agendamento tardio do quadro | Latência de entrada menor | Médio | Não |
| 2 | Cache de pipelines do Vulkan em disco | Sem engasgo na primeira vez que aparece um formato novo | Baixo | Não |
| 3 | Overlays de hardware no scene | Menos GPU e bateria com vídeo ou jogo e algo por cima | Alto | Não |
| 4 | Commit atômico só de cursor | Cursor fluido sob carga alta | Médio-alto | Não |

Itens menores, para depois: criar pipelines Vulkan antecipadamente na inicialização; trocar o `glFinish()` da leitura de
pixels (`render/gles2/texture.c:228`) por sincronização explícita, para captura de tela e gravação não travarem a CPU.

## Frente 1: agendamento tardio do quadro

**Situação atual.** O evento `frame` chega assim que o vblank anterior termina (`types/output/output.c:573` e
`wlr_output_schedule_frame`, linha 862). O compositor desenha logo em seguida, e o quadro pronto espera o próximo
vblank. Isso soma de 8 a 16 ms de atraso para a entrada.

**Mudança proposta.**
- Na biblioteca: guardar o tempo de renderização dos últimos quadros e expor uma função que diga quando começar a
  desenhar. A conta é `T_vblank - tempo_estimado_de_render - margem`.
- No `tinywl`: usar essa função como demonstração.

**Por que não é o `PAGE_FLIP_ASYNC`.** Esse modo provoca rasgo de imagem e, em muitos drivers, só vale para o plano
primário. Não resolve o problema.

**Como validar.** Em hardware DRM real, medir o atraso entre o evento de mouse e o quadro que o contém, antes e depois.
Medir também o número de quadros perdidos (vblank perdido). Se a estimativa errar, a perda de quadros piora a
experiência, então a margem precisa ser testada sob carga.

**Expectativa honesta.** A redução depende do tempo de render. Ganho de cerca de um terço a metade do intervalo de
quadro, e só aparece com cursor e janelas em movimento. O número de 50% a 70% da análise externa não foi medido.

## Frente 2: cache de pipelines do Vulkan em disco

**Situação atual.** Os pipelines são criados sob demanda em `setup_get_or_create_pipeline`
(`render/vulkan/renderer.c:1907`), com `vkCreateGraphicsPipelines` nas linhas 2054 e 2167. Não há cache persistente.
O próprio código tem um TODO sobre isso (`render/vulkan/renderer.c:32`).

**Mudança proposta.** Carregar um `VkPipelineCache` do disco na inicialização e gravá-lo ao desligar. Usar o caminho de
cache do usuário, com fallback para cache vazio se o arquivo estiver corrompido ou for de outro driver (o cabeçalho do
cache identifica o driver).

**Como validar.** Medir o tempo do primeiro quadro com um formato novo, na primeira sessão e na segunda. O esperado é que
a segunda sessão não tenha o atraso.

**Escopo.** Só afeta o renderizador Vulkan.

## Frente 3: overlays de hardware no scene

**Situação atual.**
- `types/scene/wlr_scene.c:2413`: o scanout direto só é tentado com `list_len == 1`. Qualquer outro nó (notificação,
  barra, cursor de software) desativa o scanout.
- `backend/drm/drm.c:251` e `:255`: o backend só procura planos primário e de cursor. Os planos de overlay não são usados.

**Mudança proposta.**
1. Mapear nós do scene para planos de overlay, em ordem de camadas.
2. Validar a combinação com commits atômicos só de teste (`DRM_MODE_ATOMIC_TEST_ONLY`) antes do commit real.
3. Se o hardware recusar, cair para a composição atual.

**Como validar.** Reproduzir vídeo em tela cheia com uma notificação por cima. Medir o uso de GPU, a frequência de
composição no renderizador e se a imagem aparece sem falhas. Essa medição só existe em hardware real.

**Risco.** Alto. Fica para depois que a frente 1 estiver pronta, porque a ordem de quadros afeta os dois.

## Frente 4: commit atômico só de cursor

**Situação atual.** `drm_connector_move_cursor` (`backend/drm/drm.c:1200`) só guarda a posição. A posição vai ao hardware
no próximo commit, que depende do ciclo de quadro.

**Mudança proposta.** Quando só o cursor mudou, fazer um commit atômico só do plano de cursor. Se um flip ainda estiver
pendente, tratar o `EBUSY` e tentar de novo no próximo vblank. Em hardware antigo, usar o caminho legado de movimento de
cursor.

**Como validar.** Com carga alta (um vídeo ou compilação em paralelo), medir a frequência de atualização do cursor e a
latência até a imagem do cursor mudar.

**Expectativa honesta.** Só faz diferença sob carga. No uso normal, o cursor já está sincronizado ao vblank.

## Já implementado (não é tarefa)

Sincronização explícita (`linux-drm-syncobj-v1`) já existe: `backend/drm/atomic.c:368` exporta o `sync_file`, e
`backend/drm/drm.c:372-378` sinaliza as linhas do tempo de liberação. Não há evidência de travamento nesse caminho que
justifique trabalho novo.

## Ordem de execução

1. Frente 2 (menor risco, ganho imediato na primeira vez de cada formato).
2. Frente 1, com medição antes e depois.
3. Frente 4.
4. Frente 3, só depois das anteriores.

## Pré-requisito

Este repositório compila no Windows, mas o DRM e o Vulkan real só rodam em Linux com hardware de tela. Nenhuma frente
acima pode ser considerada validada sem essa medição.
