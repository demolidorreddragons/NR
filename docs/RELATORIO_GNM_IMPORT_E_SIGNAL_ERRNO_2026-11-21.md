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

## Evidência independente para carregamento explícito do GNM

Foi encontrada uma implementação de probe GNM no projeto DolphinPS4:
https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/probe-gnm/main.cpp

O código documenta que chamar uma biblioteca de sistema cujo módulo ainda não foi carregado pode terminar o app com `PRX_NOT_RESOLVED_FUNCTION`. Para o GNM, ele:
1. consulta `sceKernelGetModuleList` e `sceKernelGetModuleInfo`;
2. procura `libSceGnmDriver` já carregado;
3. se não estiver carregado, usa `sceKernelGetFsSandboxRandomWord` para montar `/<word>/common/lib/libSceGnmDriver.sprx`;
4. tenta `sceKernelLoadStartModule`;
5. resolve `sceGnmSubmitDone` por `sceKernelDlsym`, sem depender de import direto no executável.

O comentário desse projeto relata que os caminhos relativos e `/system/common/lib/...` retornaram `0x80020002 (ENOENT)` em versões anteriores do probe, justificando testar primeiro o caminho com sandbox word. Isso é evidência externa relevante, mas não substitui um resultado do nosso PS4.

No OpenOrbis v0.5.4, `include/orbis/libkernel.h` declara `sceKernelGetModuleList`, `sceKernelGetModuleInfo`, `sceKernelGetFsSandboxRandomWord`, `sceKernelLoadStartModule` e `sceKernelDlsym`. A base pública de NIDs confirma:
- `sceKernelLoadStartModule`: `wzvqT4UqKX8`
- `sceKernelDlsym`: `LwG8g3niqwA`
- `sceKernelGetModuleList`: `IuxnUuXk6Bg`
- `sceKernelGetModuleInfo`: `kUpgrXIrz7Q`
- `sceKernelGetFsSandboxRandomWord`: `JGfTMBOdUJo` (referência etaHEN NID table)

Isso torna a hipótese de módulo não carregado mais forte que a hipótese de NID incorreto no OELF.

Foi adicionado o modo diagnóstico `gnm-load`, reaproveitando o Title ID `PNEU00004` do probe anterior. Ele não importa `libSceGnmDriver`, não chama nenhuma função GNM, consulta módulos, tenta carregar o PRX pelo caminho sandboxed e resolve `sceGnmSubmitDone` por DLSYM. Mesmo se o carregamento falhar, registra o código e termina com retorno zero para não confundir a falha de diagnóstico com um encerramento anormal do aplicativo. O pacote ainda não foi enviado nem instalado no console.

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

Host tests: execução `44875885489`, job `telemetry`, concluído com sucesso. A versão inicial da telemetria errno compilou no OpenOrbis v0.5.4 no workflow `44875885487` (Startup-Only e GNM-Link-Only). A variante atual, que mantém handlers sem `SA_ONSTACK` se a pilha alternativa falhar e inclui o novo modo `gnm-load`, está sendo recompilada no workflow `44876293567`. Isso valida compilação no toolchain, não execução no hardware.

## Próximos passos

1. Concluir a compilação OpenOrbis e confirmar que a instrumentação compila no target.
2. Na próxima coleta autorizada, ler a linha `SIGNAL_DIAG` que falhar e o layout `SIGNAL_ABI`.
3. Em paralelo, verificar no firmware alvo se `libSceGnmDriver.prx` está carregado e se exporta o NID esperado; usar `sceKernelGetModuleInfo`/carregamento explícito somente depois de validar assinatura, NID e semântica.
4. Não fazer novo teste de dispatch/shader enquanto a resolução de import e a inicialização do logger permanecerem sem explicação.

Nenhum pacote foi instalado ou enviado ao PS4 nesta auditoria.
