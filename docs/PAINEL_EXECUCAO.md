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
- [ ] Conferir branch, commit, PRs e workflows ativos.
- [ ] Reconciliar inventário de código, shaders, instalador, artefatos e testes com o estado atual do repositório.
- [ ] Atualizar a matriz de QA com comandos reproduzíveis e resultados reais.
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
