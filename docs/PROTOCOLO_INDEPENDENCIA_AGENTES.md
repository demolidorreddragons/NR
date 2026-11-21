# Protocolo de independência, revisão cruzada e qualidade dos agentes

**Estado:** aprovado pelo usuário em 2026-11-21.  
**Aplicação:** todas as tarefas de engenharia, QA, auditoria, desempenho, segurança e release do Neural Rendering.

## 1. Princípio de independência

Auditores e QA não trabalham para confirmar a narrativa, as decisões anteriores ou a expectativa de sucesso do projeto. Trabalham para verificar se os requisitos foram atendidos e para encontrar razões técnicas para reprovar uma entrega.

O auditor conhece os requisitos, contratos, critérios de aceitação e instruções necessárias para reproduzir os testes. Não recebe a defesa do desenvolvedor, avaliações prévias como evidência, nem instruções para aprovar. Decisões anteriores do projeto não são prova de correção.

## 2. Estrutura de dez especialidades

| Nº | Papel | Responsabilidade principal |
|---|---|---|
| 01 | Arquiteto-chefe / coordenador | Requisitos, arquitetura, dependências, prioridades, interfaces e fila de execução |
| 02 | Especialista em IA e renderização neural | Modelo, pesos, grafo, operadores, precisão e qualidade visual |
| 03 | Especialista AMD GCN8 | ISA, shaders, kernels, registradores, LDS, memória e otimização |
| 04 | Especialista GNM/Orbis | Backend compute, alocação GPU, descritores, command buffers, dispatch, fences e readback |
| 05 | Especialista em sistemas PS4 | Firmware, GoldHEN, PRX, inicialização, compatibilidade e estabilidade de sistema |
| 06 | Engenheiro de build e release | Toolchain, CI, integração, SFO/GP4/PKG, integridade e staging |
| 07 | QA independente | Testes cegos, reprodução, regressão, tentativa ativa de reprovação e relatório |
| 08 | Auditor técnico independente | Código, arquitetura, hashes, cadeia de build, dependências, logs e rastreabilidade |
| 09 | Especialista em desempenho e estabilidade | FPS, frame time, latência, memória, variância, regressões e testes prolongados |
| 10 | Especialista em segurança e cadeia de suprimentos | Origem/licença de componentes, vulnerabilidades, dependências, integridade e proveniência |

Estes são papéis organizacionais. Não afirmar que existem dez agentes autônomos ativos sem evidência de execução separada. Uma ferramenta ou sessão única não se torna independente apenas por receber um título diferente.

## 3. Barreiras contra viés e conflito de interesse

1. Desenvolvedor não aprova sozinho o próprio trabalho.
2. Coordenador não pode anular uma reprovação técnica sem nova evidência e revisão registrada.
3. QA e auditoria recebem pacote neutro: requisitos, critérios, commit, artefatos, hashes, ambiente e procedimento mínimo.
4. Não compartilhar a argumentação do desenvolvedor nem a conclusão esperada antes da avaliação independente.
5. O revisor não altera o artefato auditado; registra achados e recomendações.
6. Se o revisor participou da implementação ou da decisão examinada, declarar conflito e classificar a revisão como interna/não independente.
7. Se não houver executor/contexto separado, registrar explicitamente **REVISÃO INTERNA — NÃO INDEPENDENTE**. Nunca simular independência.
8. O auditor pode reprovar qualquer entrega, inclusive decisões do coordenador e documentos aprovados anteriormente.

## 4. Revisão cruzada mínima

| Área desenvolvida | Revisão técnica cruzada |
|---|---|
| Modelo, grafo e operadores neurais | GCN8 + QA independente |
| Kernels GCN8 | GNM/Orbis + auditor técnico |
| Backend GNM | GCN8 + QA independente |
| Sistemas PS4/PRX | Auditor técnico + QA independente |
| Build, PKG e release | Auditor técnico + QA independente |
| Desempenho | QA independente + especialista de desempenho |
| Segurança e dependências | Auditor técnico + especialista de segurança |

A tabela define cobertura técnica, não dispensa o gate independente. Nenhum autor deve ser o único revisor.

## 5. Contrato obrigatório de tarefa

Antes de iniciar, cada tarefa registra:
- ID, objetivo e escopo explícito;
- responsável e revisores sem conflito;
- commit/base e dependências;
- critérios de aceitação mensuráveis definidos antes dos resultados;
- entregáveis esperados (código, binário, testes, documentação);
- comandos/procedimentos reproduzíveis;
- evidências obrigatórias (logs, entradas, saídas, hashes, ambiente);
- riscos, permissões e operações proibidas;
- definição de pronto e condições de bloqueio.

A entrega inclui resumo factual, arquivos alterados, testes executados e não executados, resultados, limitações, hash e vínculo com a tarefa. Não declarar PASS com base apenas em compilação ou inspeção visual.

## 6. Estados e gates

Estados permitidos: `NOT_STARTED`, `IN_PROGRESS`, `PASS`, `FAIL`, `BLOCKED`, `NOT_RUN`.

- `PASS`: todos os critérios obrigatórios passaram com evidências.
- `FAIL`: um ou mais critérios falharam.
- `BLOCKED`: dependência, acesso ou autorização impede a verificação.
- `NOT_RUN`: teste não executado; nunca equivale a PASS.

Falha crítica, requisito obrigatório ausente, evidência não reproduzível ou conflito de interesse não tratado bloqueia promoção. Nota agregada nunca compensa um gate reprovado.

## 7. Pontuação auxiliar de qualidade

Quando fizer sentido, atribuir pontuação de 0 a 100:
- correção funcional: 30%;
- evidências e reprodutibilidade: 20%;
- qualidade do código: 15%;
- desempenho: 15%;
- segurança e estabilidade: 10%;
- documentação: 10%.

A pontuação é diagnóstica e não substitui os gates. Critérios não aplicáveis devem ser marcados N/A e o cálculo renormalizado, sem inventar nota. Falhas críticas mantêm a entrega reprovada independentemente da pontuação.

## 8. Relatório independente obrigatório

O relatório deve conter:
1. escopo e critérios recebidos;
2. commit, artefato e SHA-256;
3. ambiente e versões;
4. procedimentos e comandos exatos;
5. testes executados e não executados;
6. resultados e evidências;
7. achados classificados por severidade;
8. limitações e ameaças à validade;
9. conflitos de interesse declarados;
10. decisão final: PASS, FAIL ou BLOCKED.

## 9. Integridade do processo

- Toda conclusão precisa apontar para evidência verificável.
- Hipótese, inferência e fato observado devem ser identificados separadamente.
- Discordâncias técnicas são registradas, não apagadas por consenso forçado.
- Não fabricar logs, hashes, aprovações, revisores ou testes.
- Reabertura de tarefa reprovada exige correção vinculada a novo commit e nova rodada de QA/auditoria.
- Segurança e operações no console continuam subordinadas às regras operacionais e às autorizações explícitas já vigentes.
