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

### 2.3. Auditoria forense do linker stub OpenOrbis GNM — run 45015947338

O workflow PRX preservou os arquivos exatos do toolchain OpenOrbis v0.5.4 para auditoria. O artefato `ps4-neural-engine-build`, ID `14752435074`, tem 38.256.476 bytes e SHA-256 `bce83e2fc3eb80c0ea65f14f5fda8721827e61641cebe250eced335d7a782d8e`; o ZIP foi baixado e o hash recalculado com correspondência.

Arquivo `libSceGnmDriver.so` extraído do artefato: 40.472 bytes, SHA-256 `21d38ac5e7d79ddc9ee9bc7b9a84547f43ea3144883f6ac787d16704c1b5121f`. `file`/`readelf` identificam um ELF64 x86-64 shared object de link, não um SPRX/OELF de sistema. A tabela dinâmica tem apenas tags ELF genéricas (INIT/FINI_ARRAY, GNU_HASH, STRTAB, SYMTAB, STRSZ, SYMENT, RELA/RELASZ/RELAENT e NULL); não há `DT_SCE_*`, `DT_SCE_IMPORT_LIB`, `DT_SCE_NEEDED_MODULE`, SONAME ou DT_NEEDED. A busca de strings não encontrou o NID `CAF67BDEE414AAB9`, sua codificação `yvZ73uQUqrk`, nem metadados `DT_SCE`.

A `.dynsym` contém 203 símbolos e exporta por nome as quatro funções GNM usadas pelo harness, incluindo `sceGnmSubmitDone` (valor `0x46cc`), `sceGnmDispatchInitDefaultHardwareState` (`0x40d2`), `sceGnmFlushGarlic` (`0x41fe`) e `sceGnmSubmitCommandBuffers` (`0x46b8`). Porém, o disassembly de `sceGnmSubmitDone` é `endbr64; push rbp; mov rsp,rbp; jmp 0x46d4` — um loop infinito. `sceGnmDispatchInitDefaultHardwareState` também termina em salto para si mesmo. Portanto, os nomes/exportações satisfazem o linker, mas os corpos são stubs não funcionais e não devem ser chamados.

### Impacto sobre o GNM-Link-Only e decisão

Esta evidência explica por que `-lSceGnmDriver` não pode ser tratado como prova de import Sony válido. O linker recebe um ELF stub com símbolos C, não uma tabela Sony de NID/module/library. Isso é consistente com o OELF já examinado no antigo GNM-Link-Only: o executável tinha dependência/símbolo ELF, mas não continha a entrada Sony de importação necessária; no PS4 o slot GOT de `sceGnmSubmitDone` apareceu nulo. A ausência de metadata no stub, isoladamente, não prova todos os detalhes internos do loader FW 14.02, mas combinada com o OELF e o resultado de hardware torna a hipótese de import estático sem NID metadata a explicação mais forte disponível.

**Decisão operacional:** manter `-lSceGnmDriver` fora do modo compute e de qualquer caminho que se pretenda executar no PS4. Usar `sceKernelLoadStartModule` + `sceKernelDlsym` para o caminho runtime; esse caminho já teve sucesso observado no probe GNM-Load-Only, com endereço de export válido. O build/link smoke que usa `-lSceGnmDriver` é apenas um teste de que o stub satisfaz o linker, não teste de import Sony nem de chamada GNM. Nenhuma função do stub deve ser executada.

### Correção de nomenclatura do workflow

O log do workflow `PS4 PRX and PKG build` atualmente emite `GCN8_GNM_LINK_SMOKE=resolved_with_valid_entry`. Interpretar `resolved` somente como link ELF contra o stub e entry point válido; não como resolução no loader do PS4. A etapa deve ser renomeada/registrada como `OPENORBIS_GNM_LINK_STUB=LINKED; SONY_RUNTIME_IMPORT=NOT_VALIDATED` em atualização posterior do workflow.

**Classificação:** evidência estática/forense do toolchain e correlação com falha anterior em hardware; sem nova execução no console. A auditoria não autoriza reintroduzir link estático GNM nem alterar o bloqueio de compute.

**Correção de comunicação CI:** commit `64629afaf49e5268718a11860f35b068d36a3a29` renomeou a etapa para `Link GCN8 OpenOrbis GNM stub (link-only; not a Sony import)` e alterou o marcador para `OPENORBIS_GNM_LINK_STUB=LINKED; SONY_RUNTIME_IMPORT=NOT_VALIDATED`. Isso impede que o sucesso de link seja interpretado como import Sony resolvido.