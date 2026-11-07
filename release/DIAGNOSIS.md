# Diagnóstico — por que o XeFG nunca ligou no RDR2 Vulkan (e o que esta ponte faz)

Máquina de destino: Win11, Intel Arc B390, RDR2 Epic 1.0.1491.50, 3440×1440,
renderizador **Vulkan nativo**, OptiScaler 0.9.4 + OptiPatcher +
rdr2-vulkan-frame-generation (Mor3a 1.0 beta, binário de produção
`RDR2VulkanFG.asi`) + loader ASI dinput8 + fakenvapi.

## 1. O que os logs da máquina mostravam

Dois arquivos em `case_files/` (watchdog e log do RDR2VulkanFG):

- `wd: evals=0` — o contador de `EvaluateFeature` do OptiScaler nunca subiu
  para o wrapper de inputs;
- `Early NGX export routing commit=0` — **isto não é erro**: `commit=0` é o
  `NO_ERROR` do `DetourTransactionCommit` (inputs.cpp:677). O hook do
  `GetProcAddress` do RDR2VulkanFG foi instalado com sucesso. O que falta é
  o que ele deveria ter visto passar;
- telemetria `input=0 requested=0 enabled=0 displayed=1` — o RDR2VulkanFG
  nunca recebeu input de SR; o FG ficou desabilitado → código **FG-109**;
- OSD `Vulkan | FSR 2.2.1 | Input: DLSS` — explicado na seção 4.

## 2. Causa raiz (comprovada por código + logs)

A cadeia NGX do jogo com OptiScaler ativo:

1. O RDR2 chama `LoadLibrary("nvngx.dll")`. O OptiScaler hookiza
   `LdrLoadDll`/`NtLoadDll` e **responde com o próprio HMODULE** — um "carregamento
   falso", nenhum `nvngx.dll` chega a ser mapeado
   (`OptiScaler/OptiScaler/hooks/LibraryLoad_Hooks.cpp:66-75`,
   `EnableDlssInputs`).
2. O jogo resolve `NVSDK_NGX_VULKAN_*` com `GetProcAddress(eseloghandle, ...)`.
   O OptiScaler hookiza `GetProcAddress` (kernel32: `Kernel_Hooks.cpp:111-146`;
   kernelbase: `:257-275`) e serve suas próprias implementações. O
   **upscaling funciona** — é por isso que o jogo roda com SR.
3. O RDR2VulkanFG também hookiza `GetProcAddress` (via MS Detours,
   `rdr2-vulkan-frame-generation/src/inputs.cpp:675`). No seu gate
   `h_EarlyNgxGetProc` (inputs.cpp:638-669), quando alguém resolve
   `NVSDK_NGX_VULKAN_{CreateFeature,CreateFeature1,EvaluateFeature}`, ele
   pergunta **de que arquivo vem o módulo provedor** com
   `GetModuleFileNameW(handle)` e só instala o wrapper de captura de inputs
   se a resposta for exatamente `nvngx.dll` ou `_nvngx.dll` (a mesma
   verificação é repetida em `NgxProbeTick`, inputs.cpp:696-732).
4. Como o handle devolvido no passo 1 é o do **OptiScaler.asi**, a resposta é
   `...\OptiScaler.asi` → **REJECT silencioso** → nenhum wrapper é instalado
   → `evals=0` → nenhum `Depth`/`MotionVectors`/flags chega ao XeFG →
   `input=0 enabled=0` → FG-109.

O ponto 4 é a causa raiz. Os logs fecham o circuito: o hook do RDR2VulkanFG
está vivo (`commit=0`), mas nunca vê os três nomes serem resolvidos — porque
o gate que ele aplicaria rejeitaria, e de fato nunca é ativado
(nenhum `NGX hooks: module=...` / `FG input pipeline v2: accepted SR handle=`
aparece nos logs do caso).

### 2.1 A discrepância dos módulos "nvngx.dll" (pedida para investigar sem assumir)

Concluída e **não é outra causa**: o OptiPatcher (verificado no código-fonte)
é um binário *pattern-patcher* puro — `VirtualProtect`+`memcpy`, zero hooks,
zero injeção; ele só NOPa gates de vendor dentro do RDR2.exe em memória e o
OptiScaler o executa como plugin via loader ASI. Ele **não** interfere no
`LoadLibrary("nvngx.dll")` do jogo: o que devolve o handle falso é o hook
`LdrLoadDll` do próprio OptiScaler. Portanto não há dois "nvngx" disputando;
há um único provedor (OptiScaler) cuja resposta de nome de arquivo o gate do
RDR2VulkanFG não reconhece.

## 3. Por que o OptiScaler "já serve DLSS" mas o FG não liga — e o ABI reverso

Mesmo se o gate aceitasse, há um segundo abismo de ABI que a ponte também
resolve. O RDR2VulkanFG lê o objeto de parâmetros NGX por um **mapa de slots
de vtable fixo e com grupos invertidos** (inputs.cpp:90-103:
`GetVoidPointer→slot 8`, `GetF→14`, `GetUI→12`, `GetI→11`, `SetI→3`,
`SetF→6`). A vtable real (cabeçalho `nvsdk_ngx_params.h` v0.9.4) é:

```
Set: ULL=0 F=1 D=2 UI=3 I=4 D3D11=5 D3D12=6 void*=7
Get: ULL=8 F=9 D=10 UI=11 I=12 D3D11=13 D3D12=14 void*=15   Reset=16
```

- slot 8: o mapa reverso chama `Get(void**)`; o slot real é `Get(ULL*)`.
  Atravessa porque o OptiScaler converte `void*↔ULL` nos dois sentidos — o
  **bridge garante a resposta mesmo quando um slot recusa estritamente**;
- slots 11/12: `int` ↔ `unsigned` — leitura cruzada que o provedor recusa
  (`DLSS.Feature.Create.Flags`, `Width/Height/OutWidth/OutHeight`);
- slot 14: o mapa reverso chama `GetF`; o slot real é `Get(ID3D12Resource**)`
  — **limitação conhecida** (seção 6).

## 4. O `Vulkan | FSR 2.2.1 | Input: DLSS`

Não é contradição nem DX12 às escondidas:

- `Input: DLSS` = o OptiScaler registrou que a **ponte de input NGX** foi
  usada (o jogo falou a API DLSS-NGX; quem serve é o OptiScaler). Não indica
  o upscaler ativo;
- `FSR 2.2.1`: no caminho Vulkan, o OptiScaler tenta o XeSS-SR **exatamente**
  via `libxess.dll` contendo as exportações `xessVK*` (um só arquivo;
  `XeSS_Proxy.h:212-250`, `DllNames.h`). Se o arquivo não está lá/não exporta
  `xessVK*`, cai silenciosamente no FSR 2.2.1 embutido
  (`NVNGX_DLSS_Vk.cpp:722-741` + fallback do provedor). A chave correta é
  `[Upscalers] VulkanUpscaler=xess` — em `[General]` ela é **ignorada**
  (a amostra `OptiScaler.ini` não tem nenhuma seção `[General]`).
- Para XeSS-SR de verdade na B390: colocar um `libxess.dll` **do seu próprio
  sistema/pacote Intel** (com exportações `xessVK*`) na pasta do jogo. Não
  redistribuímos esse arquivo (licença Intel). O FG do XeFG independe disso —
  com FSR 2.2.1 como SR, o XeFG 2x pode ligar normalmente (é o modo
  "FSR-SR + XeFG" aceito no briefing como intermediário).

## 5. O que a ponte (zz_ngxbridge.asi) faz — e o que NÃO faz

Três mecanismos, cada um na única função que ainda não tinha dono na pilha:

1. **Spoof do caminho do provedor (Spoof=1)** — hook de
   `GetModuleFileNameW/A` (kernel32 + kernelbase + o próprio IAT do bridge).
   Só responde diferente quando (a) o handle perguntado é o do OptiScaler **e**
   (b) o código perguntante pertence a um módulo cujo caminho contém um dos
   `CallerHints` (default `rdr2vulkanfg`). Responde
   `<pasta do jogo>\nvngx.dll` — um caminho **fictício, nenhum arquivo é
   carregado ou precisa existir**. É só o suficiente para o gate do
   RDR2VulkanFG aceitar; nada mais muda no comportamento do jogo ou do
   OptiScaler. Perguntas de qualquer outro módulo passam batidas (contador +
   log).
2. **Espelho tipado da vtable (Mirror=1)** — hook dos **6 exportadores de
   fábrica de parâmetros** do OptiScaler (`NVSDK_NGX_{VULKAN,D3D12,D3D11}_
   {AllocateParameters,GetParameters}`; verificados os 6 nomes no código-fonte
   do 0.9.4). Quando o jogo cria o objeto, o bridge patcheia os 16 slots da
   vtable dele (VirtualProtect + escreve stubs). Cada stub: (a) chama o slot
   original do provedor; (b) se o provedor recusar (ou devolver Success com
   ponteiro nulo em slot de ponteiro), aplica **fallback tipado seguro**:
   `void*↔ULL`, `int↔unsigned`, `ULL→int` com checagem de faixa, `int→float`
   só se finito/inteiro. **Nunca** fabrica ponteiro a partir de float,
   nunca inventa valor, nunca responde frame antigo: se não existe valor
   válido do frame atual, devolve o mesmo Fail que o provedor daria.
3. **Observador (log) da resolução de nomes** — hook de
   `ntdll!LdrGetProcedureAddress`, que **ninguém mais hookiza** na pilha
   (OptiScaler hookiza LdrLoadDll/NtLoadDll/GetProcAddress; Mor3a hookiza
   GetProcAddress + exportações NGX; OptiPatcher não hookiza nada). Registra
   qual módulo pediu qual nome NGX ao handle do OptiScaler — responde na
   máquina real a última hipótese em aberto (seção 6).

**Não faz:** hook de `GetProcAddress` (já pertence a OptiScaler **e** Mor3a;
um terceiro engine no mesmo endereço — medido — silencia ambos), hook de
`LdrLoadDll` (não precisa), file `nvngx.dll` em disco (não precisa nem é
desejável), patch em RDR2.exe/DRM (proibido e desnecessário), alteração de
OptiScaler/Mor3a/OptiPatcher (nenhum byte tocado). Todos os hooks usam um
único MinHook estático; o bridge não expõe superfície para outros hookarem.

## 6. O que está provado aqui, o que só a B390 prova

**Provado neste ambiente (Fedora 44 + MinGW + Wine 11.20, sem RDR2):**
cadeia completa de eventos com mocks fiéis (ver `TEST-RESULTS.md`): spoof
entregue → gate aceita → wrapper instalado → observador vê `Depth`/
`MotionVectors` reais no `EvaluateFeature` dos dois frames → leitura cruzada
de tipo salva pelo espelho (`MIRROR-CROSS-TYPE-OK`). Testes unitários da
lógica de coerção: verde. Compilação Windows x64 real do `.asi`.

**A provar na B390 (sem atalho honesto):**
1. *Caminho de resolução do jogo:* o gate do Mor3a só existe se a resolução
   `GetProcAddress` do jogo passar pelo hook dele. O observador registra o
   módulo chamador de cada resolução (`resolved '...' by <modulo>`); se
   aparecer `kernel32.dll`/`kernelbase.dll`, a cadeia do Mor3a vê o mesmo
   evento e o spoof funciona. Se aparecer outro módulo, o log prova e a
   correção é pontual (outro ponto de observação) — até lá **não afirmamos
   "funciona no RDR2"**, apenas "a causa raiz foi eliminada no ponto
   comprovado".
2. *Jitter:* o `GetF` reverso do Mor3a (slot 14 = real `Get(D3D12**)`) não
   consegue ler os floats de `Jitter.Offset.X/Y` gravados pelo jogo no slot
   real de float — o espelho **se recusa** a responder float como ponteiro
   (seria um valor legítimo entregue num tipo falso; arriscado para
   qualquer outro leitor do slot). Consequência esperada: FG liga com
   depth+MV+flags corretos, mas `jitter=(0.0000,0.0000)` no log do Mor3a
   (reprojeção sem compensação de jitter — qualidade pode ficar aquém do
   ideal). Follow-up seguro e pequeno: permitir essa coerção **apenas** para
   as duas chaves de jitter conhecidas, com flag de ini. Documentado, não
   implementado (mínimo necessário para ligar o FG primeiro).
3. Validação final de performance/estabilidade na B390 (fora do escopo deste
   ambiente de desenvolvimento, por construção).

## 7. Critério de sucesso na máquina real

Na ordem, nos logs:

1. `RDR2VulkanFG\ngxbridge.log`: `spoofed provider path for capture module: ...nvngx.dll`;
2. `RDR2VulkanFG\RDR2VulkanFG.log`: `NGX hooks: module=... commit=0 ...`
   (o probe aceitou; antes nunca aparecia);
3. `FG input pipeline v2: accepted SR handle=... flags=0x... render=... output=...`;
4. `NGX inputs first valid: depth ... mvec ... jitter ... mvScale ...`;
5. telemetria: `input=1 ... enabled=1`; `wd: evals>0`; FG-109 some; 2x
   frames reais no OSD do XeFG.
