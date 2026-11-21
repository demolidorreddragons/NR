# Métricas de produtividade e qualidade — Neural Rendering

**Estado:** APROVADO PELO USUÁRIO — 2026-11-21

## Princípio
Métricas localizam gargalos; não premiam velocidade isolada. Nenhuma pontuação compensa gate reprovado, evidência ausente ou falta de teste de hardware.

## Indicadores
| Indicador | Definição |
|---|---|
| Lead time | Tempo entre início registrado e conclusão aprovada |
| Feedback CI | Commit até resultado utilizável |
| Build limpo/incremental | Duração por tipo de build |
| Retrabalho | Tarefas reabertas por falha/omissão |
| Falhas escapadas | Defeitos encontrados após gate |
| Flakiness | Reexecuções por resultados inconsistentes |
| Cobertura de evidências | Execuções com evidências exigidas completas |
| Falsos negativos de impacto | Testes/módulos afetados não previstos |
| Bloqueio por dependência | Tempo aguardando pré-requisito |
| Validação de hardware | Funcionalidades realmente testadas no PS4 Pro |

## Medição
Registrar baseline antes de otimizar; comparar períodos/builds equivalentes; usar mediana e p95 com tamanho da amostra; separar execução de espera; registrar ambiente/toolchain; não misturar host, CI, pacote e hardware; não ocultar retries ou tarefas reabertas.

O painel exibe valor, tendência, janela, amostra, fonte e limitações. Sem fonte rastreável, mostrar indisponível, nunca estimar como fato.
