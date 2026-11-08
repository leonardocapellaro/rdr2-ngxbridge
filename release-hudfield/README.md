# RDR2 Vulkan FG — build `0.82+hudfield-v4.1` (B390 + OptiScaler)

> Procure a tag **`hudfield-v4.1`** na primeira linha do `RDR2VulkanFG.log`.
> Se ela não aparecer, o `.asi` antigo continua em uso.

**O que este build é:** a base 0.82 do projeto (fonte pública, MIT — licença do
autor original intacta em `LICENSE.txt` do repo) + nossas mudanças: diagnóstico
de menu/hotkey, knobs de composição de UI do XeSS-FG, e proteção contra
configurações que matariam o FG. Nada de binaries NVIDIA/Intel/Rockstar aqui —
só o `.asi` compilado por nós e o coletor de diagnósticos.

**O que já está validado em campo (B390, sessão 07/11):** FG 2x funcionando sob
Vulkan com OptiScaler (~36 fps reais → ~72 exibidos em 3440x1440). O menu nunca
recebeu cliques/teclas (0 eventos no log) — os diagnósticos v2+ desta build
localizam onde o input morre. A roda de itens pisca — a grade abaixo testa o
mecanismo.

## Mudanças vs. v2

1. **`XeSSUiMode` (INI, valores 1–5, padrão 4):** escolhe o modo de composição
   de UI do swapchain XeSS-FG na criação. A build registra o modo **efetivo** na
   linha `... uiMode=%u uiComposition=%d` do log.
2. **Clamp de segurança:** modos 2/3/5 *exigem* uma textura UI separada que
   RDR2/Vulkan não expõe — sem ela o SDK recusaria a criação
   (`MISMATCH_INPUT_RESOURCES`) e **nenhum frame seria gerado**. Se você poner
   um desses, a build reduz para 4 e loga
   `XeSSUiMode=N needs a UI texture RDR2 cannot provide; clamping to 4`.
3. **`XeSSUiComposition` (INI, -1/0/1, padrão -1):** força o estado de
   composição de UI (`-1` = auto do autor: ligado, exceto modo `NONE`; `0` =
   força desligado; `1` = força ligado).
4. **Diagnósticos v2 preservados:** `menu: open/close`, primeiros cliques
   observados na fila do jogo, hotkeys ignoradas por foco errado.
5. **Sem update-check remoto:** o identificador do Nexus do autor original foi
   zerado — este build não faz nenhuma chamada de rede nem abre página de
   terceiros.
6. Branding das strings visíveis: **RDR2 Vulkan FG**.
7. **Novo na v4 — `XeSSSynthUI` (INI, 0/1, padrão 0):** sintetiza a camada UI premultiplicada
   que os modos 3/5 exigem. Um compute shader resolve `UI = B − (1−a)·C` entre o quadro
   apresentado (B) e o snapshot HUD-less (C) do mesmo epoch; o alfa vem do maior delta de
   canal (escalado). A identidade composicional reproduz B exatamente para qualquer alfa —
   o erro residual fica a cargo do refinamento por extração do modo 5 (mitigação documentada
   da Intel para UI semitransparente). Experimental: o palpite de alfa só vale olho em
   hardware; com a chave em 0 o comportamento é idêntico ao v3.
8. **Novo na v4.1:** com `XeSSSynthUI=0` (padrão) a pipeline de síntese e seu anel de staging
   (≈60 MB de VRAM em 3440x1440) deixam de ser alocados — antes eram criados mesmo desligados.
   Ajuste de strings de log; nenhum outro comportamento muda em relação à v4.

## Instalação (troca 1 arquivo)

1. Jogo fechado. Renomeie o `.asi` atual (v2) para `RDR2VulkanFG.asi.v2.bak` — rollback.
2. Copie este `RDR2VulkanFG.asi` para a pasta do jogo. O coletor opcional vai em
   `RDR2VulkanFG\RDR2VulkanFGDiagnostics.dll` (subpasta, como antes).
3. No `RDR2VulkanFG.ini`, mantenha:
   ```ini
   FGProvider=XeSS
   DebugLogging=0        ; 0 = menos custo; 1 = log detalhado (use 1 só na coleta)
   ```
   (Os knobs novos têm padrão bom; não precisa escrever nada neles para o baseline.)

## Grade de testes da roda que pisca

O flicker da roda de itens bate exatamente com a fraqueza conhecida do modo 4
(*BACKBUFFER_HUDLESS*: a UI é extraída por diferença backbuffer-vs-hudless —
a Intel documenta que UI semitransparente/animada sofre com isso). Rode cada
config **numa sessão curta**, abra a roda de armas/itens, observe, e rode
`collect-once.bat` antes de fechar:

| Config INI | O que esperar | Diagnóstico |
|---|---|---|
| *(nada — baseline)* | modo 4 + composição ligada (estado de hoje: roda pisca) | controle |
| `XeSSUiMode=1` | interpola o backbuffer **com** HUD embutido; sem composição. A roda é interpolada pelo modelo em vez de recomposta | se a roda parar de piscar mas **borrar** ao girar → confirma que o problema é a extração por dif; troca um artefato por outro |
| `XeSSUiComposition=0` (com modo 4) | composição desligada: frames gerados **sem HUD nenhum** | esperado PIOR (HUD pisca sumindo) — só para confirmar a direção; não deixe ligado |
| `XeSSSynthUI=1` + `XeSSUiMode=5` (v4) | UI sintética premultiplicada + refinamento por extração — a mitigação que a Intel documenta p/ UI semitransparente | se o culpado for o modo 4, esta deve ser a MELHOR roda; confira `Synthetic UI layer first frame` e `uiMode=5` no log |

Regras da grade: uma variável por sessão; anote qual INI usou; confira no log a
linha `uiMode=... uiComposition=...` refletindo o pedido (ou o log de clamp).

**Modos 3/5 na v4:** com `XeSSSynthUI=1` eles deixam de ser barrados — a textura UI passa a ser a camada sintetizada (a OptiScaler faz o equivalente com HUDFix). O clamp continua ativo com a chave desligada ou backbuffer HDR, sempre com log explicando.

## Menu e multiplicador

O combo **Multiplier (2x/3x/4x)** existe e é relido a cada present — na sessão
anterior nenhum clique/tecla chegou ao mod. Protocolo v3:
1. Abra o jogo, espere estar renderizando mundo (não menu principal).
2. Aperte a tecla do menu (`MenuKey`, padrão `-` do numpad) **com o jogo em
   primeiro plano**. O log deve gravar `menu: open`.
3. Se não abrir: clique 1x dentro da janela do jogo, depois `Alt+Tab` fora e de
   volta, teste de novo — os diagnósticos vão dizer se o clique apareceu na fila
   de mensagens do jogo ou se o hotkey foi rejeitado por foco.
4. Não selecione DLSS-G nem "Restart FG": DLSS-G não roda em Arc.

## Honestidades

- Base **0.82**, não 1.0beta — o beta trava com OptiScaler antes de qualquer
  FG (nunca rodou). O caminho que usamos existe idêntico nos dois.
- Build real: `zig c++ -target x86_64-windows-gnu` (zig **0.17.1**; o autor usa
  0.16). Compila limpo; carrega/descarrega no Wine; **não rodou RDR2** aqui —
  só a B390 prova.
- O custo de performance observado (47→~36 reais) é o custo conhecido de
  inferência a 1440p UW numa B390; nada segura para otimizar na parte aberta.

## Rollback

Jogo fechado → apague este `.asi` → renomeie o `.bak` da v2 de volta. A ponte
`zz_ngxbridge` não é tocada.
