# Registro de correção — GoldHEN PRX CRT no CI

**Data:** 2026-11-21  
**Repositório:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Correção do workflow:** `5de9a7a3fd244538fc7e38b97a18b83b0cd6fbbd`

## Incidente

Na execução CI `44867653456`, o job `build-prx-and-pkg` falhou na etapa
`Build and verify pinned GoldHEN SDK PRX CRT object`.

Log observado:

```text
mkdir: cannot create directory ‘build’: File exists
make: *** [Makefile:95: crt] Error 1
```

## Causa raiz

O Makefile do SDK GoldHEN fixado em
`bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7` define o alvo `crt` com
`mkdir build` (sem `-p`). O diretório `build` já existia quando o alvo
foi chamado. A falha ocorria antes da compilação de `crt/crtprx.c`.

Isto não era falha de ABI, nem falta do fonte CRT, nem problema de caminho
do linker. O objeto deve ser gerado a partir de `crt/crtprx.c`.

## Correção aplicada

O workflow deixou de chamar `make -C GoldHEN_Plugins_SDK crt` e agora
reproduz diretamente os parâmetros de compilação do alvo upstream, com
preparação idempotente do diretório:

```sh
SDK="$GITHUB_WORKSPACE/GoldHEN_Plugins_SDK"
CRT="$SDK/build/crtprx.o"
mkdir -p "$SDK/build"
clang -target x86_64-pc-linux-gnu -ffreestanding -nostdlib -fno-builtin -fPIC \
  -isysroot "$OO_PS4_TOOLCHAIN" \
  -isystem "$OO_PS4_TOOLCHAIN/include" \
  -c "$SDK/crt/crtprx.c" -o "$CRT"
```

A etapa também exige arquivo não vazio, valida ELF64 e máquina x86-64 com
`readelf`, e registra SHA-256. Os parâmetros de compilação upstream foram
preservados; a única mudança operacional é tornar a criação do diretório
idempotente.

## Prevenção de recorrência

1. Não invocar o alvo `crt` desse Makefile em um checkout onde `build/`
   possa existir previamente.
2. Manter `mkdir -p` antes da compilação.
3. Preservar os flags e o target do SDK pinado; mudanças nesses parâmetros
   exigem revisão específica de ABI/toolchain.
4. Validar presença, ELF class, machine e SHA-256 do objeto antes da linkagem.
5. A etapa de linkagem GNM permanece bloqueada até o CRT passar.
6. Não considerar CI host como prova de carregamento PRX, execução firmware
   ou dispatch GPU no PS4.

## Evidência e estado

- Execução que revelou a causa: [CI 44867653456](https://github.com/demolidorreddragons/NR/actions/runs/44867653456)
- Host tests da rodada: [CI 44867653459](https://github.com/demolidorreddragons/NR/actions/runs/44867653459)
- Compilador GCN8: [CI 44867653389](https://github.com/demolidorreddragons/NR/actions/runs/44867653389)
- Workflow corrigido: [ps4-prx.yml](https://github.com/demolidorreddragons/NR/blob/feature/gcn8-numeric-kernels/.github/workflows/ps4-prx.yml)

**Estado da correção:** registrada no commit acima; aguarda execução CI para
confirmar geração do CRT, linkagem GNM, PRX, PKG e QA independente.

**Não declarar aprovado até os gates executarem com sucesso. Nenhuma
instalação ou alteração no console foi realizada.**


## Reincidência identificada na rodada seguinte — escopo de variável no linker

A rodada do commit `5de9a7a3fd244538fc7e38b97a18b83b0cd6fbbd` confirmou que a geração/verificação de `GoldHEN_Plugins_SDK/build/crtprx.o` passou e que o smoke de compilação da API/ABI GNM passou. A etapa seguinte, porém, falhou no linker porque o comando ainda usava `$GOLDHEN_SDK/build/crtprx.o`; essa variável não estava definida no ambiente daquele step e o shell resolveu o caminho como `/build/crtprx.o`.

Correção aplicada no workflow: o linker agora usa o caminho explícito e estável `$GITHUB_WORKSPACE/GoldHEN_Plugins_SDK/build/crtprx.o`, alinhado ao caminho usado pela etapa de geração. Isso elimina dependência de variável de ambiente fora do seu escopo. Commit corretivo: `119bda23c9abbbf224e694b600bac7959543c3fb`.

A falha impediu a execução das etapas seguintes; PRX final, PKG e QA independente continuam não produzidos/não executados nesta rodada. A nova execução disparada pelo push deverá confirmar a correção. Não houve instalação nem execução no PS4.
