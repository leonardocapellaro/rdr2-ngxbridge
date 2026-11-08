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

## Próximos passos — resultado (2026-11-07, fim do dia)

1. `FGProvider=FSRFG` no 1.0beta: **FALHOU na B390.** Erro no menu do
   jogo (foto do usuário): FG-3xx "FSR Vulkan features unavailable ...
   Three independent pacing queues are unavailable". FidelityFX-FG
   Vulkan exige 3 filas de pacing independentes que o driver Intel não
   expõe. Beco sem saída enquanto o driver não mudar; a rota XeSS+patch
   é a única viável hoje.
2. **FEITO** — patch `hud_capture.cpp` no fonte público 0.82 (chave
   opt-in `CapturePresentTransfers=1`: blit/copy full-extent para
   imagem de swapchain virtual conta como snapshot HUD-less; layout
   TRANSFER_DST + restauração mantêm as barriers do jogo
   consistentes). Compilado na máquina DEV em Linux (não precisou de
   MSVC na máquina do usuário): `tools/build-mor3a-zig-linux.sh`, zig
   **0.17.1** (o 0.16.0 do Build.ps1 tem libc++ windows-gnu quebrado
   em host Linux; exige shim `Windows.h`→windows.h em
   lib/libc/include/any-windows-any/). Baseline limpa compila
   (d2adebe8…) e só o binário patchado contém as strings novas
   (f375dede…); smoke Wine LoadLibrary/FreeLibrary OK. **Não validado
   em jogo** — falta sessão na B390. Entrega:
   `release-mor3a-hudless/` (README pt-BR com instalação/rollback,
   patch, NEXUS-REPORT-draft.md, collect-once.bat de coleta única).
3. relatório a Mor3a: **rascunho pronto** em
   `release-mor3a-hudless/NEXUS-REPORT-draft.md` (inglês, pronto para
   colar; cobre a assinatura HUD-less sob SR Vulkan de terceiros, o
   FG-3xx das filas de pacing e a armadilha DLSS-G no menu em Arc).
   Envio fica com o usuário (a conta Nexus é dele).
