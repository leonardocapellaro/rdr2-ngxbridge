# TEST-RESULTS — evidência de teste (ambiente de desenvolvimento)

Data: 2026-11-07. Ambiente: Fedora 44 (kernel 7.2.13), `x86_64-w64-mingw32-gcc
(GCC) 16.2.1 (Fedora MinGW 16.2.1-1.fc44)`, `g++ (GCC) 16.2.1 (Red Hat)`,
`wine-11.20 (Staging)`. **Sem RDR2 e sem GPU Intel aqui** — os testes abaixo
provam a cadeia de compatibilidade com mocks fiéis ao comportamento real do
OptiScaler e do RDR2VulkanFG (comportamentos verificados diretamente no
código-fonte das duas bases), não o jogo.

## T1 — build Windows x64 real

```
$ bash build.sh
...
OK (0 failures)          <- T2 roda embutido agora
BUILD OK
-rwxr-xr-x. ...   292040 build/mock_game.exe
-rwxr-xr-x. ...   341272 build/mock_mor3a.dll
-rwxr-xr-x. ...  2810574 build/mock_optiscaler.dll
-rwxr-xr-x. ...  3822004 build/zz_ngxbridge.asi
```

Sanidade do artefato (imports; sem nada exótico, DLL normal):

```
$ x86_64-w64-mingw32-objdump -p build/zz_ngxbridge.asi | grep 'DLL Name'
        DLL Name: KERNEL32.dll
        DLL Name: msvcrt.dll
```

## T2 — testes unitários da ABI compartilhada (nativo Linux)

```
$ g++ -O2 -std=c++17 -Isrc tests/test_mirror.cpp src/mirror_core.cpp -o build/test_mirror -pthread
$ ./build/test_mirror
... (casos: Ptr<->ULL, I<->UI, ULL->I faixa, F->Ptr recusado, chave ausente,
     Success+ponteiro-nulo tratado como ausência, Reset, getObject idempotente)
OK (0 failures)
```

## T3 — self-test end-to-end sob Wine (o teste mandatório do briefing §7)

```
$ bash selftest/run_wine.sh
```

Cobre (§7): create/eval NGX, handle "original" vs. falso do OptiScaler,
caminho de módulo, ordem de carga dos ASI, depth/MV válidos, resolução por
nome, espelho de vtable, leitura cruzada de tipo, ausência de input (chave
inexistente → Fail original), reentrância (thread do bridge vs. loader), e
principalmente o exigido: **"mock NGX demonstrando que o observador recebe os
recursos necessários mesmo quando o OptiScaler fornece a implementação de
upscaling"** — `OBSERVER-OK` com ponteiros reais vindos do mock do OptiScaler
sem nenhum NVSDK de verdade no processo.

Transcrição completa (reprodutível; só endereços/timestamps mudam):

```
----- mock_game stdout -----
[GAME   ] == mock game: RDR2 NGX loader simulation ==
[OPTI-MOCK] loaded
[MOR3A-MOCK] loaded
[GAME   ] modules: opti=... mor3a=... bridge=...
[MOR3A-MOCK] gate installed (game IAT GetProcAddress redirect); waiting for NVSDK_NGX_VULKAN_* resolutions
[MOR3A-MOCK] gate name=...CreateFeature provider='Z:\...\build\selftest\nvngx.dll'
[MOR3A-MOCK] RESULT: ACCEPT - installing input-capture wrapper
[MOR3A-MOCK] gate name=...EvaluateFeature provider='Z:\...\build\selftest\nvngx.dll'
[MOR3A-MOCK] RESULT: ACCEPT - installing input-capture wrapper
[GAME   ] resolved alloc=... create=... eval=...
[OPTI-MOCK] AllocateParameters -> param=00000000007c5370
[OPTI-MOCK] dump(obj=00000000007c5370) vptr=... (off=+0x32140)
    slot[ 0..15]=stubs externos   <- vtable espelhada
[MOR3A-MOCK] CreateFeature: SR flags result=1 value=1 MIRROR-CROSS-TYPE-OK
[GAME   ] CreateFeature result=1
[MOR3A-MOCK] OBSERVER depth(result=1 ptr=0000000140009100) motionvectors(result=1 ptr=0000000140009000) OBSERVER-OK
[GAME   ] frame 1 evaluate result=1
[MOR3A-MOCK] OBSERVER depth(result=1 ptr=0000000140009100) motionvectors(result=1 ptr=0000000140009000) OBSERVER-OK
[GAME   ] frame 2 evaluate result=1
[GAME   ] post-Reset provider=1 flags read result=0 STALE-GUARD-OK
----- bridge log -----
== ngxbridge 0.1.1 == enabled=1 spoof=1 mirror=1 target=mock_optiscaler.dll hints=2
target found: Z:\...\build\selftest\mock_optiscaler.dll at ...
hook GetModuleFileNameW: kernel32=...:1 kernelbase=...:1
hook GetModuleFileNameA: kernel32=...:1 kernelbase=...:1
hook observer: ntdll!LdrGetProcedureAddress=... -> 0
mirror: 2/6 parameter-factory export hooks created   (mock só tem os Vulkan; real tem os 6 - verificado no código)
armed: spoof=1 (kernel32 ok), observer=1, factory hooks=2/6
intercepted NVSDK_NGX_VULKAN_AllocateParameters (result=1 obj=00000000007c5370) by mock_game.exe
mirror: vtable ... patched (16 typed slots + Reset) - cross-type reads now answered, stale-after-reset guarded
mirror: coerced typed answer for 'DLSS.Feature.Create.Flags'
mirror: no fallback for 'DLSS.Feature.Create.Flags'    <- pós-Reset: recusa correta de valor antigo
resolved 'NVSDK_NGX_VULKAN_AllocateParameters' from the OptiScaler handle by mock_mor3a.dll -> ...
resolved 'NVSDK_NGX_VULKAN_CreateFeature' from the OptiScaler handle by mock_mor3a.dll -> ...
spoofed provider path for capture module: Z:\...\build\selftest\nvngx.dll
resolved 'NVSDK_NGX_VULKAN_EvaluateFeature' from the OptiScaler handle by mock_mor3a.dll -> ...
spoofed provider path for capture module: Z:\...\build\selftest\nvngx.dll
----- verdict -----
SELFTEST: PASS
```

O que cada linha prova:

| Linha | Prova |
|---|---|
| `RESULT: ACCEPT` ×2 | com o spoof, o gate do Mor3a aceita (sem o bridge ele REJEITA — modo de regressão abaixo) |
| `spoofed provider path` | o spoof dispara só para o chamador-alvo, exatamente 2× (Create+Evaluate) |
| `OBSERVER-OK` ×2 frames | o observador do wrapper vê Depth+MotionVectors **não nulos** do OptiScaler-mock em todos os frames |
| `MIRROR-CROSS-TYPE-OK` + `coerced typed answer` | a leitura cruzada de tipo (int gravado lido por slot "unsigned") só existe por causa do espelho — o provedor puro falharia ali (verificado com Mirror=0: `MIRROR-MISSING(flags)` + `input` inválido) |
| `2/6 ... created` | degradação graciosa quando um export não existe (mock tem só os 2 Vulkan; real tem os 6, verificado no código do 0.9.4) |
| `STALE-GUARD-OK` + `no fallback ...` | depois do `Reset()` do provedor (slot 16), o espelho **recusa** o valor pré-reset — a invariante "nada de frame antigo vale" tem teste próprio |
| `by mock_game.exe` / `by mock_mor3a.dll` | atribuição correta de quem criou o objeto (o jogo) e de quem encaminhou a resolução (o hook global do Mor3a; na produção aparecerá `RDR2VulkanFG.asi` — é exatamente o sinal que a §6.1 do DIAGNOSIS procura) |

## T4 — self-test no Windows

`release/selftest-windows/run_selftest.bat` roda a mesma cadeia em Windows
x64 nativo (sem Wine) e imprime `SELFTEST: PASS/FAIL`. Rodar na B390 **antes**
de instalar no jogo. Nenhuma dependência de runtime (tudo estático).

## Regressões verificadas durante o desenvolvimento

- `Mirror=0` → `MIRROR-MISSING(flags)`: o espelho é realmente necessário,
  não decorativo.
- `Spoof=0` → `RESULT: REJECT` e nenhuma linha `OBSERVER`: reproduz o
  FG-109 de produção (comportamento atual sem o bridge).
- Terceiro engine inline em `GetProcAddress` (descartado por projeto): silencia
  as camadas anteriores — motivo pelo qual o design final não hookiza GPA.

## Limites honestos

Wine ≠ Windows de produção para semânticas de loader muito específicas; por
isso o T4 existe. E nenhum mock substitui o RDR2 real: a hipótese que só o
log do observador na B390 fecha é o módulo chamador de `LdrGetProcedureAddress`
/`GetProcAddress` no jogo (DIAGNOSIS §6). **Nada aqui afirma "funciona no
RDR2"** — afirma que a causa raiz comprovada foi eliminada e mensurável na
máquina real.
