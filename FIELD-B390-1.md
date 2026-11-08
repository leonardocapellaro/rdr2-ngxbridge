# FIELD-B390-1 — primeira validação na máquina real (2026-11-07)

Binários: zz_ngxbridge 0.1.2 + RDR2VulkanFG 1.0beta (Nexus 10997, "With Amd
Support") + OptiScaler 0.9.4. Logs em `error_logs/` (bridge + RDR2VulkanFG).

## O que a ponte provou na máquina real

Cadeia §7 da DIAGNOSIS cumprida até o passo 4:

1. `spoofed provider path for capture module: ...\nvngx.dll` (W e A) ✔
2. gate do Mor3a aceitou: `FG input pipeline v2: accepted SR handle=...
   flags=0x2b render=2293x960 output=3440x1440` — linha inexistente nos logs
   antigos (FG-109) ✔
3. `NGX inputs first valid: depth 2293x960 fmt=130, mvec ... fmt=83,
   jitter=(0.0000,-0.1667) mvScale=(1.0,1.0)` — depth/MV/jitter válidos;
   **o jitter leu** (melhor que a limitação prevista em DIAGNOSIS §6.2) ✔
4. telemetria `input=` subiu a 725+ — **FG-109 não existe mais** ✔

Anomalias observadas (inofensivas):
- hooks de fábrica: `6/6 created` mas zero `intercepted` → o jogo cria o
  objeto de parâmetros sem passar pelos corpos exportados que hookizamos
  (OptiScaler devolve impl interna via GPA hook). O pipeline funcionou
  mesmo assim; o espelho não foi necessário na prática (OptiScaler resolve
  os cross-type internamente).
- observer `LdrGetProcedureAddress` armado mas mudo → resoluções do jogo
  não passam pelo resolver do ntdll visível ao hook. A pergunta §6.1 ficou
  sem resposta pelo observador, mas respondida de fato: o gate aceitou,
  logo a resolução do chamador-alvo aconteceu.

## Novo obstáculo (FORA da ponte — camada do próprio Mor3a)

XeSS bridge fica esperando snapshot de passe "HUD-less post-tone-map"
(`haveHud`, xess_provider.cpp:676/683; erro :691) por ~17 s e desliga FG.
Assinatura exigida (hud_capture.cpp:130-148): render pass com color
attachment = imagem do swapchain virtual do bridge, extent total,
samples=1, **loadOp=LOAD**, layout≠UNDEFINED, 1 imagem subpass-0, mesmo
epoch. Com OptiScaler, o passe final do jogo mira o offscreen do OptiScaler
e o conteúdo chega às imagens via copy/blit → assinatura nunca casa.
**Nenhuma chave de ini relaxa isso.** `TagHUDLess/TagUI` só valem p/
DLSS-G nativo (Streamline slSetTagForFrame).

## FG-201 às 99s: autoinfligido pelo menu, não é XeSS

`Live DLSS-G setup failed` (xess_provider.cpp:493) só alcançável via
reinício do FG **com provider DLSS-G selecionado no menu** (menu.cpp:416).
DLSS-G nunca roda em Arc: `LoadAmpereUnlock` aceita só famílias RTX
(ampere_boot.cpp:113) → lock sticky FG-201 até reiniciar o jogo.
Regra: `FGProvider=XeSS` no ini, jogo fechado; **nunca** selecionar DLSS-G
no menu. `context ownership/mode rejected` (5s) é cosmético (pacing
software do SDK 2.0.2.68). `libxell.dll`/`libxess_fg.dll` carregaram OK.

## Versionamento do fonte

Repositório GitHub = **0.82 público** (SOURCE-NOTES.txt; config.cpp não
conhece FSRFG e migra FSR3→DLSSG). O binário 1.0beta adiciona FSRFG
(FSR4/*dll no pacote) por cima; strings do caminho XeSS batem com o fonte
público (FG input pipeline v2, MFG unlock, virtual swapchain).

## Próximos passos testados/pendentes

1. (teste 1, custo zero) `FGProvider=FSRFG` no 1.0beta — suporte AMD novo
   do beta, runtime FSR4 FG embutido, Vulkan agnóstico de GPU; ver se o
   provider FSRFG exige haveHud.
2. (se 1 falhar) patch público 0.82 em `hud_capture.cpp` CaptureCandidate
   (aceitar fonte de blit/copy full-extent para imagem de swapchain como
   HUD-less, ou relaxar LOAD/membros) + build MSVC na máquina do usuário —
   com o aviso honesto: binário derivado do fonte público 0.82, sem FSRFG.
3. relatório a Mor3a no Nexus com este log (eles development ativo; correção
   da assinatura HUD-less sob OptiScaler Vulkan é deles).
