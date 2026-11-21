# Registro de decisões — Neural Rendering

Este arquivo preserva decisões aprovadas e seus motivos. Novas decisões devem ser acrescentadas no topo, sem apagar o histórico. Uma proposta só passa a ser decisão quando aprovada pelo usuário ou quando decorre diretamente de uma regra já aprovada.

## DEC-2026-11-21-01 — Execução orientada por painel, gates e evidências

**Estado:** APROVADA PELO USUÁRIO  
**Data:** 2026-11-21  
**Decisão:** organizar o trabalho com painel central de execução, tarefas pequenas e sequenciais, critérios explícitos de conclusão, matriz de QA e evidências, revisão independente documentada, automação CI e registro permanente de decisões.

**Motivo:** reduzir repetição de contexto, retrabalho, declarações sem prova e tempo entre implementação e resultado verificável.

**Aplicação:**
- `PAINEL_EXECUCAO.md` mantém fila, prioridades, estado e gates.
- `MATRIZ_QA.md` define níveis de validação e evidências mínimas.
- `REGISTRO_DECISOES.md` preserva decisões e justificativas.
- O plano mestre continua sendo a referência de arquitetura e fases.
- Cada tarefa deve terminar com commit, testes aplicáveis, revisão, evidências e limitações.
- Após passar um gate, seguir para a próxima tarefa desbloqueada sem pedir confirmação redundante.

**Limites:** esta aprovação documental não autoriza instalação/desinstalação de PKG, ativação de PRX, reinício de GoldHEN ou alterações destrutivas no console. As regras operacionais existentes continuam prevalecendo.

**Primeiro marco técnico prioritário:** executar um compute GNM mínimo real no PS4 Pro (buffer → dispatch → readback) e comparar o resultado com um oráculo CPU. Não declarar esse marco concluído com testes apenas em host.

## Modelo para novas decisões

### DEC-AAAA-MM-DD-NN — Título
- **Estado:** PROPOSTA / APROVADA / REJEITADA / SUPERADA
- **Data:**
- **Decisão:**
- **Motivo:**
- **Impacto em arquivos/código:**
- **Evidências:**
- **Decisões substituídas:**


## DEC-2026-11-21-02 — Sistema de eficiência, automação e controle de qualidade

**Estado:** APROVADA PELO USUÁRIO  
**Data:** 2026-11-21

**Decisão:** aprovar integralmente fila inteligente por dependências, paralelismo seguro, contratos padronizados, Impact Analyzer, Test Selector, Failure Analyzer, Build Optimizer com cache verificável, Evidence Collector, painel de produtividade e ampliação dos testes numéricos/visuais.

**Motivo:** reduzir tempo de ciclo e retrabalho, antecipar detecção de falhas e melhorar qualidade e rastreabilidade.

**Ordem:** contratos/fila → análise de impacto/seleção de testes → análise de falhas → cache/build → coleta de evidências → métricas → regressão ampliada.

**Salvaguardas:** testes direcionados não removem suítes obrigatórias; cache não substitui build limpo; diagnósticos automáticos não aprovam nem corrigem sozinhos; métricas não substituem gates; host/CI não equivale a hardware. Regras de autorização do console e PKG Full/Base permanecem inalteradas.

**Documentos:** `PLANO_AUTOMACAO_E_EFICIENCIA.md`, `CONTRATO_PIPELINE_CI_QA.md`, `METRICAS_PRODUTIVIDADE_E_QUALIDADE.md`, Plano Mestre, Painel e Matriz QA.
