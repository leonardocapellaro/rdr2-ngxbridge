# Teste na B390 — pacote v3 (marca no log: `hudfield-v3`)

## Antes de tudo (1x só, jogo fechado)

1. No PC da B390, baixe o zip do release:
   https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/tag/v0.82-hudfield-v3
   Extraia em qualquer pasta (ex.: `Downloads\hudfield-v3`).
2. Na pasta do jogo: renomeie `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v2.bak`.
3. Copie o `RDR2VulkanFG.asi` do pacote pra pasta do jogo. O
   `RDR2VulkanFGDiagnostics.dll` do pacote vai na subpasta `RDR2VulkanFG\` da pasta do jogo.
4. Abra `RDR2VulkanFG.ini` (mesma pasta do `.asi`) e deixe o bloco do teste BASELINE (abaixo).

Caminho do log, sempre: `<pasta do jogo>\RDR2VulkanFG.log`. Toda sessão começa com a linha
`... RDR2 Vulkan FG 0.82+hudfield-v3 ...` — se não aparecer, o `.asi` novo não está em uso.

## Como rodar cada teste (repetir os 4 passos)

- Jogo aberto, personagem no mundo (não em menu principal). 2–3 min de gameplay.
- Faça a ROTEIRA do teste (abaixo de cada um).
- Feche o jogo. Na pasta extraída, dê dois cliques em `collect-once.bat`.
- Renomeie o zip gerado com a etiqueta do teste: `coleta-BASE.zip`, `coleta-MODE1.zip`, `coleta-COMP0.zip`.

Depois, me mande os zips (ou o caminho).

---

## TESTE 1 — BASELINE (mesmo setting de 07/11, só pra confirmar a v3)

No INI, seção `[fgvk]`:
```ini
FGProvider=XeSS
DebugLogging=1
```
(apague/comente `XeSSUiMode` e `XeSSUiComposition` se existirem; `CapturePresentTransfers` não é necessário)

Roteira:
1. Ande pelo mundo. Deve dar ~2x os fps "reais" (como antes: ~36→~72).
2. Abra o menu do MOD com a tecla `-` do NUMERICO. No log deve aparecer `menu: open`.
   - Se abriu: teste trocar o Multiplier pra 4x e veja os fps dobrarem de novo (~144).
   - Se NÃO abriu: abra o jogo de novo? NÃO — siga o TESTE 4 (diagnóstico de input).
3. Abra a roda de armas e a roda de itens. Deve PISCAR (é o problema conhecido — confirme e pronto).
4. Procure no log a linha: `virtual swapchain: ... uiMode=4 uiComposition=1`.

Resultado esperado: funciona como na sessão passada, roda pisca, e o log mostra `uiMode=4 uiComposition=1`.

## TESTE 2 — `XeSSUiMode=1` (o teste importante)

Mesmo INI + uma linha:
```ini
XeSSUiMode=1
```

Roteira: mesma (roda de armas/itens é o foco; repare principalmente na RODA DE ITENS).

O que os resultados SIGNIFICAM (me diga qual aconteceu):
- **(a) A roda parou de piscar, mas embaça/derrete quando você gira ou ela anima**
  → o culpado é a "extração da UI por diferença" do modo 4. Vitória parcial: abre o
    caminho do modo 5 (sintetizar a UI), que é o fix de verdade. É o mais provável.
- **(b) A roda continua piscando do mesmo jeito**
  → o flicker não vem só da extração; vou investigar o caminho do HUD com OptiScaler.
- **(c) O mundo/objetos começaram a borrar ou deixar rastro (ghosting)** — a roda pode
  até parar, mas ficou pior
  → o modo 1 manda o HUD pela IA de interpolação, que é a fraqueza documentada da
    Intel. Não é pra deixar ligado; só mede a direção.
- No log confirme `uiMode=1` (se aparecer `uiMode=4`, o INI não foi lido).

## TESTE 3 — `XeSSUiComposition=0` (esperado PIOR — só confirmar a direção)

Mesmo INI do baseline, SEM a linha `XeSSUiMode`, com:
```ini
XeSSUiComposition=0
```

Roteira: 30–60 s de gameplay + roda.

Resultado esperado: PIOR que baseline (o HUD tende a sumir/emagrecer nos frames
gerados, piscando feio). Se piorar mesmo: ótimo, confirma que a composição ligada é o
que está segurando o HUD hoje. **Desligue essa chave e volte ao baseline logo.**

## TESTE 4 — Diagnóstico do MENU (roda junto com o TESTE 1, se o menu não abrir)

Com o jogo em primeiro plano:
1. Aperte `-` do NUMERICO. (É a tecla do menu, `MenuKey=0x2D`.)
2. Não abriu? Clique 1x em qualquer lugar DENTRO da janela do jogo e aperte de novo.
3. Não abriu? `Alt+Tab` pra fora do jogo e `Alt+Tab` de volta, aperte de novo.
4. Não abriu? Aperte `End` e depois `*` do NUMERICO (são hotkeys — o `*` liga/desliga o FG;
   os fps devem cair/dobrar com ele).

Depois, no log, me diga quais destas linhas existem:
- `menu: open` → menu abriu (use o menu pra trocar o multiplicador)
- `menu input: left click observed` → clique chega no jogo (problema é só o menu)
- `hotkey: ... ignored — foreground` → tecla chegou, jogo não estava focado
- nada disso → o input não chega nem perto; é aqui que eu vou consertar no fork

## Sem o menu (alternativa do multiplicador, se necessário)

Com o jogo fechado, no INI: `DLSSGFrames=1` = x2, `2` = x3, `3` = x4. Reinicie o jogo a
cada mudança e confirme nos fps e na linha de telemetria do log.

## Regras

- Uma variável por sessão. Nunca troque provider pra DLSS-G na sessão (trava).
- Não apague os `.bak` (beta e v2) — são o rollback.
- Sessão de uso normal depois dos testes: `DebugLogging=0` (economiza um tiquinho de fps).
