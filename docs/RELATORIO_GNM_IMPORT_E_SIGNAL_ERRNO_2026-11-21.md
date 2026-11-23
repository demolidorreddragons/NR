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


## Resultado real no PS4: GNM-Load-Only (21/11/2026)

Coleta feita pelo runner PS4, sem instalação ou alteração no console:
- Workflow de coleta: https://github.com/demolidorreddragons/NR/actions/runs/44919921684
- Artifact: `ps4-crash-logger-evidence`, ID `14713467252`
- SHA-256 do ZIP: `f930172f94f787f24ff6dbd6aef5e75a18979a3d2ea4bfd119bb91d40fe1d0f2`
- Log do probe: `/data/pne_gcn8_compute_smoke_44876293567.log`
- Tamanho: 1.484 bytes
- SHA-256: `a273ac0a9ef042efeabcb6db18a2a172cc77c2ce726ae3bb05b98c28608df791`

Trechos decisivos observados no hardware:
```
STAGE=GNM_MODULE_LIST rc=0x00000000 count=5
STAGE=GNM_MODULE_ENTRY name=eboot.bin handle=0x00000000
STAGE=GNM_MODULE_ENTRY name=libkernel_sys.sprx handle=0x00002001
STAGE=GNM_MODULE_ENTRY name=libSceLibcInternal.sprx handle=0x00000002
STAGE=GNM_MODULE_ENTRY name=libSceSysmodule.sprx handle=0x00000012
STAGE=GNM_MODULE_ENTRY name=libkernel.sprx handle=0x00000014
STAGE=GNM_SANDBOX_WORD=5T3geF0DJY
STAGE=GNM_LOAD path=/5T3geF0DJY/common/lib/libSceGnmDriver.sprx rc=0x00000015
STAGE=GNM_DLSYM symbol=sceGnmSubmitDone rc=0x00000000 address=0x800319720
STAGE=RESULT_PASS_GNM_DYNAMIC_LOAD_AND_DLSYM
STAGE=PNE_GCN8_SMOKE_EXIT=0
```

### Conclusões confirmadas

1. O aplicativo chegou ao `main`; o probe não morreu antes da inicialização do processo.
2. A enumeração inicial mostrou somente cinco módulos e **não** mostrou `libSceGnmDriver`.
3. O caminho sandboxed `/<word>/common/lib/libSceGnmDriver.sprx` foi aceito por `sceKernelLoadStartModule`, retornando handle positivo `0x15`.
4. `sceKernelDlsym` resolveu `sceGnmSubmitDone` com retorno zero e endereço não nulo `0x800319720`.
5. O probe terminou com código zero. Não há coredump presente no diretório raiz `/data/sce_coredumps/` na coleta posterior.

**Resultado:** a hipótese prioritária agora é confirmada: no estado inicial observado, o módulo GNM não estava carregado; carregá-lo explicitamente e resolver o símbolo por DLSYM funciona no firmware/console testado. Isso explica por que depender exclusivamente da relocação/import estática não era um teste confiável de disponibilidade do driver. Não prova ainda que todos os símbolos necessários ao compute estejam exportados nem que a fila/dispatch funcionem.

### Sinais e telemetria

- `sigaltstack` retornou `-1`, `errno=22` (EINVAL) no ABI `stack_t` apresentado pelo OpenOrbis.
- O fallback sem `SA_ONSTACK` funcionou: `sigaction` retornou zero para SIGSEGV, SIGBUS, SIGILL, SIGABRT e SIGFPE; o logger registrou `SIGNAL_INSTALL_COMPLETE_NO_ALTSTACK`.
- Portanto, o handler de sinal foi instalado, mas sem pilha alternativa. A divergência do ABI da pilha alternativa continua aberta.
- O arquivo contém alguns trechos de texto corrompidos/intercalados nas linhas iniciais da telemetria. Os marcadores GNM posteriores estão legíveis e coerentes; a coleta binária deve ser preservada para futura análise do logger.

### Próxima etapa técnica

1. Não repetir o teste de carga: ele já passou no hardware.
2. Criar uma camada de runtime GNM que carregue o módulo uma vez e resolva por DLSYM, com assinatura PS4_SYSV_ABI conferida, todos os exports usados pelo compute: pelo menos `sceGnmDispatchInitDefaultHardwareState`, `sceGnmFlushGarlic`, `sceGnmSubmitCommandBuffers`, `sceGnmSubmitDone` e `sceGnmBufSetMemoryType` (confirmar se este último é função exportada ou helper inline no OpenGNM).
3. Fazer um modo de auditoria que apenas resolva e reporte todos esses endereços, sem chamar GNM. Exigir que todos estejam presentes antes de habilitar o caminho compute.
4. Remover do executável final a dependência estática `libSceGnmDriver.prx`; não deixar referências de link que voltem a introduzir relocations GNM.
5. Só depois da auditoria de exports, preparar compute smoke com chamadas dinâmicas. O bloqueio GPU VA/V# e InputUsage continua independente e não pode ser ignorado.
6. O novo pacote de auditoria ainda não foi staged nem executado. Qualquer novo staging/execução de pacote diagnóstico no console exige autorização específica.


## Reconciliação adicional com implementação OpenGNM — 2026-11-23

A revisão do código-fonte OpenGNM fixado pelo projeto (PS4-OpenGNM/opengnm, commit
4b295ca54c82c83acf308d1c646a2dfa9ae57350) acrescenta uma comparação relevante:

- config.orbis.mak compila o backend Orbis e liga -lkernel -lSceGnmDriver -lSceVideoOut.
- src/driver_orbis.c implementa resolvegnmdriversym(): enumera módulos com
  sceKernelGetModuleList, consulta nomes com sceKernelGetModuleInfo, procura
  GnmDriver e resolve o endereço por sceKernelDlsym. Se o módulo não estiver
  disponível ou não exportar o nome, percorre os demais módulos tentando DLSYM.
- Referências fixadas:
  - https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_orbis.c
  - https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/config.orbis.mak

### Conclusão revisada do import

O conjunto de evidências estáticas permanece consistente:

1. O stub libSceGnmDriver.so do sysroot OpenOrbis 0.5.4 exporta
   sceGnmSubmitDone como símbolo global FUNC.
2. O OELF anterior contém a dependência libSceGnmDriver.prx, módulo
   libSceGnmDriver e NID yvZ73uQUqrk#C#C, correspondente a
   CAF67BDEE414AAB9.
3. A implementação OpenGNM de referência não depende apenas de tomar o endereço
   de uma função importada: ela também possui uma resolução dinâmica explícita
   via DLSYM após enumerar módulos.

Isso NÃO fecha a causa do endereço nulo no PS4 FW 14.02. O stub do sysroot
prova a presença do símbolo no provedor de link, não que o firmware do console
tenha export compatível nem que o loader tenha resolvido a relocação estática no
momento em que NR entrou em main. A estratégia OpenGNM torna plausível uma
dependência de estado/carregamento do módulo, mas ainda é hipótese, não causa
confirmada. Não substituir o probe Link-Only por DLSYM e declarar a causa
resolvida sem comparar os resultados dos dois caminhos no mesmo firmware.

### Instrumentação dos sinais e repetição isolada

O código atual de stage_log.c já captura imediatamente o errno de
sigaltstack, sigemptyset e de cada sigaction (SIGSEGV, SIGBUS, SIGILL,
SIGABRT, SIGFPE). O teste host foi reforçado para exigir uma linha SIGNAL_DIAG
de sucesso/errno para cada um dos cinco handlers, em vez de verificar apenas SIGSEGV.

O workflow .github/workflows/ps4-prx.yml está sendo atualizado para produzir
novamente um PKG GNM-Link-Only com o código de telemetria atual e submetê-lo a
QA independente. O QA verifica a importação esperada no OELF, o NID,
gnm_function_calls=none, ausência do shader e integridade do PKG. Um PASS
dessa etapa continuará sendo somente QA de PC/toolchain, não execução no PS4.

### Ordem de liberação preservada

1. Fechar a comparação estática sysroot/NID/OELF/OpenGNM — consistência estática
   confirmada; resolução runtime ainda aberta.
2. Validar a telemetria de errno por operação — implementação presente;
   cobertura host ampliada para os cinco sinais.
3. Rebuild + QA independente do GNM-Link-Only — em CI nesta revisão; sem
   execução de console presumida.
4. Só depois avaliar GNM-Init e medições de VA/4 GiB, sob o plano reversível
   aprovado separadamente. Não introduzir chamada de init sem ABI/NID/export
   confirmado. Dispatch permanece bloqueado por padrão.
## Rebuild GNM-Link-Only com telemetria atual — 2026-11-23

Execução OpenOrbis: https://github.com/demolidorreddragons/NR/actions/runs/45151860412
Commit: b810c42a9035d4acf15f0309eb9dc45b436b3996
Build job: build-prx-and-pkg — SUCCESS.
QA independente: independent-pkg-qa — SUCCESS.
Host-tests: https://github.com/demolidorreddragons/NR/actions/runs/45151860473 — SUCCESS.

### Artefato GNM-Link-Only

- Arquivo: PS4-Neural-Rendering-GCN8-gnm-link-45151860412.pkg
- Local no artefato: gcn8-smoke/45151860412/gnm-link/
- Tamanho: 6.619.136 bytes.
- SHA-256 PKG: 86208571912781468918774b230ae2579d6b55ee55104e21908801731000485b
- SHA-256 ELF: ab5ffb61b86f64b19bdab7e7be6dd2a077b3bc443d35327e9c13f2c891bba8e4
- SHA-256 OELF: 3364bc39dc24f21cda41383fba6b10d74110be3c68937e6c22bee205941d2a60
- SHA-256 MAP: 5f94f4180172d48f79065ab0a00230cf0cc5cf1d9333217bde940df03c7f28f5
- Manifest: gnm_import_probe=sceGnmSubmitDone_address_only_no_call; gnm_function_calls=none; hardware_status=not_run.
- QA artifact: 14807750099; GNM_LINK_QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED.
- Resultado QA: SFO PASS; import estático esperado; GNM calls NONE; compute shader ABSENT.

### Stub OpenOrbis reexaminado

- Arquivo do sysroot: libSceGnmDriver.so, 40.472 bytes.
- SHA-256: 21d38ac5e7d79ddc9ee9bc7b9a84547f43ea3144883f6ac787d16704c1b5121f.
- readelf --dyn-syms confirma sceGnmSubmitDone como FUNC GLOBAL DEFAULT, valor 0x46cc, tamanho 10.
- O corpo do stub termina em um salto para si próprio (jmp 0x46d4). Ele é um provedor de link/metadata, não a implementação de runtime do driver.
- O OELF do novo probe contém libSceGnmDriver.prx, módulo libSceGnmDriver e NID yvZ73uQUqrk#C#C.

Esta repetição fecha novamente a consistência estática de nome/biblioteca/NID e prova que o import esperado foi preservado no novo OELF. Não fecha o endereço nulo no firmware 14.02: nenhum código GNM foi chamado e nenhum teste de hardware ocorreu.

### Comparação com um app de referência OpenGNM

O exemplo público PS4-OpenGNM/freegnm-examples, triangle/src/main.c, chama
sceGnmSubmitCommandBuffers() e sceGnmSubmitDone() no fluxo de apresentação.
Seu Makefile, com USE_OPENGNM=1, liga -lSceGnmDriver e libopengnm.a; o
caminho USE_OPENGNM também evita incluir diretamente orbis/GnmDriver.h.
Referências:
- https://github.com/PS4-OpenGNM/freegnm-examples/blob/main/triangle/src/main.c
- https://github.com/PS4-OpenGNM/freegnm-examples/blob/main/triangle/Makefile
- https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_orbis.c

No commit OpenGNM fixado, a implementação de sceGnmSubmitDone é um wrapper
que chama resolvegnmdriversym("sceGnmSubmitDone"). Esse resolver enumera módulos
e usa sceKernelDlsym no módulo GnmDriver, com fallback pelos demais módulos.
Portanto, o exemplo de referência demonstra uma rota de resolução dinâmica
com wrapper; não demonstra que a relocação estática direta do NR deva estar
resolvida antes de qualquer resolução/carregamento de módulo. Também não prova
compatibilidade do firmware 14.02 deste console.

### Limite operacional

O PKG foi construído e passou QA independente no PC, mas não foi staged em
/data/pkg, instalado nem executado no PS4. O workflow de staging continua
manual e não foi iniciado. Não converter PASS_PC_ONLY em resultado de hardware.
Próximo gate: obter execução real do Link-Only com SIGNAL_DIAG/errno e
GNM_IMPORT_ADDRESS; até lá, GNM-Init, VA/4 GiB, memória/cache e dispatch ficam
bloqueados.