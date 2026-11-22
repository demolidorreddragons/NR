# Painel de execução — Neural Rendering

**Repositório:** `demolidorreddragons/NR`  
**Branch de referência:** `feature/gcn8-numeric-kernels`  
**Alvo:** PS4 Pro / AMD GCN8  
**Estado inicial deste painel:** reorganização aprovada; backend GNM real ainda não comprovado no console.

Este painel é a fonte operacional para retomar o trabalho. Atualizar após cada entrega relevante. Não marcar tarefa como concluída sem evidência rastreável.

## Estado geral

| Frente | Estado | Critério para avançar |
|---|---|---|
| Auditoria e documentação | Em consolidação | Inventário reconciliado com código, commits e workflows |
| Toolchain GFX8 | Host/CI parcialmente validado | Build reproduzível, logs, hashes e metadados |
| Backend GNM mínimo | Pendente / não validado em hardware | Buffer → dispatch real → readback no PS4 Pro |
| Kernels numéricos | Parcial em host; GPU pendente | Oráculos CPU/GPU e tolerâncias aprovadas |
| Modelo e grafo neural | Pendente | Modelo/pesos legítimos, operadores e grafo completo validados |
| Integração por título | Pendente | Adaptador específico e bypass seguro demonstrados |
| Desempenho/estabilidade | Pendente | Métricas brutas e metas pré-definidas aprovadas |
| Release Full/Base | Pipeline existente; cada release exige QA | PKG `gde`, integridade, QA e staging verificados |

## Fila prioritária

### P0 — Fechar auditoria da linha de base
- [x] Conferir branch, commit, PR #5 e workflows relevantes (snapshot em 2026-11-21).
- [ ] Reconciliar inventário de código, shaders, instalador, artefatos e testes com o estado atual do repositório.
- [x] Registrar resultado real do CI host na matriz; reconciliação integral ainda em andamento.
- [ ] Identificar dependências e bloqueios sem presumir ferramentas ou recursos inexistentes.

**Saída:** baseline versionada, com estados comprovado / parcial / ausente / bloqueado.

### P1 — Primeiro compute GNM real no PS4 Pro
- [ ] Implementar harness mínimo de alocação/mapeamento de memória GPU.
- [ ] Construir descritores V# válidos e documentar ABI/registers.
- [ ] Implementar cache coherency, sincronização, submissão e conclusão.
- [ ] Executar kernel determinístico simples.
- [ ] Fazer readback e comparar com oráculo CPU.

**Gate:** resultado reproduzível no console, com logs, commit, hashes, dados de entrada/saída e comparação numérica. Host tests não satisfazem este gate.

### P2 — Kernels numéricos
- [ ] Validar E4M3FN em todos os 256 códigos.
- [ ] Validar GEMM FP32 contra referência de precisão dupla.
- [ ] Validar FP16 conforme IEEE-754 binary16.
- [ ] Cobrir bordas, NaN, infinito, subnormais, overflow e dimensões irregulares.
- [ ] Otimizar somente após correção comprovada.

### P3 — Modelo e grafo
- [ ] Identificar modelo, licença, origem, formato e hash dos pesos.
- [ ] Inventariar os 71 blocos previstos no escopo e seus operadores/shapes.
- [ ] Criar referência host e validar resultados intermediários.
- [ ] Portar operadores para GCN8 e validar o grafo completo.

### P4 — Integração gráfica por título
- [ ] Selecionar título-alvo e documentar pipeline.
- [ ] Criar adaptador específico de entrada/saída.
- [ ] Validar sincronização, cache, dimensões, row pitch e lifetime.
- [ ] Manter bypass/recuperação seguros.

### P5 — Desempenho e estabilidade
- [ ] Definir metas antes de medir.
- [ ] Medir mediana, p95, variância, frame budget, memória e qualidade.
- [ ] Executar teste prolongado e registrar falhas/leaks/resets.

### P6 — Release
- [ ] Gerar somente um PKG Full/Base (`gde`) por padrão.
- [ ] QA de pacote, integridade, hashes e relatório.
- [ ] Após aprovação, realizar staging e readback no PS4 conforme regras operacionais.
- [ ] Não instalar, desinstalar, ativar PRX ou reiniciar GoldHEN sem autorização explícita.

## Definition of Done (DoD)

Uma tarefa só pode ser encerrada quando:
1. implementação e escopo estão identificados por commit;
2. compilação aplicável foi executada e seu resultado registrado;
3. testes relevantes foram executados com comandos e logs;
4. revisão de código foi registrada, distinguindo revisão interna de independente;
5. evidências e hashes estão anexados ou referenciados;
6. limitações e cenários não testados estão explícitos;
7. o nível de validação está corretamente classificado: `STATIC_REVIEW`, `HOST_VALIDATED`, `CI_VALIDATED`, `PACKAGE_QA` ou `PS4_HARDWARE_VALIDATED`.

Não promover fase se o gate anterior não foi satisfeito.

## Política de execução contínua

Depois que uma tarefa passa seu gate, selecionar a próxima tarefa desbloqueada da fila e prosseguir sem solicitar uma confirmação redundante. Parar somente quando houver bloqueio real, risco/destruição, necessidade de autorização do usuário ou decisão técnica fora do escopo aprovado. Essa política não autoriza instalação, ativação de PRX, reinício de GoldHEN nem qualquer ação proibida em `REGRAS_OPERACIONAIS_DO_PROJETO.md`.

## Documentos de controle

- [Regras operacionais](REGRAS_OPERACIONAIS_DO_PROJETO.md)
- [Matriz QA](MATRIZ_QA.md)
- [Registro de decisões](REGISTRO_DECISOES.md)
- [Plano mestre](PLANO_MESTRE_NEURAL_RENDERING.md)
- [Contrato de runtime GCN8](GCN8_COMPUTE_RUNTIME_CONTRACT.md)
- [Plano de teste reversível GCN8](GCN8_PS4_PRO_REVERSIBLE_COMPUTE_TEST_PLAN.md)


## Estrutura de agentes aprovada — 2026-11-21

A equipe passa a ter dez especialidades organizacionais: coordenação; IA neural; GPU AMD GCN8; GNM/Orbis; sistemas PS4; build/release; QA independente; auditoria técnica independente; desempenho/estabilidade; segurança/cadeia de suprimentos.

- [Protocolo de independência, revisão cruzada e qualidade](PROTOCOLO_INDEPENDENCIA_AGENTES.md)
- [Plano mestre](PLANO_MESTRE_NEURAL_RENDERING.md)
- [Matriz QA](MATRIZ_QA.md)

Toda tarefa nova deve ter contrato de tarefa, critérios mensuráveis, responsáveis/revisores sem conflito, evidências e estado explícito. QA/auditoria devem receber pacote neutro e podem reprovar sem interferência. Se não houver executor separado, classificar a revisão como interna/não independente. Os dez itens são papéis definidos, não prova de dez agentes autônomos ativos.


## Sistema de eficiência e automação — aprovado em 2026-11-21

Especificação: [Plano de automação e eficiência](PLANO_AUTOMACAO_E_EFICIENCIA.md).

### Fila de implantação
- [ ] E0 — Contratos de tarefa, fila priorizada, dependências e estados explícitos.
- [ ] E1 — Impact Analyzer: mapear módulos, interfaces e testes potencialmente afetados por cada diff.
- [ ] E2 — Test Selector: executar testes rápidos relevantes, preservando suítes completas obrigatórias nos gates.
- [ ] E3 — Failure Analyzer: agrupar logs/falhas e apresentar hipóteses rastreáveis, sem corrigir/aprovar sozinho.
- [ ] E4 — Build Optimizer: medir baseline, cache determinístico e builds limpos de controle.
- [ ] E5 — Evidence Collector: vincular commit, workflow/job, ambiente, comandos, logs, artefatos e SHA-256.
- [ ] E6 — Painel automático: tempos, bloqueios, retrabalho, cobertura de evidências e estado de hardware.
- [ ] E7 — Regressão ampliada: precisão numérica, casos-limite, camadas intermediárias, qualidade visual e estabilidade.

### Regras
- Paralelizar somente tarefas independentes, com dependências resolvidas e interfaces definidas.
- Testes direcionados aceleram feedback, mas não substituem suítes obrigatórias, QA independente ou gates de hardware.
- Cache só pode ser reutilizado com chave que inclua todos os inputs relevantes; registrar hit/miss e hashes.
- Após um gate aprovado, avançar para a próxima tarefa segura e desbloqueada; parar apenas por bloqueio real ou autorização necessária.


## Execução P0/E0 — 2026-11-21

- Baseline inicial: [Relatório P0](RELATORIO_P0_BASELINE_2026-11-21.md), commit de criação `9f4fb59fecf21d2ec82e811cf5116dd9ade4c1b0`.
- [E0-001 — contratos/fila](https://github.com/demolidorreddragons/NR/issues/14): **IN_PROGRESS**. Contrato formalizado; fila automatizada e grafo de dependências ainda não implementados.
- [P0-001 — baseline](https://github.com/demolidorreddragons/NR/issues/15): **IN_PROGRESS**. Branch/PR/workflows confirmados; CI host PASS; GFX8 artifact disponível apenas em commit histórico.
- [GNM-001 — auditoria do smoke](https://github.com/demolidorreddragons/NR/issues/16): **IN_PROGRESS**. Fonte contém harness experimental com tentativa de dispatch, ainda sem build verificado no HEAD ou execução hardware.
- CI host HEAD `f602a436d6efe87e2a54fd7e5185c2e583c7473f`: run `44862586387`, job `136719063324`, success.
- GFX8 compile histórico: run `44743915113`, commit `5141ed44fe3202dbdec78f0a69700d34ef76e039`; não promover ao HEAD (267 commits de diferença).
- Bloqueadores runtime: GPU VA do mapeamento, V# writable, cache/coerência, submissão/fence/readback e revisão independente.
- Ferramentas no runner Windows: **NOT_RUN**; probe não executado.
- Console: nenhuma instalação, ativação, dispatch ou alteração persistente realizada.


### Atualização após revisão do harness — 2026-11-21

- Commit: `edbb7f8e017016fc826b2fa231747c56ccaaee94`.
- Correção: o harness agora verifica e registra os retornos de `pne_gcn8_set_cs_shader`, `pne_gcn8_dispatch_direct` e `pne_gcn8_emit_eop_fence`, além das contagens de dwords.
- [Workflow GFX8](https://github.com/demolidorreddragons/NR/actions/runs/44863096651): em execução; inclui teste PM4, compilação dos seis shaders e build/QA do PKG de smoke.
- [Workflow PRX/PKG](https://github.com/demolidorreddragons/NR/actions/runs/44863096656): em execução; inclui compile/link smoke GNM e build do PRX.
- Até ambos terminarem, resultado da correção = **PENDING CI**. Mesmo com sucesso, isso não valida dispatch no hardware.


## Atualização CI — commit `734b3aa1` — 2026-11-21

- Commit auditado: `734b3aa1ec9dd3d465e90c57c4f2f223fd9f7902`.
- Host Tests: [run 44869051638](https://github.com/demolidorreddragons/NR/actions/runs/44869051638), job `136739200322`: success; Configure, Build e Test passaram, incluindo contratos GCN8 resource table/EOP PM4, offsets GNM, layout tensorial 71-block e intake verifier.
- PRX/PKG: [run 44869051627](https://github.com/demolidorreddragons/NR/actions/runs/44869051627): build e QA independente success; jobs `136739407561` e `136739799751`. Jobs de patch foram `skipped`, conforme política Full/Base.
- GFX8: [run 44869051626](https://github.com/demolidorreddragons/NR/actions/runs/44869051626): compilador PSBC construído; seis shaders SPIR-V/GFX8 emitidos; parser C++ e verificações estruturais/CRC passaram; auditoria byte-level passou sob interpretação OpenGNM.
- Artefatos GFX8: `nr-gcn8-spirv` ID `14693780653` (17,561 bytes); `nr-gcn8-compute-shaders-gfx8` ID `14693426426` (26,071 bytes; ZIP SHA-256 `0830f0a77b5d3b8827719609d1508eabe9a8ec8ce57e31c3ef1deca6f8a8035c`).
- Smoke PKG: artifact `nr-gcn8-compute-smoke-ps4-pkg` ID `14693371031` (25,267,698 bytes). Compute payload PASS; PKG SHA-256 `d8f7935493013722cf37834713b9de3c8f81c077d3bb5e777e83269e1ad966ad`; manifest SHA-256 `659a8d491b2feb71a3e6a0d49ea577748068592f4dea157c3d3b9cee22edbb89`. Startup-only PKG payload PASS, compute shader ausente por projeto; SHA-256 `578964f74b3f9335a1d467f53f84a15dfafdb5320bb2b6bf54e334e67e5b36ca`.
- Limite crítico: `runtime_semantics=UNRESOLVED`; OpenGNM aponta slot start 88 dwords e fpPS4 84 dwords. `pointer_hypotheses_agree=false`. Nenhum dispatch/readback no PS4 ocorreu. Não promover GNM runtime a validado.
- Próxima tarefa: resolver a semântica do `chunkUsageBaseOffsetDwords`/`numInputUsageSlots` com evidência de implementação/ABI e testes binários direcionados, sem escolher hipótese por conveniência. Em paralelo, manter bloqueado o teste de hardware até contrato de memória/V# e plano reversível aprovados.


## Atualização da auditoria InputUsage — 2026-11-21

- Relatório: [Auditoria OpenGNM × fpPS4 × PSBC](RELATORIO_AUDITORIA_INPUT_USAGE_OFFSET_2026-11-21.md).
- Commit do relatório: `020388e973dc4c3e982475e0cc3bc209123ba0e7`.
- A inspeção do PSBC pinned `28697f007862427ebec54388ec972ce6e3545912`, `libpsbc/psbc_compile.c:793–809`, confirma que o produtor calcula `chunkusagebaseoffsetdwords` como distância até o início da tabela de slots. Isso coincide com os bytes emitidos e com a interpretação OpenGNM usada pelo verificador NR.
- fpPS4 observado em `04cefd43e6fddd1ab033e7980cd356d14c964905` subtrai adicionalmente `numInputUsageSlots`; para os seis shaders (um slot cada), o candidato fica 4 bytes antes da tabela.
- Decisão: parser/offset não alterados; evidência favorece contrato do produtor PSBC, mas ABI Sony/runtime ainda não comprovado. `runtime_semantics=UNRESOLVED`.
- Host Tests [run 44870307627](https://github.com/demolidorreddragons/NR/actions/runs/44870307627), job `136743115128`: success; validação aritmética GNM, Configure, Build e Test passaram.
- Console, staging e instalação: não executados.


## Fechamento da rodada Host CI — 2026-11-21 05:24 UTC

Os três workflows Host Tests acionados pelos commits da auditoria InputUsage terminaram **SUCCESS**:

| Commit | Run / Job | Configure | Build | Test |
|---|---|---|---|---|
| `020388e973dc4c3e982475e0cc3bc209123ba0e7` | [44870664622](https://github.com/demolidorreddragons/NR/actions/runs/44870664622) / `136744219097` | PASS | PASS | PASS |
| `aae561144e4d814ff088a09c62848f4cfea943fc` | [44870671014](https://github.com/demolidorreddragons/NR/actions/runs/44870671014) / `136744239138` | PASS | PASS | PASS |
| `124073b77a96733fd1a4a748f5f5ee6cc660d5e3` | [44870678044](https://github.com/demolidorreddragons/NR/actions/runs/44870678044) / `136744261320` | PASS | PASS | PASS |

Em todos os três jobs passaram também os validadores do instalador, referência numérica/tiled mapping GCN8, aritmética InputUsage GNM, fórmulas do tensor 71-block e intake verifier. Não houve falha de CI nesta rodada. São três execuções Host CI; não representam três validações independentes do produto nem validação PS4.

Estado após fechamento: a investigação estática InputUsage continua sem conclusão de ABI/runtime; parser e offset permanecem inalterados. Não houve execução de shader build/PKG para estes commits documentais, staging ou operação no console. O próximo gate técnico permanece a evidência ABI independente para resolver a divergência OpenGNM/PSBC × fpPS4.


## Escopo de hardware upstream PSBC — auditoria complementar — 2026-11-21

A documentação pública upstream do PS4-OpenGNM/opengnm-psbc declara validação em hardware apenas para shaders VS+PS em PS4 base GFX7/FW 9.00, com 600 frames. O próprio plano lista teste de compute shader em hardware como trabalho futuro. Portanto, esse relato não cobre compute GFX8/PS4 Pro nem FW 14.02 e não resolve a divergência InputUsage do NR. Classificação: evidência documental upstream, não validação reproduzida/independente do NR.

A pesquisa não justificou mudança de código: parser e offset seguem intactos; runtime_semantics=UNRESOLVED.


## Auditoria externa e correção da dependência GNM — 2026-11-22

- Auditoria registrada em [RELATORIO_AUDITORIA_PARTICIPACAO_EXTERNA_E_GNM_IMPORT_2026-11-22.md](RELATORIO_AUDITORIA_PARTICIPACAO_EXTERNA_E_GNM_IMPORT_2026-11-22.md).
- PRs de delegação #9 (modelo), #10 (GNM) e #11 (QA clean-room) foram criados pela conta proprietária do repositório; as consultas de reviews não retornaram revisões formais. Não há evidência verificável de entrega/revisão externa nesses registros. Isso não prova ausência absoluta de atividade fora do GitHub.
- Achado concreto: o link do modo compute ainda incluía `-lSceGnmDriver`, apesar da resolução dinâmica dos exports. A dependência foi removida e o ELF agora falha fechado se apresentar `DT_NEEDED libSceGnmDriver` ou símbolo `sceGnm*` indefinido.
- Correção: commit [83d15edf1c3411752f75fdc98172f9e2f8094865](https://github.com/demolidorreddragons/NR/commit/83d15edf1c3411752f75fdc98172f9e2f8094865).
- Relatório: criação em `39747f9668f6e59eb21aa75f947cc86c7ac835d0`, complemento técnico em `a798a1c628cbb7e25eff5b414334ab3bdffad3d8`.
- **Validação CI do commit corrigido `83d15edf`: PASS no nível ELF/build.** Run [45005750367](https://github.com/demolidorreddragons/NR/actions/runs/45005750367), jobs `137156845303` (shader pipeline) e `137157762811` (compute smoke PKG), ambos SUCCESS. Log confirmou `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only`, `COMPUTE_SMOKE_PKG_PAYLOAD=PASS`, PKG compute SHA-256 `97b866dfa5cc5bbb655417a4d6ec10700dd22a50055d6e22f06813dde793e5fb`. Host Tests [45005750309](https://github.com/demolidorreddragons/NR/actions/runs/45005750309), job `137156845061`, SUCCESS (Configure/Build/Test); coleta logger read-only job `137156844849`, SUCCESS. Artefato PKG ZIP ID `14747203138`, 25.268.050 bytes, SHA-256 `630e1a9a1f314837796fae560953733b808116aa9fb1b173cf76119d950c5612`. Limite: audit cobre ELF, não OELF/Sony import metadata. O ZIP foi baixado e hash conferido, mas não contém ELF/OELF/map (só seus hashes no manifest), impedindo re-inspeção independente; PKG foi validado/extraído pelo próprio pipeline, sem job clean-room independente. Macro `PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE` não definida no build normal: harness retorna BLOCKED antes de carregar GNM/dispatch. PKG diagnóstico **não deve ser instalado**. Nenhum staging ou operação de console ocorreu.
- GPU VA/V# writable, InputUsage runtime, cache/coerência e revisão independente permanecem BLOCKED/NOT_RUN. Dispatch/readback PS4 não executado; nenhum PKG staged ou instalado.

## Auditoria binária compute — run 45014637939

- Commit: `b2baac371acfbc0b10d17f5ad15cf5f1d7c77171`; workflow [45014637939](https://github.com/demolidorreddragons/NR/actions/runs/45014637939): jobs de shader GFX8 e PKG SUCCESS.
- Artefato `nr-gcn8-compute-smoke-ps4-pkg` ID `14751630143`, 25.433.004 bytes; ZIP SHA-256 `8674616f7855d217f57ae7d7002b2cc9581cfa0e03a67b52c8081c21f667b907` verificado após download.
- ELF/OELF/MAP preservados e auditados: ELF SHA-256 `e8e6cc962e23f11a3f4eb93f288667fd9142be1b18873c0df9cc79cdecc793c4`; OELF `51473828592f0ad3b5eb11837f91211fb59bd3755a0f9edbfa529285a9cec506`; MAP `00b938daa630e0070179db063c10c959cdc8661f3d00f00956a3ebdd24012136`.
- PKG compute Full/Base (`gde`), 6.619.136 bytes, SHA-256 `9d00d221e0888b0d2543bee9fecf3ab93fd52796e36856b898b3919d5d37bac0`; PkgTool payload PASS e ícone oficial hash aprovado.
- OELF `PT_SCE_DYNLIBDATA` e tags Sony indicam somente `libkernel.prx` / módulo `libkernel`; 19 NIDs correspondem a imports de sistema. Nenhum import GNM, módulo GNM ou símbolo GNM indefinido no ELF/OELF. Auditoria estática CI: `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS`.
- O harness compute não define `PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE`; retorna BLOCKED antes de load/dispatch. **PKG diagnóstico não deve ser instalado.**
- Nível: inspeção binária e payload CI PASS; revisão externa clean-room e hardware `NOT_RUN`; GPU VA/V#, InputUsage, coerência/cache e dispatch/readback continuam bloqueados. Nenhum staging/instalação/execução no PS4.
- Relatório detalhado: [Auditoria de participação externa e dependência estática GNM](RELATORIO_AUDITORIA_PARTICIPACAO_EXTERNA_E_GNM_IMPORT_2026-11-22.md), seção 2.2.