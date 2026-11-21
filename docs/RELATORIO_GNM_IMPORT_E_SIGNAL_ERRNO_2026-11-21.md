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

## Resultado do build do probe dinâmico

O novo modo `gnm-load` compilou com o OpenOrbis v0.5.4 no workflow:
https://github.com/demolidorreddragons/NR/actions/runs/44876293567

O job `build-prx-and-pkg` concluiu com sucesso. Artefato:
- ID: `14695779072`
- SHA-256 do ZIP: `174b7d8b7d9da1c08ae6aaa9ebea46d62369d299143f3af068cf8dbec19c196f`

Manifest do pacote:
- PKG SHA-256: `bc5cdd06e73578281c66498decab293abc907043488b8a1dc351c2c5001cc9a6`
- ELF SHA-256: `7b742abd43a625042778d4335d1c121414ae3c1786fc75f4cb3c92ceb4e73534`
- OELF SHA-256: `e9187a023918474a1291ff543c5fe2932cb26e7532d80bcd6c7a21077c93d399`
- `gnm_import_probe=none`
- `gnm_load_probe=sceKernelLoadStartModule_then_sceKernelDlsym_no_gnm_call`
- `gnm_function_calls=none`
- `hardware_status=not_run`

A inspeção do OELF confirma somente a dependência SCE `libkernel.prx` / módulo `libkernel`, sem import estático de `libSceGnmDriver.prx`. A tabela de NIDs contém:
- `IuxnUuXk6Bg#B#B` — `sceKernelGetModuleList`
- `kUpgrXIrz7Q#B#B` — `sceKernelGetModuleInfo`
- `JGfTMBOdUJo#B#B` — `sceKernelGetFsSandboxRandomWord`
- `wzvqT4UqKX8#B#B` — `sceKernelLoadStartModule`
- `LwG8g3niqwA#B#B` — `sceKernelDlsym`

O nome `libSceGnmDriver` aparece no executável apenas como texto usado para procurar/carregar o PRX; não aparece como biblioteca importada na tabela SCE. O OELF também não contém o NID estático `yvZ73uQUqrk#C#C` do probe anterior.

A validação independente terminou com sucesso no job `independent-pkg-qa` do workflow `44876293567`. Artifact QA ID `14695194744`, SHA-256 `f7f7efe3cd866c67423088d6997660153d4e164475de214eec9856d55fdaf7ba`. Resultado: `GNM_LOAD_SFO=PASS; STATIC_GNM_IMPORT=ABSENT; COMPUTE_SHADER=ABSENT`; `GNM_LOAD_QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`. O host-tests do commit com o probe e a instrumentação passou na execução `44876368025`. Nenhum staging ou instalação foi realizado.

## Próximos passos

1. Concluir a compilação OpenOrbis e confirmar que a instrumentação compila no target.
2. Na próxima coleta autorizada, ler a linha `SIGNAL_DIAG` que falhar e o layout `SIGNAL_ABI`.
3. Em paralelo, verificar no firmware alvo se `libSceGnmDriver.prx` está carregado e se exporta o NID esperado; usar `sceKernelGetModuleInfo`/carregamento explícito somente depois de validar assinatura, NID e semântica.
4. Não fazer novo teste de dispatch/shader enquanto a resolução de import e a inicialização do logger permanecerem sem explicação.

Nenhum pacote foi instalado ou enviado ao PS4 nesta auditoria.


## Staging autorizado do GNM-Load-Only

O usuário autorizou explicitamente o staging, sem instalação. Execução:
https://github.com/demolidorreddragons/NR/actions/runs/44876962323

Resultado do job `stage-diagnostic`: `completed / success`.

Verificações realizadas pelo runner PS4:
- Download dos artefatos de build e QA com digest SHA-256 conferido.
- QA independente validado antes de qualquer alteração no console.
- PKG anterior GNM-Link-Only movido para `/data/pkg/old/` e readback confirmado:
  - `PS4-Neural-Rendering-GCN8-gnm-link-44873622760.pkg`
  - 6.619.136 bytes
  - SHA-256 `e68da9f35981aa32e788ebe3f0461360167c0194b0c408c47c945745ebab5847`
- Novo GNM-Load-Only enviado para arquivo temporário fora de `/data/pkg/`, readback verificado e promovido por rename.
- Destino final: `/data/pkg/PS4-Neural-Rendering-GCN8-gnm-load-44876293567.pkg`
- Tamanho remoto: 6.619.136 bytes
- SHA-256 remoto: `bc5cdd06e73578281c66498decab293abc907043488b8a1dc351c2c5001cc9a6`
- Exatamente um PKG Neural Rendering permaneceu na raiz de `/data/pkg/`.
- Pacotes de terceiros preservados.
- `INSTALLATION_PERFORMED=FALSE`.

O staging foi concluído. O pacote está disponível no PS4, mas não foi instalado nem executado. O próximo resultado necessário é a execução manual do GNM-Load-Only e a coleta do log atualizado; não inferir sucesso de carregamento até obter `GNM_LOAD`/ `GNM_DLSYM` do console.
