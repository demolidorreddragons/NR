# Auditoria de participação externa e dependência estática GNM

Data: 2026-11-22  
Repositório: `demolidorreddragons/NR`  
Branch: `feature/gcn8-numeric-kernels`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**  
Escopo: evidências públicas de colaboração/revisão externa e correção da cadeia de link do compute smoke.  
Limite: esta auditoria não prova ausência absoluta de acesso externo ao repositório; registra somente os eventos e metadados consultados.

## 1. Participação externa — evidência verificada

Foram consultados os PRs de delegação e as submissões de revisão:

| PR | Escopo | Autor exibido pelo GitHub | Revisões formais consultadas | Estado observado |
|---|---|---|---|---|
| #9 | Reconstrução do grafo DLSS-NR 310.8.3 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara handoff, não implementação |
| #10 | Runtime compute GNM para PS4 Pro GFX8 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara handoff, não backend funcional |
| #11 | QA clean-room independente dos shaders GCN8 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara apenas contrato de tarefa |

Links:
- https://github.com/demolidorreddragons/NR/pull/9
- https://github.com/demolidorreddragons/NR/pull/10
- https://github.com/demolidorreddragons/NR/pull/11

As consultas de reviews e review threads para os três PRs não retornaram revisões formais. Os corpos dos PRs #9–#11 explicitam que são tarefas/handoffs e não alegam implementação ou aprovação.

**Conclusão delimitada:** não há evidência verificável, nos PRs e revisões examinados, de entrega técnica ou revisão independente por colaborador externo. Isso não permite afirmar que ninguém externo teve acesso ou realizou qualquer atividade fora desses registros. Nenhum nome de especialista externo será creditado sem commit/review/relatório identificável.

## 2. Achado de engenharia: dependência GNM ainda ligada estaticamente

Na inspeção do commit anterior, o modo compute usava quatro exports GNM via `sceKernelDlsym`, mas o comando de link ainda continha `-lSceGnmDriver`. Isso preservava uma dependência estática `DT_NEEDED` desnecessária, incompatível com o objetivo de isolar o carregamento/resolução dinâmica e evitar a classe de falha observada no GNM-Link-Only.

O código-fonte não mostrava chamadas diretas às quatro funções de runtime, mas a presença de uma biblioteca na linha de link não deve ser tratada como inofensiva sem inspeção do ELF final.

### Correção aplicada

Commit: [83d15edf1c3411752f75fdc98172f9e2f8094865](https://github.com/demolidorreddragons/NR/commit/83d15edf1c3411752f75fdc98172f9e2f8094865)

Arquivo: `tools/build_compute_smoke_pkg.sh`

1. Removido `-lSceGnmDriver` somente do link do modo `compute`. Os modos diagnósticos que testam explicitamente link estático permanecem separados.
2. Adicionada auditoria do ELF antes de gerar o OELF/PKG:
   - falha se `DT_NEEDED` mencionar `libSceGnmDriver`;
   - falha se a tabela de símbolos mostrar símbolo `sceGnm*` indefinido;
   - emite `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only` somente quando ambos os testes passam.
3. A resolução das quatro funções continua dinâmica e atômica pelo resolver existente.


### Verificação adicional do setter de memória

Foi inspecionado diretamente `PS4-OpenGNM/opengnm/include/gnm_buffer.h` no commit pinado `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, linhas 90–98. `sceGnmBufSetMemoryType` é `static inline` e apenas escreve os campos `mtype`, `mtype_l1s` e `mtype_l2`; não chama uma exportação GNM externa. Portanto, a chamada existente no harness não justifica manter `-lSceGnmDriver` no link. Os setters de base address/formato do mesmo header podem chamar `sceGnmWriteMsg` em caminhos de erro, mas o harness constrói esses campos manualmente e não chama esses setters. Esta leitura de fonte reduz a hipótese de dependência implícita, mas o ELF final ainda precisa passar pela auditoria automatizada.

### Critério de aceitação da correção

- Build compute do PS4 conclui sem depender de `-lSceGnmDriver`.
- Auditoria do ELF emite PASS e não encontra `DT_NEEDED libSceGnmDriver` nem símbolo GNM indefinido.
- PKG continua contendo o shader e passa validação/extração independente.
- Host tests e demais jobs obrigatórios passam no commit corrigido.
- O artefato e seus hashes são correlacionados ao commit exato.

**Estado inicial desta seção:** correção versionada; a validação foi concluída posteriormente e está registrada na seção 2.1 abaixo.

## 2.1. Resultado da validação CI — commit corrigido

A validação foi localizada diretamente na lista de execuções do GitHub Actions, não inferida a partir de commits anteriores:

| Verificação | Run / Job | Resultado observado |
|---|---|---|
| Build de shaders GFX8 + compute smoke PKG | [45005750367](https://github.com/demolidorreddragons/NR/actions/runs/45005750367), job `137156845303` + `137157762811` | SUCCESS |
| Host tests | [45005750309](https://github.com/demolidorreddragons/NR/actions/runs/45005750309), job `137156845061` | SUCCESS; Configure, Build e Test concluídos |
| Coleta read-only do logger | [45005750309](https://github.com/demolidorreddragons/NR/actions/runs/45005750309), job `137156844849` | SUCCESS; FTP read-only e upload de evidência concluídos |

O run de build tem `head_sha=83d15edf1c3411752f75fdc98172f9e2f8094865`, branch `feature/gcn8-numeric-kernels`, evento `push`. No log do job `137157762811` foram inspecionados:
- `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only` — o script não encontrou `DT_NEEDED libSceGnmDriver` nem símbolo `sceGnm*` indefinido no ELF;
- `COMPUTE_SMOKE_PKG_PAYLOAD=PASS`;
- PKG compute: `PS4-Neural-Rendering-GCN8-compute-45005750367.pkg`, SHA-256 `97b866dfa5cc5bbb655417a4d6ec10700dd22a50055d6e22f06813dde793e5fb`;
- PKG startup-only separado: `PS4-Neural-Rendering-GCN8-startup-45005750367.pkg`, SHA-256 `e6e1553399f91bf96d03790a5efc6677856d0f8819279f8d1c2144c2da16c1a1`.

O workflow também executou validação e extração do PKG pelo PkgTool, verificando o payload compute e o shader esperado. O artefato ZIP `nr-gcn8-compute-smoke-ps4-pkg` é o ID `14747203138`, 25.268.050 bytes, digest SHA-256 `630e1a9a1f314837796fae560953733b808116aa9fb1b173cf76119d950c5612`; o ZIP foi baixado e seu SHA-256 recalculado, com correspondência. O PKG compute dentro dele tem 6.619.136 bytes e SHA-256 `97b866dfa5cc5bbb655417a4d6ec10700dd22a50055d6e22f06813dde793e5fb`, recalculado diretamente após extração do ZIP. O artefato de shaders GFX8 é o ID `14747912741`, 26.065 bytes, digest SHA-256 `047e83a8cecaaf4799f7ef14ea969ae2c9dbe20d6c6358890bede230429e1717`.

**Lacuna de rastreabilidade do binário:** o ZIP de PKG contém os PKGs compute/startup, manifests, GP4 e SFO, mas não contém `smoke.elf`, `smoke.oelf` nem `smoke.map`. O manifest registra apenas seus hashes (`ELF ffc40a370ada4d920a5034cf1bad3f1695b60ad630111d0c9d7b4bab31bdb309`; `OELF c84dd06e796b0e77371ee2c427ad836f926110879b4a3cc5e35e83ecd55fc721`; `MAP ba46e6aac1088a0013e0e6b835081b62f939f0bfccecacf2de07f8476c57fcc5`). Assim, o marcador de build prova que a auditoria ELF passou no runner, mas não foi possível reabrir o ELF nem inspecionar os Sony import metadata do OELF a partir do artefato preservado. Isso permanece pendente de publicação dos binários de auditoria ou de evidência equivalente.

**Classificação correta:** build/shader pipeline = `CI_VALIDATED`; testes host = `HOST_VALIDATED`; validação do PKG executada no job = validação automatizada local do próprio pipeline, não revisão independente clean-room. Não houve job independente de QA de pacote neste workflow. O marcador PASS cobre o ELF no runner; o ZIP não preservou os binários ELF/OELF/map para re-inspeção. Portanto, Sony import metadata do OELF continua não auditada independentemente.

O compute harness permanece bloqueado por padrão: a compilação não define `PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE`, e `compute_smoke.cpp` retorna `RESULT=BLOCKED; REASON=COMPUTE_HARDWARE_GATES_NOT_CLEARED` antes de carregar GNM ou tentar dispatch. Portanto, o PKG compute é artefato de diagnóstico/CI e **não deve ser instalado** para testar dispatch.

Esta validação não resolve GPU VA/V#, InputUsage runtime, cache/coerência, nem constitui execução de hardware. Nenhum PKG foi staged, instalado ou executado.

### 2.2. Reinspeção binária do artefato preservado — run 45014637939

A correção de retenção do artefato foi exercitada no commit `b2baac371acfbc0b10d17f5ad15cf5f1d7c77171`. O run [45014637939](https://github.com/demolidorreddragons/NR/actions/runs/45014637939) terminou com os dois jobs SUCCESS: `build-gfx8-compute` (job `137184213535`) e `Build standalone PS4 GCN8 compute smoke PKG` (job `137185098537`).

Artefato `nr-gcn8-compute-smoke-ps4-pkg`, ID `14751630143`: 25.433.004 bytes; SHA-256 do ZIP recalculado após download: `8674616f7855d217f57ae7d7002b2cc9581cfa0e03a67b52c8081c21f667b907` (igual ao digest do GitHub). O ZIP contém os dois PKGs, manifests, GP4, SFO e, para ambos os modos, `smoke.elf`, `smoke.oelf` e `smoke.map`.

**Compute PKG e arquivos auditados:**
- PKG: `PS4-Neural-Rendering-GCN8-compute-45014637939.pkg`, 6.619.136 bytes, SHA-256 `9d00d221e0888b0d2543bee9fecf3ab93fd52796e36856b898b3919d5d37bac0`.
- ELF: 167.928 bytes, SHA-256 `e8e6cc962e23f11a3f4eb93f288667fd9142be1b18873c0df9cc79cdecc793c4`.
- OELF: 170.248 bytes, SHA-256 `51473828592f0ad3b5eb11837f91211fb59bd3755a0f9edbfa529285a9cec506`.
- MAP: 51.490 bytes, SHA-256 `00b938daa630e0070179db063c10c959cdc8661f3d00f00956a3ebdd24012136`.
- Manifest: shader `compute_smoke_add.sb`, SHA-256 `ab59ebaf6f3c343a2228103205404b15c86f1359a48ac7f55e600c59aa2ed51f`; `gnm_import_probe=none`, `gnm_load_probe=none`, `gnm_function_calls=none`, `hardware_status=not_run`.

Os hashes dos quatro arquivos foram recalculados diretamente dos membros extraídos e conferem com o manifesto. O PKG principal e o alias Content ID dentro do ZIP são byte-idênticos e têm o mesmo SHA-256. O SFO registra categoria `gde`, `APP_TYPE=1`, Title ID `PNEU00002` e Content ID `IV0000-PNEU00002_00-GCN8SMOKE0000000`. O GP4 registra `app_type=full`. O log do job contém `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only`, `COMPUTE_SMOKE_PKG_PAYLOAD=PASS` e o mesmo SHA-256 do PKG; o PkgTool extraiu e enumerou `eboot.bin`, o shader e os módulos de suporte. O ícone empacotado corresponde ao hash aprovado `7fe6a11ad7b4d37e5741c77822730a82487237dbed4a3eca529e995a29421ae4`.

**Auditoria Sony/OELF:** o OELF contém `PT_SCE_DYNLIBDATA` em offset `0x28ff8`, tamanho `0x910`. A tabela dinâmica foi interpretada usando os offsets Sony: `DT_SCE_STRTAB=0x18`, `DT_SCE_STRSZ=0x159`, `DT_SCE_SYMTAB=0x178`, `DT_SCE_SYMTABSZ=0x1f8`, `DT_SCE_HASH=0x6e8`, `DT_SCE_HASHSZ=0x60`, `DT_SCE_RELA=0x538`, `DT_SCE_RELASZ=0x1b0`, `DT_SCE_JMPREL=0x370` e `DT_SCE_PLTRELSZ=0x1c8`. A string table Sony resolve o offset `0x1` para `libkernel.prx` e `0xf` para o módulo `libkernel`. As tags `DT_NEEDED=1`, `DT_SCE_NEEDED_MODULE=0x101010000000f`, `DT_SCE_IMPORT_LIB=0x100010000000f` e `DT_SCE_IMPORT_LIB_ATTR=0x1000000000009` descrevem esse único módulo/biblioteca de importação. Os 19 NIDs de importação usam o sufixo de IDs `#B#B` e correspondem às 19 funções de sistema/libkernel indefinidas no ELF (close, fsync, sigaltstack, sigaction, _exit, write, poll, __error, _open, _fcntl, mmap, munmap, madvise, _ioctl, _readv, _read, _writev, _write, lseek). Não há `libSceGnmDriver`, módulo GNM, NID GNM ou símbolo `sceGnm*` indefinido no ELF/OELF; as relocações auditadas também não apontam para símbolos GNM. A interpretação segue a especificação OpenOrbis de `PT_SCE_DYNLIBDATA` e tags Sony: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/docs/MD/OELF%20Specification/PS4%20ELF%20Specification%20-%20Dynlib%20Data.md.

**Gate de execução:** o modo compute compila `smoke_main.cpp` sem `PNE_GCN8_GNM_LOAD_ONLY`/`PNE_GCN8_GNM_LINK_ONLY` e sem definir `PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE`; a única macro adicional específica é o Run ID. Em `compute_smoke.cpp:185–187`, o código retorna `RESULT=BLOCKED; REASON=COMPUTE_HARDWARE_GATES_NOT_CLEARED` e `COMPUTE_DISPATCH_NOT_ATTEMPTED` antes de qualquer load/resolução GNM ou dispatch. O mapa não registra símbolos GNM. Assim, este PKG não executa GNM e **não deve ser instalado**.

**Classificação atualizada:** artefato/manifest/ELF/OELF/MAP têm rastreabilidade binária reaberta e hashes conferidos; auditoria de import estático GNM = PASS; payload/extração automatizada do PKG = PASS; categoria técnica Full/Base = confirmada. Isto continua sendo validação interna do build e inspeção binária, não QA independente clean-room nem validação de firmware/hardware. Permanecem bloqueados GPU VA/V#, InputUsage runtime, coerência/cache, dispatch/readback no PS4 e revisão independente externa. Nenhum staging, instalação, execução no console ou alteração no PS4 ocorreu.
## 3. Bloqueios que permanecem

1. **GPU VA / V# writable:** sem contrato autoritativo demonstrando que o ponteiro mapeado usado pelo harness é o GPU VA válido e representável para a tabela de recursos.
2. **InputUsage:** sem prova do parser Sony no firmware-alvo para a semântica do offset em compute GFX8; `runtime_semantics=UNRESOLVED`.
3. **Coerência e sincronização:** política de cache/flush/invalidate e semântica fence ainda não validadas em hardware.
4. **Revisão externa:** não há revisão independente registrada para este escopo.
5. **Hardware:** dispatch/readback no PS4 permanece `NOT_RUN`.

Nenhuma destas lacunas é compensada por build verde, compilação de shader, validação estrutural ou teste host.

## 4. Segurança e operações no console

Nenhum PKG foi staged, instalado ou executado nesta rodada. Nenhum PRX foi ativado; nenhuma configuração, save, log ou dado do PS4 foi alterado. O compute smoke permanece bloqueado para instalação/execução até que os gates técnicos e de revisão sejam satisfeitos e exista autorização específica.

## 5. Próximas ações

1. Completar a inspeção binária do artefato: conferir OELF e metadados Sony de importação, além da auditoria ELF já aprovada.
2. Se o build/link falhar por símbolo que dependia da biblioteca removida, identificar o símbolo concreto e sua origem; não reintroduzir a dependência inteira sem análise.
3. Obter evidência primária para GPU VA/V# e InputUsage ou manter os respectivos gates bloqueados.
4. Solicitar revisão externa real do commit e do pacote neutro de critérios, registrando identidade, escopo, conflitos, achados e decisão somente quando houver submissão verificável.

**Decisão:** `IN_PROGRESS`. A remoção da dependência estática está aprovada no nível ELF/CI do commit `83d15edf`; o build, o marcador de auditoria e o payload do PKG passaram. Não se promove a aprovação para OELF/Sony import metadata, QA independente clean-room ou runtime. Revisão externa e validação de hardware continuam `NOT_RUN`/ `BLOCKED`.

### 2.3. Auditoria forense do linker stub e reauditoria do GNM-Link eboot

O workflow PRX preservou os arquivos exatos do toolchain OpenOrbis v0.5.4 para auditoria. O artefato `ps4-neural-engine-build` do run [45015947338](https://github.com/demolidorreddragons/NR/actions/runs/45015947338), ID `14752435074`, tem 38.256.476 bytes e SHA-256 `bce83e2fc3eb80c0ea65f14f5fda8721827e61641cebe250eced335d7a782d8e`; o ZIP foi baixado e o hash recalculado com correspondência.

Arquivo `libSceGnmDriver.so` extraído do artefato: 40.472 bytes, SHA-256 `21d38ac5e7d79ddc9ee9bc7b9a84547f43ea3144883f6ac787d16704c1b5121f`. `file`/`readelf` identificam um ELF64 x86-64 shared object de link, não um SPRX/OELF de sistema. A tabela dinâmica tem apenas tags ELF genéricas; não há `DT_SCE_*`, `DT_SCE_IMPORT_LIB`, `DT_SCE_NEEDED_MODULE`, SONAME ou `DT_NEEDED`. A `.dynsym` contém 203 exports por nome, incluindo as quatro funções GNM usadas pelo harness. `sceGnmSubmitDone` e `sceGnmDispatchInitDefaultHardwareState` têm corpos stub que terminam em loop infinito. O `.so` satisfaz o linker; não é runtime e não deve ser chamado.

**Reauditoria do pacote GNM-Link-Only instalado anteriormente:** o run de build [44873622760](https://github.com/demolidorreddragons/NR/actions/runs/44873622760), commit `bb40fb8e23582e8fbae53ca50812c128a7b15c0f`, produziu o artefato `ps4-neural-engine-build` ID `14694982825`, ZIP SHA-256 `490e33b2cb9fb574ffa29e33af8605f538db6cc559e13f3b479d533a56884a11`. O PKG GNM-Link era `PS4-Neural-Rendering-GCN8-gnm-link-44873622760.pkg`, SHA-256 `e68da9f35981aa32e788ebe3f0461360167c0194b0c408c47c945745ebab5847`; ELF SHA-256 `ac9bc3ac522fe2bcaa2768ef1f650aa2396f263e3f251f6a66525002be6eebb3`; MAP SHA-256 `9a840b64fbc8f2800f010af1ab25d4fbc9fee5a1ea4b8f5d880406af21b77c69`.

O PKG antigo foi reaberto com o PkgTool no run de auditoria [45018118702](https://github.com/demolidorreddragons/NR/actions/runs/45018118702), job `137194754008`. A inspeção do `uroot/eboot.bin` corrigiu a conclusão anterior: o OELF **tem** Sony import metadata para GNM. O resultado do audit script foi:
- `SONY_IMPORT_LIBRARIES=1:libkernel;2:libSceGnmDriver`.
- `SONY_NEEDED_MODULES=1:libkernel;2:libSceGnmDriver`.
- NID `yvZ73uQUqrk#C#C`, símbolo Sony índice `2`, relocation `r_offset=0xc000`.
- `SONY_IMPORT_AUDIT=PASS`.

O ELF pré-SELF contém `DT_NEEDED libSceGnmDriver.so`, símbolo indefinido `sceGnmSubmitDone` e relocation `GLOB_DAT` no mesmo offset `0xc000`. O NID `yvZ73uQUqrk` decodifica para `0xCAF67BDEE414AAB9`, correspondendo ao NID conhecido de `sceGnmSubmitDone`. Logo, a causa do GOT nulo observado no PS4 **não é ausência da Sony import metadata nem NID incorreto**. O loader/runtime não preencheu o GOT apesar de a tabela de importação estar estruturalmente correta. A causa exata (carregamento automático do módulo, resolução de import em eboot ou outra condição do loader FW 14.02) permanece `UNRESOLVED`.

**Decisão operacional:**
- Compute eboot: manter sem import estático GNM e usar `sceKernelLoadStartModule` + `sceKernelDlsym`; o caminho dinâmico já passou no GNM-Load-Only em hardware.
- GNM-Link eboot: metadata estática `PASS`, mas resolução de GOT em firmware `FAIL`; não repetir instalação/execução até isolar o comportamento do loader.
- PRX principal: import estático `sceGnmSubmitAndFlipCommandBuffers` tem metadata `STATIC_PASS` (seção 2.4), mas resolução runtime/hook ainda `NOT_RUN`.
- O `.so` OpenOrbis é apenas um stub de link; o conversor `create-fself` gera os NIDs no OELF a partir dos símbolos indefinidos. A ausência de NID no `.so` não significa ausência no OELF.

Fonte do conversor: https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/OELFGenDynlibData.go. A fonte percorre imports indefinidos para construir a NID table; a diferença entre `--eboot` e `--lib` não explica a falha antiga.

**Classificação:** linker stub = `STATIC_REVIEW`; GNM-Link eboot Sony metadata = `STATIC_PASS`; GOT resolution no PS4 = `FAIL`; causa raiz do loader = `UNRESOLVED`; GNM-Load-Only DLSYM = `PS4_HARDWARE_VALIDATED` para o export testado. Nenhum staging/instalação nesta reauditoria.

### 2.4. Auditoria Sony import metadata do PRX principal — PASS estático

O PRX final do run `45015947338` foi aberto como fake SELF, usando a tabela de segmentos SELF para extrair o `PT_SCE_DYNLIBDATA` real. O segmento está no offset SELF `0x30b90`, tamanho `0x6100`; no ELF/OELF corresponde a `PT_SCE_DYNLIBDATA` em offset `0x47850`, tamanho `0x6100`. A tabela dinâmica contém dez módulos e dez bibliotecas, incluindo `libSceGnmDriver.prx` e `libSceGnmDriver`.

A entrada GNM está presente e correlacionada com a relocação original:
- `DT_SCE_NEEDED_MODULE=0x20101000000bf` identifica o módulo GNM; `DT_SCE_IMPORT_LIB=0x20001000000bf` identifica a biblioteca `libSceGnmDriver`; `DT_SCE_IMPORT_LIB_ATTR=0x2000000000009`.
- A Sony string table tem a entrada `xbxNatawohc#C#C` no offset `0x1ae`. O símbolo Sony correspondente é o índice `5` de `DT_SCE_SYMTAB`.
- A relocação Sony `R_X86_64_GLOB_DAT` em `r_offset=0x28000` aponta para o símbolo índice `5`, isto é, `xbxNatawohc#C#C`.
- No ELF pré-SELF, a relocação original no mesmo GOT offset `0x28000` aponta para o símbolo indefinido `sceGnmSubmitAndFlipCommandBuffers` (dynsym index `4`).
- O NID `xbxNatawohc` decodifica para `0xC5BC4D6AD6B0A217` e coincide com o registro público shadPS4: `LIB_FUNCTION("xbxNatawohc", "libSceGnmDriver", 1, "libSceGnmDriver", sceGnmSubmitAndFlipCommandBuffers)`.

**Conclusão:** o `create-fself --lib` usado para o PRX principal gerou Sony import metadata válida e coerente para `sceGnmSubmitAndFlipCommandBuffers`. A ausência de NID no `.so` de link OpenOrbis não significa ausência no PRX final: `create-fself` gera a tabela Sony a partir dos símbolos indefinidos e das bibliotecas identificadas. O PRX principal contém a entrada correta. O OELF exato do antigo GNM-Link eboot também foi reaberto e contém a NID correta; seu GOT continuou nulo no PS4. A falha é de resolução runtime apesar da metadata correta, com causa raiz ainda `UNRESOLVED`.

**Limites:** isto valida a estrutura estática do import e a correspondência NID/library/module. Não prova que o loader do firmware 14.02 resolva o import em runtime, que o endereço seja detour-safe ou que a ABI do hook esteja correta. `HOOK32` usa o endereço importado e `Detour_DetourFunction`; runtime/hardware continuam `NOT_RUN`. Nenhum staging/instalação foi feito.

Fontes:
- OpenOrbis create-fself documenta `--lib` como modo para gerar PRX library: https://github.com/OpenOrbis/create-fself
- shadPS4 NID registration `sceGnmSubmitAndFlipCommandBuffers`: https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/gnmdriver/gnmdriver.cpp
- GoldHEN SDK hook implementation: https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/include/Utilities.h


### 2.5. Auditoria estática do DetourMode_x32 — novo bloqueio de segurança

Após confirmar a NID metadata do PRX, foi inspecionada a implementação exata do GoldHEN SDK pinado (`Detour.c`, commit `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`). O macro `HOOK32(sceGnmSubmitAndFlipCommandBuffers)` cria `DetourMode_x32` e chama `Detour_DetourFunction`, descartando o ponteiro de retorno.

Em `Detour_DetourFunction32`, o SDK:
- calcula o tamanho de instruções para sobrescrever pelo menos 5 bytes;
- aloca `TrampolinePtr` com `malloc` sem solicitar proximidade ao endereço original;
- grava um salto x86 `E9 rel32` no endereço da função, calculando `Destination - (Address + 5)` e truncando diretamente para `uint32_t` em `Detour_WriteJump32`;
- não verifica se a distância assinada está no intervalo `INT32_MIN..INT32_MAX` antes de escrever o deslocamento;
- não retorna status ao chamador via macro `HOOK32`, que ignora o resultado.

`DetourMode_x32` não significa processo de 32 bits; é um salto relativo de 32 bits dentro de código x86-64. O salto só é válido se o trampoline estiver a menos de 2 GiB do alvo. Como o trampoline vem de `malloc` e não há verificação de alcance, o deslocamento pode truncar e apontar para endereço incorreto. O modo x64 usa salto absoluto de 14 bytes, mas requer um prólogo de instruções suficientemente longo; não é seguro trocar de modo sem medir o prólogo real do export no firmware.

**Decisão:** `PRX GNM NID import metadata=STATIC_PASS`, mas `HOOK_DETOUR_SAFETY=BLOCKED`. Não instalar/ativar o PRX nem executar o hook até corrigir ou substituir a lógica de detour com validação de alcance, conferir retorno de instalação e validar o prólogo/ABI no firmware. A assinatura de nove argumentos coincide estruturalmente com a implementação pública shadPS4, mas isso não valida o prólogo nem o patch.

Fontes:
- GoldHEN SDK `Utilities.h` (`HOOK32`/`HOOK_CONTINUE`): https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/include/Utilities.h
- GoldHEN SDK `Detour.c`: https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/source/Detour.c
- GoldHEN SDK `Detour.h`: https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/include/Detour.h
### 2.6. Algoritmo de geração de NID do create-fself

A fonte pública de `OpenOrbis/create-fself` (`pkg/oelf/OELFGenDynlibData.go`) esclarece como o conversor constrói os NIDs: para cada símbolo indefinido, encontra a biblioteca cujo ELF exporta o nome, calcula SHA-1 de `symbolName + 518D64A635DED8C1E6B039B1C3E55230`, usa os primeiros oito bytes e codifica em Base64, acrescentando `#libraryId#moduleId`. A tabela `_moduleToLibDictionary` mapeia explicitamente `libSceGnmDriver` para `libSceGnmDriver.prx`.

Reprodução independente do algoritmo:
- `sceGnmSubmitDone` → `yvZ73uQUqrk` → NID `0xCAF67BDEE414AAB9`, correspondente ao NID conhecido do export.
- `sceGnmSubmitAndFlipCommandBuffers` → `xbxNatawohc` → NID `0xC5BC4D6AD6B0A217`, correspondente ao registro público shadPS4.

Isso confirma a coerência dos NIDs no PRX principal e no antigo GNM-Link eboot. A fonte mostra que a geração de imports não é exclusiva de `--lib`; a reauditoria confirmou que o eboot também contém a NID correta. Portanto, o GOT nulo antigo é uma falha de resolução runtime apesar da metadata válida; a causa exata do loader FW 14.02 permanece `UNRESOLVED`.

Fonte: https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/OELFGenDynlibData.go
### 2.7. Auditoria automatizada de OELF e expansão do GNM-Load-Only

Foi adicionado `tools/audit_ps4_self_imports.py`, parser fail-closed para `PT_SCE_DYNLIBDATA` em OELF e fake SELF/PRX. Ele lê `DT_SCE_IMPORT_LIB`, `DT_SCE_NEEDED_MODULE`, Sony string/symbol tables e relocações; exige o NID correto com relocation para imports requeridos e rejeita bibliotecas proibidas.

**Integração CI:**
- `.github/workflows/ps4-prx.yml` audita o PRX final e exige `libSceGnmDriver:libSceGnmDriver:xbxNatawohc`; run [45018436422](https://github.com/demolidorreddragons/NR/actions/runs/45018436422), build e QA independente SUCCESS; log `SONY_IMPORT_AUDIT=PASS`. O artifact `14752946485` (38.256.653 bytes) teve ZIP SHA-256 `db3782f080e561a2d276af3e8da9c3989d7a6412d8393960369dd8b30270ea01` conferido; PRX hash `f85a69a7165d4fd5ae62e3237fea9bacbdd1232a19ab1e8745f5f52da9e6598e`.
- `tools/build_compute_smoke_pkg.sh` audita o OELF depois de `create-fself`: compute/startup/GNM-Load devem proibir import estático GNM; GNM-Link exige NID `yvZ73uQUqrk`. O compute OELF passou no run [45017446531](https://github.com/demolidorreddragons/NR/actions/runs/45017446531), artifact `14751474371`, ZIP SHA-256 `2c8738c9abb675102226748df0eea99b38b0b586199be737ef33cfabca404725`, PKG SHA-256 `38c785e6e2d4be10c37270f3143df2d4fb861101508c9e71c393eaaaddb7795d`, OELF SHA-256 `ea1a3527e4184de9196e78d3ac18ba9b837165facf8d46f32ba125680cac3719`.
- A reauditoria histórica do GNM-Link eboot no run `45018118702` passou com NID `yvZ73uQUqrk#C#C` e relocation `0xc000` (seção 2.3).

**GNM-Load-Only ampliado:** o probe agora resolve cinco exports sem chamar nenhum: `sceGnmSubmitDone`, `sceGnmDispatchInitDefaultHardwareState`, `sceGnmFlushGarlic`, `sceGnmSubmitCommandBuffers` e `sceGnmSubmitAndFlipCommandBuffers` (alvo do hook do PRX). O manifest declara a lista em `gnm_exports_required`. Build [45018436422](https://github.com/demolidorreddragons/NR/actions/runs/45018436422) terminou SUCCESS; o OELF do GNM-Load tem somente `libkernel`, `SONY_IMPORT_AUDIT=PASS`; QA independente `PASS_PC_ONLY_NOT_HARDWARE_TESTED` no job `137196373694`.

Artefato `ps4-neural-engine-build` ID `14752946485`, 38.256.653 bytes, ZIP SHA-256 `db3782f080e561a2d276af3e8da9c3989d7a6412d8393960369dd8b30270ea01` verificado. PKG GNM-Load `PS4-Neural-Rendering-GCN8-gnm-load-45018436422.pkg`, 6.619.136 bytes, SHA-256 `7b0807a882895b9a392f64f0e97bb7aca8e2aaa94c254e0667cc0128996c1051`; ELF `8e2fdcd2b2b49f006ee62920835d3cca38d50b399eb56faab029931674dc1ed1`; OELF `85c329480fb07e229ded29782ad1569204df305e4d5a7af18a8011c071777dc3`; MAP `9f43ec92cdfe773e10f24d3ebfd98458283cf19407d50a567d1de12faf3ae4ba`. Manifest: `gnm_exports_required` contém os cinco símbolos; `gnm_function_calls=none`; `hardware_status=not_run`.

**Limite:** o build e QA de PC confirmam estrutura, payload e import isolation, não a disponibilidade real de `sceGnmSubmitAndFlipCommandBuffers` no firmware 14.02. O novo DLSYM ainda precisa de execução no PS4. O pacote não foi staged nem instalado; o staging exige autorização específica.