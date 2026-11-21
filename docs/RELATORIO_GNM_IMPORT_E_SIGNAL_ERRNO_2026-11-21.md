# Auditoria estática do import GNM e telemetria de sinais
Data: 2026-11-21
Branch: `feature/gcn8-numeric-kernels`

## Escopo

Investigação somente de artefatos de build, sysroot OpenOrbis v0.5.4 e referências públicas de NID. Nenhuma execução, instalação ou staging no PS4 foi realizada nesta etapa.

## Evidência do stub OpenOrbis v0.5.4

Artefato de auditoria:
- Workflow: https://github.com/demolidorreddragons/NR/actions/runs/44875448709
- Artifact: `ps4-neural-engine-build`, ID `14695697983`
- SHA-256 do ZIP: `e1cfeb30a88f66364ee90515f91cc1139ad4767a92ac4eecde80adf3a5890b47`
- Arquivo: `libSceGnmDriver.so`
- Tamanho: 40.472 bytes
- SHA-256: `21d38ac5e7d79ddc9ee9bc7b9a84547f43ea3144883f6ac787d16704c1b5121f`

O stub exporta `sceGnmSubmitDone` como símbolo dinâmico global `FUNC`, no endereço relativo `0x46cc`. Portanto, o símbolo está presente no provedor de link do sysroot.

Importante: a implementação desse stub não é o driver real. O corpo em `0x46cc` termina em um salto para si mesmo. O arquivo `.so` serve para link/metadata e não deve ser tratado como módulo de runtime. O OELF converte a dependência para o módulo PS4 `libSceGnmDriver.prx`.

## Evidência do OELF realmente gerado

- Run que gerou o OELF: `44875448709`
- Arquivo: `gcn8-smoke/44875448709/gnm-link/smoke.oelf`
- SHA-256: `75e0e13c39b278e2f87f3ec4bfdb916b88823a6e4e97f789d27423b489386b4d`
- Manifest do probe: `hardware_status=not_run`

O OELF contém o segmento `PT_SCE_DYNLIBDATA` no offset de arquivo `0x1dbe8`, tamanho `0x9b0`. Dentro dele foram confirmados:

- Biblioteca: `libSceGnmDriver.prx`
- Módulo: `libSceGnmDriver`
- NID de `sceGnmSubmitDone`: `yvZ73uQUqrk#C#C`
- O mesmo segmento contém a dependência `libkernel.prx` / `libkernel`.
- A tabela dinâmica contém `DT_SCE_STRTAB`, `DT_SCE_IMPORT_LIB` e `DT_SCE_IMPORT_LIB_ATTR` para as dependências.

A base pública de NIDs do PS4 `orbisLibGen` associa:
- nome: `sceGnmSubmitDone`
- NID hexadecimal: `CAF67BDEE414AAB9`
- NID codificado: `yvZ73uQUqrk`
- biblioteca: `libSceGnmDriver`

Referência: https://github.com/CrazyVoidProgrammer/orbisLibGen/blob/50b2a0ccc652799a404090b3da1e3e769fa6b1dd/ps4libdoc/system/common/lib/libSceGnmDriver.sprx.json

### Interpretação

O NID produzido no OELF coincide com o NID da referência pública, e o nome da biblioteca/módulo no segmento SCE coincide com `libSceGnmDriver` / `libSceGnmDriver.prx`.

Assim, **não há evidência de erro de grafia do símbolo, NID calculado incorretamente ou nome de módulo errado no OELF gerado**. A hipótese 1, na forma de incompatibilidade estática entre o nome/NID do sysroot e o OELF, fica fortemente enfraquecida.

Limite da conclusão: a referência pública de NID não é uma extração direta do firmware 14.02 deste console. Ainda não prova que o módulo instalado nesse firmware exporta esse NID, nem que o loader conseguiu carregar/resolver o módulo em runtime.

Nota de ferramenta: `readelf -d` interpreta os `DT_NEEDED` do OELF usando a string table ELF genérica e mostra textos aparentemente truncados. O OELF usa `DT_SCE_STRTAB` próprio; a leitura correta deve considerar o segmento `PT_SCE_DYNLIBDATA`, não as strings exibidas pelo decodificador ELF genérico.

## Hipótese seguinte: módulo/export em runtime

O cabeçalho OpenOrbis v0.5.4 declara `sceKernelLoadStartModule` e `sceKernelGetModuleInfo` em `include/orbis/libkernel.h`. Isso torna possível investigar carregamento/estado de módulo, mas não prova que `libSceGnmDriver.prx` precise de carregamento explícito: o OELF já declara a dependência por metadata SCE.

A API `sceKernelInstallExceptionHandler` não foi encontrada no cabeçalho `libkernel.h` nem na busca do repositório OpenOrbis v0.5.4. Não será usada como fallback sem evidência de export e ABI.

## Instrumentação de errno para os handlers

A implementação anterior retornava apenas `-1`, sem distinguir qual operação falhou. Foi alterada para registrar:

- `SIGNAL_INSTALL_BEGIN`
- `SIGNAL_ABI`: `sizeof(stack_t)`, offsets de `ss_sp`, `ss_size`, `ss_flags` e tamanho da pilha alternativa
- resultado e errno de `sigaltstack`
- resultado e errno de `sigemptyset`
- resultado e errno de cada `sigaction`, com número do sinal
- `SIGNAL_INSTALL_COMPLETE` somente depois de todas as operações concluírem

O errno é copiado imediatamente após a chamada que falhou, antes de qualquer escrita de log. Isso evita que o logger sobrescreva o valor relevante.

Commits:
- `9252591` — primeira instrumentação
- `5bab295` e `56e14cb` — correções de formatação das linhas de diagnóstico
- `0a41d20` — asserts dos novos marcadores no teste host
- `1f68064` — inclusão de `stddef.h` para os offsets ABI

Host tests: execução `44875885489`, job `telemetry`, concluído com sucesso. A compilação OpenOrbis v0.5.4 também passou: no workflow `44875885487`, os passos `Build OpenOrbis Startup-Only diagnostic PKG` e `Build OpenOrbis GNM-Link-Only diagnostic PKG` concluíram com sucesso, ambos compilando o `stage_log.c` atualizado. Isso valida a compilação no toolchain, não a execução no hardware.

## Próximos passos

1. Concluir a compilação OpenOrbis e confirmar que a instrumentação compila no target.
2. Na próxima coleta autorizada, ler a linha `SIGNAL_DIAG` que falhar e o layout `SIGNAL_ABI`.
3. Em paralelo, verificar no firmware alvo se `libSceGnmDriver.prx` está carregado e se exporta o NID esperado; usar `sceKernelGetModuleInfo`/carregamento explícito somente depois de validar assinatura, NID e semântica.
4. Não fazer novo teste de dispatch/shader enquanto a resolução de import e a inicialização do logger permanecerem sem explicação.

Nenhum pacote foi instalado ou enviado ao PS4 nesta auditoria.
