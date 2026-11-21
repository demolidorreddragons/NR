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
