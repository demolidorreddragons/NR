# Contrato do pipeline CI/QA — Neural Rendering

**Estado:** APROVADO PELO USUÁRIO — 2026-11-21

## Entradas obrigatórias
Commit SHA, branch/PR, arquivos alterados, toolchain/versões, configuração/target, dependências, catálogo de testes e artefatos de entrada.

## Etapas
1. Preflight: validar metadados, dependências e integridade dos inputs.
2. Análise: diff, análise estática e Impact Analyzer.
3. Build: compilar alvos afetados com cache verificável; executar build limpo conforme gate.
4. Testes rápidos: seleção justificada para feedback.
5. Suíte obrigatória: executar conjunto completo exigido pela mudança/gate.
6. Pacote: quando aplicável, gerar somente o PKG Full/Base previsto e validar categoria, SFO, conteúdo, extração e hashes.
7. Revisão: revisão cruzada e QA independente quando requerido, sem conflito.
8. Evidências: coletar logs, exit codes, artefatos, hashes, ambiente e limitações.
9. Decisão: PASS, FAIL, BLOCKED ou NOT_RUN por gate; teste não executado nunca é PASS.

## Seleção de testes
Mudança numérica exige oráculos e casos de borda; shader/toolchain exige compilação GFX8 e inspeção; GNM/runtime exige hardware para afirmar dispatch/readback; instalador/PKG exige QA estrutural e revisão de segurança; integração gráfica exige fixtures e validação no título-alvo. Release exige a suíte completa aplicável e gates correspondentes ao escopo declarado.

## Cache e reprodutibilidade
Chave inclui fontes, headers, dependências, compilador/toolchain e versão, flags, target, ambiente relevante e scripts. Registrar chave, hit/miss e hashes. Builds limpos de controle verificam reprodutibilidade; descartar cache incompleto.

## Relatório mínimo
Workflow/job IDs e URLs; commit/branch/PR; executor/ambiente; versões; etapas executadas/não executadas; comandos, exit codes, duração e logs; testes selecionados/justificativa; artefatos/SHA-256; resultados por gate; falhas/limitações/conflitos; decisão final.

## Independência e PS4
Quem implementa não aprova sozinho. QA recebe pacote neutro, pode reprovar e não modifica o produto. Sem executor separado, declarar revisão interna/não independente. Staging segue regras operacionais; instalação, desinstalação, ativação PRX, alteração de `plugins.ini` e reinício GoldHEN exigem autorização explícita. Staging não é instalação.
