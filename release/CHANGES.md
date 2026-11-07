# CHANGES — o que este pacote adiciona (e o que não toca)

## Nada existente foi modificado

Zero bytes alterados em: OptiScaler 0.9.4, RDR2VulkanFG (Mor3a 1.0 beta),
OptiPatcher, loader ASI dinput8, fakenvapi, RDR2.exe, DRM, configs existentes.
Os repositórios `OptiScaler/` e `rdr2-vulkan-frame-generation/` continuam
intactos (os caminhos citados em DIAGNOSIS.md são só leitura).

## Arquivos novos (tudo)

| Caminho | O que é |
|---|---|
| `case/ngxbridge/src/ngxbridge.cpp` | código da ponte (ASI único, ~600 linhas C++17) |
| `case/ngxbridge/src/mirror_core.{h,cpp}` | ABI compartilhada versionada: mapa de slots real, tabela de coerção defensiva |
| `case/ngxbridge/tests/test_mirror.cpp` | testes unitários nativos da coerção |
| `case/ngxbridge/mock/` | mocks fiéis p/ self-test (game/OptiScaler/Mor3a) — nunca instalados no jogo |
| `case/ngxbridge/build.sh`, `selftest/run_wine.sh` | build MinGW Windows-x64 + self-test automatizado |
| `release/` | este pacote (binário, ini, scripts, docs, self-test Windows) |

## Decisões de projeto (e por quê)

1. **ASI único chamado `zz_ngxbridge.asi`** — o prefixo `zz` garante que o
   loader ASI o carregue por último (ordem alfabética), depois de OptiScaler
   e RDR2VulkanFG, então ele observa e estende um estado já montado. Não
   substitui nem precede ninguém.
2. **Sem hook de `GetProcAddress`.** Ele já tem DOIS donos na pilha:
   OptiScaler (kernel32+kernelbase, sempre com fall-through) e Mor3a (MS
   Detours). Um terceiro engine inline no mesmo endereço — medido na prática
   — silencia as camadas anteriores. Tudo no bridge foi reposicionado para
   superfícies sem dono: `GetModuleFileNameW/A`, `ntdll!LdrGetProcedureAddress`
   e os corpos das 6 exportações de fábrica de parâmetros.
3. **Nenhum `nvngx.dll` em disco.** O spoof é só a resposta de nome para o
   perguntante certo; carregar um arquivo falso criaria um módulo real na
   frente do OptiScaler (regressão de upscale) — inaceitável (§6).
4. **Spoof com escopo de chamador** (handle do OptiScaler + caminho do
   perguntante ∈ `CallerHints`): nenhum outro módulo percebe o bridge. É a
   menor superfície possível para satisfazer exatamente o gate do Mor3a.
5. **Espelho só nos 6 objetos de fábrica**: vtables são patcheadas por
   objeto novo (idempotente, mapa protegido por mutex, nunca liberadas — o
   objeto vive enquanto o OptiScaler vive; sem acesso a recurso Vulkan após
   liberação porque o bridge não toca recursos Vulkan, só a mesa de valores
   tipados do objeto de parâmetros).
6. **Coerção defensiva com lista fechada** (`MirrorCoerce` em mirror_core.h,
   documentada linha a linha): `Ptr↔ULL` sempre; `I↔UI` com extensão/sinal
   corretos; `ULL→I` com checagem de faixa; `F/D→int` só se finito e integral;
   **nunca** `F→Ptr` nem nenhum tipo→ponteiro que não seja ULL/Ptr. Recusou-
   se deliberadamente responder jitter-float pelo slot de ponteiro — documentado
   em DIAGNOSIS §6.2 como limitação com follow-up, em vez de esconder o erro
   com um valor falso.
7. **Instrumentação sem spam**: macros `LOGT` com "primeiros N + a cada 600";
   contadores de GPFN, spoof, alocações e resoluções observadas. Nada por
   frame.

## Mapa de posse de hooks (estado final da pilha)

| API | OptiScaler | Mor3a (RDR2VulkanFG) | zz_ngxbridge |
|---|---|---|---|
| `LdrLoadDll`/`NtLoadDll` | hook | — | — |
| `GetProcAddress` (k32/kb) | hook (fall-through) | Detours | — (não toca) |
| `GetModuleFileNameW/A` | — | — | hook (escopo de chamador) |
| `LdrGetProcedureAddress` | — | — | hook (observador log-only) |
| exportações `NVSDK_NGX_VULKAN_{Create,Create1,Evaluate}` | — | Detours (só após gate aceitar) | — |
| exportações `{Allocate,Get}Parameters` × 3 APIs | — | — | hook (instala espelho) |
| vtable dos objetos de parâmetros | — | lida por mapa reverso | stubs tipados por objeto |

## v0.1.1 — correções da revisão pré-entrega

Uma revisão adversarial do código (achados de code-review + smoke test dos
`.bat` sob Wine) encontrou e corrigiu 11 problemas. Os que importam para quem
vai rodar no jogo:

| # | Problema | Consequência sem correção | Correção |
|---|---|---|---|
| R1 | Corrida em `MirrorEnsureObject` (checagem/patch/registro não atômicos) | dois threads criando o 1º objeto de cada classe podiam salvar stubs como "originais" → **auto-recursão → stack overflow**, ou drops silenciosos do provedor | seção crítica única cobre check+patch+registro |
| R2 | `wcscpy` sem limite em `g_fakePath[MAX_PATH]` | caminho do OptiScaler ≥251 chars → **estouro de buffer** em .bss | montagem verificada; caminho longo demais desliga só o spoof, logado |
| R3 | `fprintf`/`fflush` executando sob a **loader lock** (hooks de GPFN/`LdrGPA`) | deadlock clássico loader-lock↔heap travando o boot do jogo | fila estática lock-free (sem heap) drena num thread normal |
| R4 | Observador obrigatório (`!anyL` desarmava tudo) | hook de **log** fracassar desligava o **fix** | só o spoof é load-bearing; observador/espelho degradam com aviso |
| R5 | `HookBoth` aceitava kernelbase como suficiente | kernel32 não hookado → forwarder disfarça o chamador → spoof nunca dispara **em silêncio** | kernel32 é obrigatório; falha é logada como CRITICAL e o resto arma p/ diagnóstico |
| R6 | `Reset()` (slot 16) nunca espelhado | leitura cruzada pós-Reset responderia **valor antigo** (possível recurso já liberado) — viola o §6 do briefing | slot 16 patcheado; `Reset` apaga a memória do espelho (teste `STALE-GUARD-OK`) |
| R7 | Alvo procurado por só 30 s e desistência eterna | OptiScaler lento (boot com muitos MODs) = bridge inerte a sessão toda | polling enquanto o processo viver, aviso após ~30 s |
| R8 | Observador sempre atribuía "by kernel32.dll" | as perguntas §9 ("quem resolveu?") respondidas erradas | `RtlCaptureStackBackTrace` pulando sistema/trampolins; factories também atribuem |
| R9 | Contador com wrap pulando um `NoteSet` a cada 2^32 | 1 registro perdido por volta (comentário dizia "inofensivo" — não era) | incremento vira só estatística; store sempre acontece |
| R10 | `install.bat` rodando na própria pasta copiada (modo README): move p/ backup e copia de fonte já movida | **instalação falhava no fluxo principal documentado** | modo in-place detectado; backup só no modo com argumento |
| R11 | `%date%` p/ carimbo de backup + caminho sem `\` no argumento + `if` encadeado que wine/cmd não engolem | carimbo inválido, concatenação quebrada, uninstall sem efeito | carimbo `%time%`, normalização de path, `goto` no lugar do `if` duplo |

Instalador/desinstalador foram testados sob Wine nos 5 fluxos (instalação
limpa com argumento, upgrade com backup+restore, uninstall com restore, modo
in-place, uninstall sem backup). Self-test com a nova condição `STALE-GUARD-OK`
e atribuição de chamador: PASS.

## Conformidade com as restrições do briefing (§6)

- sem hooks recursivos/duplicados: um único MinHook, zero sobreposição de endereços ✔
- sem deadlock entre mutex/apresentação/filas Vulkan: único mutex é o mapa de
  valores do espelho (nunca segurando locks de apresentação; stubs não
  chamam Vulkan) ✔
- sem acesso a recurso Vulkan após liberação: bridge não toca recursos ✔
- sem frame anterior como válido / sem inventar motion vectors: coerções só
  reformatam o valor do próprio frame atual; sem valor atual → o Fail original
  do provedor ✔
- verificações de segurança não desligadas para esconder erro: o gate do
  Mor3a continua ativo; a ponte só torna a resposta de nome honesta sobre um
  provedor real (OptiScaler) — recusou-se coerção perigosa documentada ✔
- RDR2.exe/DRM/proteções intocados ✔
- sem regressão no upscaling Vulkan: OptiScaler continua servendo tudo; o
  spoof é invisível fora do chamador-alvo ✔
- ABI compartilhada versionada/documentada/defensiva/fallback: `mirror_core`
  com tabela explícita + testes ✔
- binário Windows x64 real compilado (não fictício): `zz_ngxbridge.asi`
  construído com MinGW 16.2.1 (ABI MSVC x64), auto-testado sob Wine ✔
