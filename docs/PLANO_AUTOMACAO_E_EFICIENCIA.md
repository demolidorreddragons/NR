# Plano de automação e eficiência — Neural Rendering

**Estado:** APROVADO PELO USUÁRIO — 2026-11-21  
**Branch:** `feature/gcn8-numeric-kernels`

## Objetivo
Reduzir tempo de ciclo, retrabalho e investigação manual, aumentando previsibilidade, correção e rastreabilidade. Aceleração nunca remove testes obrigatórios, revisão independente ou gates técnicos.

## Fila e contrato de tarefa
Toda tarefa deve registrar ID, objetivo, escopo incluído/excluído, responsável, revisores sem conflito, commit-base, dependências, prioridade/justificativa, critérios mensuráveis, comandos, entregáveis, evidências, riscos/permissões e Definition of Done.

Estados: `NOT_STARTED`, `IN_PROGRESS`, `PASS`, `FAIL`, `BLOCKED`, `NOT_RUN`.

Priorizar caminho crítico, risco, desbloqueio de outras tarefas, valor técnico e esforço. Não priorizar apenas tarefas fáceis. Paralelizar somente frentes independentes com interfaces definidas; declarar arquivos/áreas de alteração para reduzir conflitos.

## Automações
1. **Impact Analyzer:** recebe diff/commit e grafo de dependências; lista módulos, APIs, artefatos e testes potencialmente afetados. Medir falsos negativos. Nunca dispensa sozinho testes obrigatórios.
2. **Test Selector:** seleciona testes rápidos para feedback; registra seleção, omissões e justificativas. Suítes completas continuam obrigatórias em merge, gates e release conforme a matriz QA.
3. **Failure Analyzer:** agrupa falhas por assinatura, etapa, arquivo e commit; apresenta hipóteses e reprodução. Correções exigem revisão e regressão; não mascara flakiness nem converte FAIL em PASS.
4. **Build Optimizer:** medir baseline; cache key inclui fontes, headers, dependências, toolchain/versão, flags, target e scripts. Registrar hit/miss, chaves e hashes. Executar builds limpos de controle.
5. **Evidence Collector:** associa tarefa a commit, branch, workflow/job, ambiente, versões, comandos, exit codes, logs, artefatos, hashes e limitações. Evidência ausente fica explicitamente ausente.
6. **Painel:** fila, dependências, bloqueios, estado, responsável, idade e próximo passo; atualizar somente com eventos reais.
7. **Regressão técnica:** ampliar fixtures FP32/FP16/E4M3FN, NaN, infinitos, subnormais, overflow, arredondamento, alinhamento, dimensões irregulares, camadas intermediárias e imagens determinísticas.

## Ordem de implantação
E0 contratos/fila; E1 Impact Analyzer; E2 Test Selector; E3 Failure Analyzer; E4 cache/build; E5 Evidence Collector; E6 painel/métricas; E7 regressão ampliada. Cada etapa exige diff/PR, testes, revisão e evidências.

## Salvaguardas
Testes direcionados não substituem suítes completas. Nenhuma automação aprova seu próprio resultado. QA/auditoria independentes permanecem separados. Cache não é fonte de verdade. Host/CI não prova execução GCN8 no console. Operações no PS4 seguem as regras operacionais; não instalar/desinstalar, ativar PRX, alterar `plugins.ini` ou reiniciar GoldHEN sem autorização. Entrega padrão permanece um único PKG Full/Base (`gde`).

## Critério de sucesso
Comparar baseline e período posterior: tempo de feedback CI, build limpo/incremental, diagnóstico, retrabalho, falhas escapadas, cobertura de evidências e falsos negativos. Declarar ganhos somente com amostras comparáveis e método registrado.
