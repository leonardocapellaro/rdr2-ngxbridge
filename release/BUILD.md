# BUILD — como o `zz_ngxbridge.asi` foi (e pode ser) compilado

## No Fedora (ambiente usado) — MinGW-w64

```
sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils wine
# deps do wine: sudo dnf install --assumeyes wine
```

```
cd case/ngxbridge
bash build.sh          # build cruzado Windows-x64 + unit tests (g++ nativo)
bash selftest/run_wine.sh
```

Saídas: `build/zz_ngxbridge.asi` (o artefato do release), os mocks do
self-test, `build/test_mirror`, `build/zz_ngxbridge.imports.txt`.

Notas do toolchain:

- **MinGW 16.2 emite a ABI MSVC x64** — nada de MSVC é obrigatório; o briefing
  pedia CI que *realmente compile*, e é este o pipeline usado. O código é
  C++17 limpo (sem SEH, sem `__try`, sem `<excpt.h>`; `__declspec(dllexport)`
  não é usado no bridge; os casts FARPROC↔tipado são os únicos pontos que
  pedem `-Wno-cast-function-type`), então abrir a solução em MSVC depois e
  compilar `src/*.cpp` como DLL x64 com MinHook também funciona — mas o
  binário **entregue** é o deste script, não um "compilaria-se".
- `mirror_core` é compilado nos dois mundos (MinGW e g++ nativo p/ testes).
- MinHook 1.3.3 **vendorizado** em `case/vendor/minhook-1.3.3` (MIT, licença
  inclusa em `THIRD-PARTY-NOTICES.txt`); link estático — nenhum DLL extra.

## Arquivos-fonte

| Arquivo | Conteúdo |
|---|---|
| `src/ngxbridge.cpp` | DllMain → config → init thread (procura target) → hooks (GPFN W/A k32+kb+self-IAT, observador LdrGPA, 6 fábricas de parâmetros) + espelho por objeto |
| `src/mirror_core.{h,cpp}` | ABI versionada (`MIRROR_ABI_VERSION`), mapa de slots real vs. reverso, `MirrorCoerce` defensiva, espelho mutex'd |
| `tests/test_mirror.cpp` | unit tests nativos |
| `mock/*`, `selftest/*` | self-test (nunca instalados no jogo) |

## CI opcional (GitHub Actions, esboço)

```yaml
- name: Build (MinGW on ubuntu)
  run: |
    sudo apt-get update && sudo apt-get install -y mingw-w64
    bash build.sh
- name: Native unit tests
  run: ./build/test_mirror
```

(O self-test Wine não roda no runner Ubuntu sem `--enable-interactive`; use
`selftest-windows/run_selftest.bat` num runner/VM Windows para cobertura
nativa — é por isso que o bundle Windows existe.)

## Reprodutibilidade

Os binários do `release/` são exatamente os produzidos por `build.sh` na data
de `SHA-256SUMS.txt`. Não há blob pré-compilado sem código-fonte correspondente
no repositório.
