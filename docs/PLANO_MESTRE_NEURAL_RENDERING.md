# Plano mestre de execução — Neural Rendering (PS4 Pro)

**Estado:** plano inicial publicado; execução iniciada pela auditoria da linha de base.  
**Alvo de hardware:** PS4 Pro, AMD GCN8.  
**Ambiente de referência informado no projeto:** firmware 14.02 e GoldHEN 2.4b18.14.  
**Branch de trabalho:** `feature/gcn8-numeric-kernels`.  
**PR de referência:** #5 — kernels numéricos GCN8 (WIP).

> Este documento é o plano de engenharia e os critérios de saída. Não declara que a equipe é formada por oito agentes autônomos: neste ambiente, as funções são papéis de trabalho coordenados. Teste realmente independente exige executor/contexto separado, acesso somente aos requisitos e artefatos, e relatório próprio.

## 1. Objetivo do produto

Construir e validar um pipeline de renderização neural que possa ser executado no PS4 Pro, com:

- inferência neural efetiva na GPU AMD GCN8;
- reconstrução/super-resolução com qualidade comparada a uma referência reproduzível;
- integração segura com o pipeline gráfico de um título-alvo, sem presumir captura universal de framebuffer;
- desempenho, memória, estabilidade e compatibilidade medidos em hardware;
- instalação e atualização por pacotes PS4 corretamente identificados e validados.

O repositório atualmente descreve um **scaffold de diagnóstico/captura**, não um renderer neural completo. A existência de shaders de referência ou de um PKG compilado não prova inferência neural nem funcionamento em jogo.

## 2. Equipe e responsabilidades

| Função | Responsabilidade | Não pode aprovar |
|---|---|---|
| 1. Arquiteto-chefe / coordenador | Arquitetura, requisitos, interfaces, prioridades e integração entre frentes | Sua própria decisão sem revisão |
| 2. Engenheiro-chefe de IA neural | Modelo, pesos, grafo, operadores, precisão e qualidade visual | Execução GCN8 ou release |
| 3. Engenheiro GPU AMD GCN8 | Shaders, kernels, ABI, memória, sincronização e otimização | Correção matemática sem referência |
| 4. Engenheiro de sistemas PS4 / Orbis | APIs, runtime, firmware, GoldHEN, PRX e integração de sistema | Seu próprio deploy |
| 5. Engenheiro de integração e release PS4 | SFO, GP4, PKG base/patch, dependências, instalação, atualização e artefatos | O PKG que ele próprio montou |
| 6. Testador independente | Testes cegos, reprodução, tentativa de reprovação e relatório | Não recebe justificativas internas do desenvolvedor |
| 7. Auditor técnico | Revisão de código, cadeia de build, hashes, logs, metadados e rastreabilidade | Não substitui teste de hardware |
| 8. Engenheiro de desempenho e estabilidade | GPU timestamps, latência, FPS, memória, variância e testes prolongados | Resultado sem dados brutos |

### Independência do QA

- O testador recebe apenas requisitos de aceitação, instruções de teste, binários e dados de entrada necessários.
- Não recebe a narrativa do desenvolvedor nem instruções para aprovar.
- Registra ambiente, versão, comandos, logs, hashes, resultados e falhas.
- Pode reprovar qualquer entrega, inclusive a coordenação.
- Engenharia corrige os defeitos; QA repete os testes sem editar o produto.
- Se não houver um executor separado disponível, o relatório deve dizer **“revisão interna — não independente”**. Nunca simular independência.

## 3. Fases e marcos

### Fase 0 — Auditoria da linha de base e congelamento de requisitos

**Objetivo:** descobrir exatamente o que existe, o que funciona e o que está bloqueado antes de ampliar o código.

Tarefas:
1. Registrar branch, commit, PR e workflows relevantes.
2. Inventariar fontes, shaders, pesos/modelos, ferramentas, scripts, instalador e artefatos.
3. Executar testes host existentes e registrar logs.
4. Verificar a disponibilidade e versões do compilador GLSL, PSBC GFX8, validador SPIR-V e toolchain Orbis.
5. Separar validação host, compilação GFX8, execução GPU no console e teste de instalação.
6. Documentar lacunas de modelo, pesos, grafo, backend GNM, readback e integração por título.

**Saída obrigatória:** matriz de capacidades com estado *comprovado / parcial / ausente / bloqueado*, hashes e evidências.

### Fase 1 — Toolchain e compilação GCN8 reproduzível

**Objetivo:** compilar os shaders para o alvo GFX8 sem depender de suposições.

Tarefas:
1. Fixar versões e hashes das ferramentas autorizadas.
2. Instalar/descobrir ferramentas no runner isolado; não executar binários desconhecidos no runner que alcança o PS4.
3. Compilar todos os shaders de compute para SPIR-V e binário GFX8.
4. Validar saída, logs e metadados de recursos: VGPR, SGPR, LDS e scratch, quando reportados.
5. Guardar artefatos e hashes por commit.

**Gate F1:** todos os shaders aprovados compilam de forma reproduzível; metadados e logs arquivados. Compilar não equivale a executar no console.

### Fase 2 — Backend mínimo de compute no PS4 Pro

**Objetivo:** provar um caminho buffer → compute → buffer no hardware.

Tarefas:
1. Implementar harness mínimo GNM com alocação, binding/descriptor, dispatch, barreiras/fences e readback.
2. Validar alinhamento, coerência de cache, sincronização e limites de buffer.
3. Executar kernel determinístico simples e comparar readback byte a byte.
4. Testar dimensões alinhadas e de borda, repetição e tratamento de erro.

**Gate F2:** evidência de dispatch real no PS4 Pro, logs, hashes e readback determinístico. Nenhum resultado de host pode substituir este gate.

### Fase 3 — Núcleo numérico e operadores

**Objetivo:** estabelecer correção antes de otimização.

Ordem:
1. E4M3FN encode/decode e fixtures de todos os 256 códigos.
2. GEMM FP32 contra referência de precisão dupla.
3. Semântica FP16 contra referência IEEE-754 binary16.
4. Testes de dimensões não múltiplas do tile, NaN, infinito, subnormais, overflow e empates.
5. Só então otimizar GEMM tiled, fusão de operadores e uso de LDS.

**Gate F3:** tolerâncias numéricas aprovadas, resultados repetíveis e comparação host/GPU documentada. Não chamar emulação FP16 de FP16 nativo sem evidência do compilador/hardware.

### Fase 4 — Modelo neural e grafo completo

**Objetivo:** converter o modelo real em um grafo executável no hardware disponível.

Tarefas:
1. Identificar arquitetura, licença/origem, formato e integridade dos pesos.
2. Inventariar os 71 blocos mencionados no escopo e mapear operadores, tensores, shapes e precisão.
3. Criar referência host independente para cada operador e para o grafo.
4. Implementar operadores GCN8 por prioridade de custo e dependência.
5. Validar camadas intermediárias, não apenas a imagem final.
6. Medir memória total, buffers temporários e estratégia de tiling.
7. Definir política de fallback quando uma operação não couber ou exceder o orçamento.

**Gate F4:** grafo completo executa e suas saídas respeitam tolerâncias previamente definidas. Se os pesos/modelos originais não estiverem disponíveis ou não puderem ser redistribuídos, parar e registrar o bloqueio; não inventar pesos.

### Fase 5 — Integração gráfica por título

**Objetivo:** conectar entrada e saída neural ao renderizador sem presumir acesso universal ao framebuffer.

Tarefas:
1. Escolher um título de teste e documentar seu pipeline/API.
2. Criar adaptador específico para obter o input em formato e momento válidos.
3. Garantir sincronização GPU, cache, dimensões, row pitch e lifetime dos recursos.
4. Integrar saída neural ao destino de apresentação.
5. Preservar caminho de bypass e recuperação em caso de erro.
6. Comparar imagem de entrada, saída e referência em cenas fixas.

**Gate F5:** integração demonstrada no título-alvo, sem corrupção, deadlock ou regressão não explicada.

### Fase 6 — Desempenho, estabilidade e qualidade

Medições mínimas:
- warm-up e pelo menos 100 dispatches cronometrados por cenário;
- mediana, p95, variância e tempo por quadro;
- FPS e impacto no frame budget;
- uso de memória, VGPR/SGPR/LDS/scratch quando disponível;
- erro absoluto máximo e RMS contra referência;
- conjunto fixo de imagens/cenas e métricas de qualidade apropriadas;
- teste prolongado e registro de travamentos, resets e leaks.

**Gate F6:** metas de qualidade, latência e estabilidade definidas antes do teste e atingidas com dados brutos anexados. Não comparar com DLSS sem mesmo input, resolução, métrica e método de medição.

### Fase 7 — Integração PS4, PKG e release

**Objetivo:** entregar pacote correto, identificável, instalável e recuperável.

Checklist:
1. Confirmar identidade do produto: TITLE_ID, CONTENT_ID, CATEGORY, APP_VER e SYSTEM_VER.
2. Distinguir claramente PKG base de PKG patch; patch deve estar associado a base compatível. Não chamar um base de híbrido.
3. Conferir SFO, GP4, payload, módulos runtime, PRX e dependências.
4. Executar validação de PKG, extração e comparação de arquivos/hashes.
5. Verificar upgrade path, instalação limpa, atualização, desinstalação e recuperação em ambiente de teste.
6. Fazer teste de instalação em console somente após QA do pacote e autorização explícita para a operação.
7. Manter staging, ativação de PRX e instalação como operações separadas.
8. Publicar release com changelog, hashes, logs e limitações conhecidas.

**Gate F7:** auditoria de pacote aprovada, QA independente aprovado e instalação/execução real verificadas. Build verde sozinho não libera release.

## 4. Critérios globais de bloqueio

Bloquear a promoção de fase quando houver:
- ferramenta ou binário sem origem/hash confiável;
- divergência numérica fora da tolerância;
- shader sem compilação para GFX8;
- ausência de prova de dispatch real quando a fase exigir hardware;
- falha de instalação, atualização, inicialização ou recuperação;
- alteração de PRX, `plugins.ini`, dados ou saves sem autorização;
- falta de evidência reproduzível;
- conflito entre requisitos e comportamento real.

## 5. Estado inicial conhecido ao publicar este plano

Com base na inspeção do branch e da documentação atual:

| Item | Estado inicial |
|---|---|
| Scaffold de diagnóstico/captura | Existe; não é renderer neural completo |
| Kernels E4M3FN / GEMM FP32 / FP16 de referência | Existem como bring-up |
| Teste host numérico | Existe; deve ser executado novamente na linha de base |
| Compilador GLSL / PSBC GFX8 no runner | Execução anterior reportou ferramentas ausentes; revalidar |
| Backend/harness GNM compute | Não identificado na documentação atual |
| Dispatch/readback compute no PS4 Pro | Não comprovado |
| Grafo neural de 71 blocos completo | Não implementado/comprovado |
| Integração temporal e adaptador de título | Não implementados/comprovados |
| PKG atual com renderer neural completo | Não existe evidência |
| Firmware 14.02 / GoldHEN runtime | Requer teste real e controlado |

O PR #5 está marcado como WIP e descreve explicitamente os kernels atuais como referências, sem backend GNM ou validação de compute no console.

## 6. Primeira execução iniciada

A primeira frente é a **Fase 0 — auditoria da linha de base**. A sequência de execução é:

1. confirmar o estado do branch e do PR;
2. executar testes host automatizados;
3. registrar o estado atual das ferramentas GFX8 e do harness GNM;
4. produzir uma lista priorizada de bloqueios com evidências;
5. só então iniciar alterações de implementação na Fase 1.

Nenhuma instalação, ativação de PRX, alteração de `plugins.ini`, remoção de app ou modificação de saves faz parte desta fase.

## 7. Regra de promoção

Uma fase só muda para **APROVADA** quando suas evidências forem anexadas e revisadas. Estados permitidos: **NÃO INICIADA**, **EM EXECUÇÃO**, **BLOQUEADA**, **REPROVADA**, **APROVADA**. Não usar “pronto” como sinônimo de “compilou”.


## 8. Registro da primeira auditoria — 20/11/2026

**Branch verificada:** `feature/gcn8-numeric-kernels`  
**Commit do plano:** `a63aefdfe6348c5dde1cb8fa7834ea27784c2f86`  
**PR #5:** aberto, ainda WIP.

| Verificação | Resultado observado | Consequência |
|---|---|---|
| Host CI | PASS no run [44668770203](https://github.com/demolidorreddragons/NR/actions/runs/44668770203), commit de código `ff7c50d` | Testes host/telemetria passaram nessa revisão; o run é anterior ao commit documental do plano |
| Build PRX | PASS no run [44668763351](https://github.com/demolidorreddragons/NR/actions/runs/44668763351) até a etapa de compilação PRX | PRX foi compilado; isso não prova carregamento no console |
| Build PKG | FAIL no run [44668763351](https://github.com/demolidorreddragons/NR/actions/runs/44668763351), etapa **Build installable PKG** | Não houve artefato PKG aprovado; motivo específico ainda precisa ser extraído do log dessa etapa |
| Probe GFX8 | FAIL no run [44667415701](https://github.com/demolidorreddragons/NR/actions/runs/44667415701), etapa de compilação | A descoberta de ferramentas terminou, mas a compilação não; execução anterior reportou ausência de `glslangValidator` e `opengnm-psbc` |
| GNM compute | Não localizado/implementado no escopo atual do PR | Bloqueia prova de dispatch/readback até existir harness |
| Execução GPU no PS4 Pro | Não comprovada | Não há medição de latência/GFLOP/s nem readback de compute |
| Renderer neural completo | Não comprovado; README declara scaffold de diagnóstico | Não promover a release como renderer neural |

### Bloqueios prioritários encontrados

1. **B0 — PKG:** obter o log integral da etapa que falhou no run `44668763351`; corrigir a causa antes de qualquer novo pacote candidato.
2. **B1 — Toolchain GFX8:** localizar ou provisionar versões confiáveis de `glslangValidator` e `opengnm-psbc`, registrar origem e hashes, e repetir a compilação.
3. **B2 — Backend compute:** projetar e implementar harness GNM mínimo, separado do instalador/PRX de diagnóstico.
4. **B3 — Referência neural:** localizar e validar o modelo, pesos e especificação do grafo de 71 blocos; sem isso, não inventar parâmetros ou afirmar port completo.
5. **B4 — QA independente:** definir executor separado e protocolo de teste cego; até lá, revisões são internas e devem ser rotuladas como tal.

### Estado da Fase 0

**EM EXECUÇÃO — auditoria parcial concluída.** O branch/PR, documentação, workflows e principais kernels foram inspecionados. Ainda falta recuperar o log detalhado do PKG e consolidar inventário completo de ferramentas/modelos. O próximo trabalho técnico é resolver B0 e B1 sem instalar nada no console.



### Atualização da auditoria — falha de PKG diagnosticada e correção aplicada

O log integral do job `136064398853` no run `44668763351` revelou a causa exata:

```
Generated icon0.png is not a valid PNG.
```

O PRX foi compilado com sucesso. A geração do pacote parou na validação da assinatura PNG do ícone. A inspeção de `tools/make_icon.py` mostrou que a assinatura estava escrita com escapes duplicados, fazendo a comparação procurar bytes literais de barra invertida em vez da assinatura PNG real.

**Correção aplicada:** commit `4f8b82a8dbea1e213236b03e04e5616122939ae6`. A verificação agora compara os primeiros oito bytes com `bytes.fromhex("89504e470d0a1a0a")`, evitando ambiguidade de escaping.

**Estado após correção:** correção de código publicada no branch; ainda não há build PKG pós-correção confirmado. O próximo gate é executar novamente o workflow de PRX/PKG e inspecionar a validação e extração do pacote. Até esse resultado, não declarar o bloqueio B0 resolvido nem afirmar que existe um novo PKG funcional.

## 9. Resultado do primeiro build pós-correção — 20/11/2026

**Workflow:** [PS4 PRX and PKG build — run 44669637925](https://github.com/demolidorreddragons/NR/actions/runs/44669637925)  
**Commit de origem:** `a14bb99dd9eef131f3768cd2c9beca0fb3a08119`  
**Resultado do job:** SUCCESS.

### Evidências observadas no log

- Compilação do `PkgTool.Core`: concluída.
- Build da biblioteca de hooks GoldHEN: concluído.
- Build do PRX: concluído.
- Build do arquivo de fontes GoldHEN: concluído.
- Build do PKG: concluído.
- Verificação de hashes e assinaturas do PKG: aprovada em todos os itens reportados pelo validador.
- Extração do PKG: concluída.
- Verificação de presença dos arquivos extraídos: aprovada para `eboot.bin`, `ps4_neural_engine.prx`, `keystone`, `right.sprx`, `libc.prx` e `libSceFios2.prx`.
- Upload do artefato: concluído.

**PKG produzido:** `PS4-Neural-Engine-006.pkg`  
**SHA-256 do PKG, conforme log do build:** `7bd4b2b31fcadcc549ccb81186cad56dca515a02bb3384a53d09f5b7799d6422`  
**Artefato GitHub Actions:** [ps4-neural-engine-build — artifact 14599800224](https://github.com/demolidorreddragons/NR/actions/runs/44669637925/artifacts/14599800224)  
**SHA-256 do ZIP do artefato:** `e00172c21c2afee6a60fb1a999273028500afeedf1376ac28813288207ad77c5`

### Interpretação correta

A correção do gerador de ícone e o bloqueio de geração/validação estrutural do PKG estão resolvidos para este commit: o pacote foi criado, seus hashes/assinaturas foram aceitos pelo validador utilizado, seu payload foi extraído e os arquivos esperados foram encontrados.

Isto **não** comprova instalação, atualização, inicialização ou execução no PS4 Pro. Também não comprova que o PRX foi carregado pelo GoldHEN nem que existe inferência neural/GPU compute funcional. Nenhuma instalação ou ativação no console foi realizada nesta etapa.

### Atualização dos gates

| Item | Estado após run 44669637925 |
|---|---|
| B0 — geração, integridade e extração estrutural do PKG | **RESOLVIDO para o commit testado** |
| Instalação/atualização no PS4 Pro | **PENDENTE — teste controlado e autorização explícita** |
| B1 — toolchain GFX8 | **BLOQUEADO — ainda sem compilação de shaders GFX8 comprovada** |
| B2 — backend GNM compute | **AUSENTE / NÃO COMPROVADO** |
| B3 — modelo, pesos e grafo neural completo | **PENDENTE de inventário e validação** |
| B4 — executor de QA realmente independente | **PENDENTE** |

A Fase 0 permanece **EM EXECUÇÃO**. O build PKG bem-sucedido fecha somente a verificação estrutural do pacote; não promove o produto a release aprovada.

## 10. Auditoria estática do PKG 006 — 20/11/2026

Relatório detalhado: [docs/RELEASE_AUDIT_006.md](RELEASE_AUDIT_006.md).

**Decisão:** PKG 006 NÃO APROVADO PARA INSTALAÇÃO no PS4 Pro nesta etapa.

Achados:
1. O workflow independente `ps4-base-pkg-qa-006.yml` filtrava somente builds da branch `main`; por isso não auditou o artefato do run 44669637925, produzido na branch de trabalho.
2. O instalador substitui o PRX anterior sem backup/rollback transacional se a atualização de `plugins.ini` falhar.
3. Executar o eboot altera armazenamento persistente do GoldHEN e `plugins.ini`; não é uma operação somente de leitura.
4. A cópia byte a byte do PRX não valida ABI, compatibilidade ou capacidade de carregamento.
5. O PRX permanece um scaffold de diagnóstico; não existe backend GNM compute nem inferência neural comprovada.

Ajustes já publicados:
- Workflow de QA 006 agora inclui `feature/gcn8-numeric-kernels` no filtro de branches.
- `docs/PKG_INSTALLER.md` registra explicitamente que o instalador não deve ser executado antes de QA e autorização.

**Nova execução solicitada pelo push:** [build run 44670068072](https://github.com/demolidorreddragons/NR/actions/runs/44670068072), pendente na fila no momento deste registro. O QA independente só será acionado se esse build terminar com sucesso.

Nenhum FTP de escrita, staging, instalação, alteração de `plugins.ini`, ativação de PRX ou reinício do GoldHEN foi realizado.


## 8. Governança de execução aprovada (2026-11-21)

A execução passa a ser controlada também pelos seguintes documentos versionados:

- [Painel de execução](PAINEL_EXECUCAO.md): fila priorizada, estado das frentes, gates e Definition of Done.
- [Matriz de QA e evidências](MATRIZ_QA.md): níveis de validação, verificações e evidências obrigatórias.
- [Registro de decisões](REGISTRO_DECISOES.md): decisões aprovadas, justificativas e histórico.
- [Regras operacionais](REGRAS_OPERACIONAIS_DO_PROJETO.md): limites de autorização, preservação de dados, PKG e fluxo de staging.

**Regra de execução:** concluir tarefas em unidades pequenas e verificáveis; após aprovação de um gate, avançar para a próxima tarefa desbloqueada. Nenhum gate pode ser considerado aprovado sem as evidências definidas na matriz QA. Validação host/CI não equivale a validação no PS4 Pro. Esta governança não amplia as autorizações para instalação, ativação de PRX, reinício de GoldHEN ou operações destrutivas.


## 9. Estrutura de agentes e independência aprovada (2026-11-21)

A estrutura passa a contemplar dez especialidades, incluindo separação explícita entre engenharia GCN8 e backend GNM/Orbis, QA independente, auditoria técnica independente, desempenho/estabilidade e segurança/cadeia de suprimentos. A independência não é presumida pelo nome do papel: exige executor/contexto separado, pacote neutro de requisitos e artefatos, ausência de conflito e relatório reproduzível.

- [Protocolo de independência, revisão cruzada e qualidade dos agentes](PROTOCOLO_INDEPENDENCIA_AGENTES.md)
- [Painel de execução](PAINEL_EXECUCAO.md)
- [Matriz de QA e evidências](MATRIZ_QA.md)

O protocolo define dez papéis, revisão cruzada, barreiras contra viés, contrato obrigatório de tarefa, estados PASS/FAIL/BLOCKED/NOT_RUN, pontuação auxiliar e relatório independente. A pontuação nunca substitui gates obrigatórios. Papéis organizacionais não significam dez agentes autônomos ativos.
