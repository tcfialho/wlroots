# Notas de Otimização e Decisões de Design (wlroots)

Este documento registra as decisões de design, debates técnicos e soluções implementadas para otimizações de hotpath no wlroots.

---

## 1. Scene Graph (`types/scene/wlr_scene.c`)

### Gargalo Original
A cada frame, `wlr_scene_output_build_state` limpava a render list lógica (`size = 0`), montava a lista de nós visíveis no frame atual (fazendo `wl_array_add` e expandindo via `realloc` se necessário) e, no final, encolhia o array físico no heap para o tamanho de nós usando `array_realloc(list_con.render_list, list_con.render_list->size)`.

### Solução Aplicada
Removeu-se a chamada de `array_realloc` ao final de cada frame. O array agora mantém a capacidade física máxima atingida na sessão ativa. Isso elimina chamadas de `realloc` de encolhimento e re-alocação em transições rápidas (minimizar janelas, mudar de workspace, abrir/fechar aplicativos).

### Debate sobre Estouro de Memória e Alternativa de Trava de Segurança
Durante a revisão de código, levantou-se a preocupação sobre o crescimento do array e a retenção do consumo de pico indefinidamente (caso um estresse temporário com milhares de janelas ocorra).

* **Por que a solução atual é segura:** 
  * A render list só armazena nós **visíveis** na tela (limitação física de resolução).
  * Mesmo em picos de estresse de 1.000 nós visíveis simultâneos, o consumo máximo é de apenas ~128 KB (1000 nós * 128 bytes por entrada).
  * O wlroots já realiza a desalocação completa do array (`wl_array_release`) no desligamento/suspensão ou destruição da tela (`scene_output_destroy`).
* **Ideia de Trava de Segurança (Teto Inteligente - "Cap-based Shrinking"):**
  Se no futuro for desejado um controle rígido contra picos bizarros de retenção na sessão ativa, sugere-se a implementação de um teto de segurança permanente (ex: 128 elementos ou ~16 KB). Se a alocação ultrapassar esse teto, permite-se o crescimento durante o frame, mas executa-se o encolhimento de volta para o limite seguro de 128 elementos no fim do ciclo.
  
  Exemplo de código da ideia:
  ```c
  if (list_con.render_list->alloc > 128 * sizeof(struct render_list_entry) &&
          list_con.render_list->size <= 128 * sizeof(struct render_list_entry)) {
      array_realloc(list_con.render_list, 128 * sizeof(struct render_list_entry));
  }
  ```
  *(Nota: Optou-se por não implementar esta trava no momento para priorizar simplicidade e performance absoluta, dado que o consumo de pico de KB é irrisório e a desalocação final do array já é feita de forma limpa).*

---

## 2. Renderer OpenGL ES 2 (`render/gles2/pass.c`)

### Problema do Cache Compartilhado (Bug 1)
O cache original dos filtros OpenGL (`cached_min_filter`/`cached_mag_filter`) foi colocado em `struct wlr_gles2_texture`. Porém, quando múltiplas texturas são importadas de um mesmo buffer compartilhado (ex: no screencopy), elas compartilham o mesmo handle físico da textura GL (`wlr_gles2_buffer::tex`). Uma textura dessincronizava o cache da outra ao alterar as propriedades na GPU, gerando rendering incorreto/borrado.

### Solução Aplicada
Os campos do cache de filtro foram estendidos para a estrutura do buffer `wlr_gles2_buffer`. A lógica de renderização consulta se a textura possui um buffer associado e centraliza a leitura/escrita do cache nele, mantendo o cache local de texturas privadas intacto.

---

## 3. Busca de Formatos de Pixel (`render/pixel_format.c`)

### Alinhamento e Manutenção (Bug 2)
Para alcançar complexidade $O(1)$ na conversão e busca de informações de formatos DRM de pixels frequentes, mapeou-se os casos no `switch-case` para retornar ponteiros diretos do array global `pixel_format_info[X]`. 

Para evitar regressões e fragilidade caso a ordem do array estático seja modificada futuramente no desenvolvimento do repositório, inseriu-se **42 `static_assert`** protegendo a indexação direta em tempo de compilação.
