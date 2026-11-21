# Matriz de QA e evidências — Neural Rendering

## Objetivo

Definir o que deve ser testado, qual evidência comprova o resultado e qual nível de validação pode ser declarado. QA de host/PC não é prova de funcionamento no PS4 Pro.

## Níveis de validação

| Nível | Significado | Não comprova |
|---|---|---|
| `STATIC_REVIEW` | Inspeção estática/revisão de fonte | Build ou execução |
| `HOST_VALIDATED` | Testes executados em host/PC | GPU/firmware do PS4 |
| `CI_VALIDATED` | Workflow CI concluído com logs | Teste físico no console |
| `PACKAGE_QA` | Estrutura, categoria, conteúdo e integridade do PKG validados | Instalação ou runtime |
| `PS4_HARDWARE_VALIDATED` | Teste reproduzido no PS4 Pro, com evidência direta | Funcionalidades não cobertas pelo teste |

## Matriz de verificações

| Área | Verificação | Evidência obrigatória | Nível mínimo |
|---|---|---|---|
| Código | Diff, revisão, análise estática | Commit, escopo, achados e decisão | STATIC_REVIEW |
| Build | Compilação C++/PRX | Workflow/job, comando, log, artefato e hash | CI_VALIDATED |
| Testes host | CTest e testes unitários | Lista de testes, exit code, log e commit | HOST_VALIDATED |
| Shaders | Compilação para GFX8 e inspeção de metadados | Binários, logs, ferramentas/versões e SHA-256 | CI_VALIDATED |
| Runtime GNM | Alocação, binding, dispatch, fence e readback | Logs do console, dados de entrada/saída, hash e comparação com oráculo | PS4_HARDWARE_VALIDATED |
| Numérico | E4M3FN, GEMM, FP16 e casos-limite | Vetores de teste, referência, tolerâncias e resultados | HOST_VALIDATED + PS4_HARDWARE_VALIDATED para GPU |
| Modelo/grafo | Camadas e inferência completa | Origem/licença/hash do modelo, fixtures e comparações intermediárias/finais | PS4_HARDWARE_VALIDATED |
| Integração gráfica | Captura/adaptador e composição por título | Título/versão, cenário reproduzível, imagens e logs | PS4_HARDWARE_VALIDATED |
| Desempenho | Latência, FPS, memória e qualidade | Dados brutos, metodologia, warm-up, mediana/p95 e metas prévias | PS4_HARDWARE_VALIDATED |
| PKG | Categoria, SFO, conteúdo e extração | Relatório do validador, metadados, listagem e hashes | PACKAGE_QA |
| Staging | Upload, readback e promoção | Logs do workflow, caminho, tamanho e SHA-256 antes/depois | PACKAGE_QA + staging verificado |
| Instalação | Instalação limpa/atualização | Autorização explícita e logs/resultado no console | PS4_HARDWARE_VALIDATED |
| Revisão independente | Avaliação por revisor separado | Identidade/escopo do revisor, commit, achados e decisão | Relatório independente |

## Modelo obrigatório de registro por execução

Para cada execução de QA, registrar:
- ID/data da execução;
- commit e branch;
- artefato e SHA-256;
- ambiente, firmware e versões de ferramentas;
- comando/procedimento exato;
- resultado e exit code;
- logs e arquivos de entrada/saída;
- desvios, falhas e limitações;
- nome/papel do executor e se a revisão foi independente;
- nível de validação atingido.

## Regras de aprovação

1. Resultado sem log/evidência reproduzível não é aprovado.
2. Teste não executado é `NOT_RUN`, nunca PASS.
3. Falha é `FAIL`; bloqueio externo é `BLOCKED`; não há promoção automática nesses estados.
4. Aprovação de host/CI não pode ser promovida a aprovação de hardware.
5. Um PKG transferido para staging não está instalado.
6. Revisão interna não pode ser rotulada como independente.
7. Toda tolerância numérica e meta de desempenho deve ser definida antes de observar o resultado.
8. QA deve tentar encontrar falhas e pode reprovar qualquer entrega.

## Estado inicial conhecido

- Os testes host/CI e QA de pacote previamente documentados são evidências limitadas aos respectivos ambientes.
- O PKG Full/Base 011 teve staging e readback SHA-256 verificados, sem instalação.
- O backend GNM executável, dispatch real, readback GPU, inferência neural completa e validação de desempenho no PS4 Pro permanecem **não comprovados**.
- A revisão independente REA-5 permanece pendente até existir relatório real de revisor independente.

Atualizar esta matriz com IDs de workflow/job, commits, hashes e links de logs a cada nova execução. Não inventar resultados retrospectivos.


## Independência e revisão cruzada — regra aprovada em 2026-11-21

- QA independente e auditoria técnica não herdam a narrativa, defesa ou aprovação prévia do desenvolvedor.
- Recebem requisitos, critérios, commit, artefatos, hashes, ambiente e procedimento mínimo; conclusões anteriores não contam como evidência.
- Desenvolvedor não aprova sozinho o próprio trabalho; conflito de interesse deve ser declarado.
- Revisor não modifica o artefato auditado e pode emitir PASS, FAIL ou BLOCKED.
- Sem executor/contexto separado, registrar **REVISÃO INTERNA — NÃO INDEPENDENTE**.
- Falha crítica ou critério obrigatório não atendido bloqueia a promoção, independentemente de qualquer pontuação agregada.

Consultar [Protocolo de independência, revisão cruzada e qualidade dos agentes](PROTOCOLO_INDEPENDENCIA_AGENTES.md) para papéis, matriz de revisão, contrato de tarefa, critérios e modelo de relatório.


## Automação, seleção de testes e evidências — aprovado em 2026-11-21

| Controle | Verificação | Evidência obrigatória | Restrição |
|---|---|---|---|
| Impact Analyzer | Diff → módulos/interfaces/testes afetados | Mapa, versão e revisão de falsos negativos | Não dispensa testes sozinho |
| Test Selector | Seleção de testes rápidos por impacto | Testes selecionados/omitidos, motivo, comando e resultado | Suítes completas continuam obrigatórias nos gates |
| Failure Analyzer | Agrupamento e diagnóstico de falhas | Logs de origem, hipótese e confirmação humana | Não aplica correção não revisada |
| Build Optimizer | Cache e build incremental | Chave, inputs, hit/miss, hashes e build limpo de controle | Cache não prova correção |
| Evidence Collector | Rastreabilidade da execução | Commit, ambiente, comandos, logs, artefatos e SHA-256 | Não sintetiza evidência ausente |
| Regressão visual/numérica | Comparação com fixtures/oráculos | Dataset, métricas, tolerâncias prévias e resultados | Host não aprova GPU/hardware |

Automação acelera feedback, mas não reduz o nível mínimo de validação. `PS4_HARDWARE_VALIDATED` exige execução real e evidência direta no PS4 Pro. QA independente permanece separado da implementação.


## Evidências CI/artefatos — snapshot 2026-11-21

| ID | Commit | Run / Job | Resultado | Nível e limite |
|---|---|---|---|---|
| P0-HOST-001 | `f602a436d6efe87e2a54fd7e5185c2e583c7473f` | [44862586387](https://github.com/demolidorreddragons/NR/actions/runs/44862586387) / `136719063324` | success; steps de validação, Configure, Build e Test passaram | CI_VALIDATED; host apenas |
| P0-GFX8-HIST-001 | `5141ed44fe3202dbdec78f0a69700d34ef76e039` | [44743915113](https://github.com/demolidorreddragons/NR/actions/runs/44743915113) / `136314194759` | success; cinco shaders convertidos e verificados | CI_VALIDATED histórico; 267 commits atrás do HEAD |
| P0-GFX8-HEAD-001 | `f602a436d6efe87e2a54fd7e5185c2e583c7473f` | nenhum run confirmado | NOT_RUN | não declarar compilação GFX8 no HEAD |
| P0-WIN-TOOLS-001 | HEAD atual | probe self-hosted não executado | NOT_RUN | não classificar ferramentas como ausentes |
| GNM-HW-001 | HEAD atual | nenhum dispatch no PS4 Pro | NOT_RUN | sem GPU VA, V# writable, cache ou readback comprovados |

Artifact histórico GFX8: ID `14631648737`; ZIP SHA-256 `a722c2e67e8138b086cd8c6569850bb054277298339b4a23ea8e45d9ce337586`. ZIP contém cinco `.sb`, cinco SPIR-V, cinco GCN e cinco ISA, além de relatório e manifest. O hash foi recalculado após download. Como o commit do artifact é 267 commits anterior ao HEAD e arquivos de shader/backend/workflow mudaram nesse intervalo, ele não comprova os bytes atuais.

**Correção de inventário:** `platform/ps4/gcn8/compute_smoke.cpp` contém um harness experimental que tenta alocação/mapa, V# manual, PM4, dispatch, EOP fence, submit e readback. Isso é implementação-fonte experimental, não validação de runtime. GPU VA derivado do mapeamento, V# writable e política de cache permanecem bloqueadores não resolvidos.

Relatório completo: [RELATORIO_P0_BASELINE_2026-11-21.md](RELATORIO_P0_BASELINE_2026-11-21.md). Revisão do relatório: **REVISÃO INTERNA — NÃO INDEPENDENTE**.


## Evidências CI atualizadas — commit `734b3aa1` — 2026-11-21

| ID | Run / Job | Resultado | Evidência e limite |
|---|---|---|---|
| NR-HOST-734B3 | [44869051638](https://github.com/demolidorreddragons/NR/actions/runs/44869051638) / `136739200322` | success | Configure/Build/Test; contratos resource-table/EOP PM4, offsets GNM, layout 71-block e intake; host/CI apenas |
| NR-PKG-734B3 | [44869051627](https://github.com/demolidorreddragons/NR/actions/runs/44869051627) / `136739407561` + `136739799751` | success | PRX/PKG Full/Base build e QA independente; jobs de patch skipped; não comprova instalação/runtime |
| NR-GFX8-734B3 | [44869051626](https://github.com/demolidorreddragons/NR/actions/runs/44869051626) / `136739746528` | success | PSBC, SPIR-V e seis shaders GFX8 compilados; parser, containers/CRC e byte-level report cross-check passaram; runtime semantics unresolved |
| NR-GCN8-SMOKE-PKG-734B3 | [44869051626](https://github.com/demolidorreddragons/NR/actions/runs/44869051626) / `136740942984` | success | Compute smoke PKG validado/extraído; SHA-256 `d8f7935493013722cf37834713b9de3c8f81c077d3bb5e777e83269e1ad966ad`; startup-only separado; nenhum hardware test |

**Achado bloqueador mantido:** o relatório binário registra `runtime_semantics=UNRESOLVED`: início do input-usage slot sob hipótese OpenGNM = 88 dwords; hipótese fpPS4 = 84 dwords; as hipóteses não concordam. O PASS é estrutural/byte-level sob interpretação explicitada, não aprovação semântica de runtime. Dispatch GPU, GPU VA, descritores V# writable, cache/coerência e readback permanecem NOT_RUN/BLOCKED para hardware.


## Auditoria InputUsage e Host CI — 2026-11-21

| ID | Commit | Run / Job | Resultado | Nível e limite |
|---|---|---|---|---|
| NR-HOST-OFFSET-6403 | `6403956658223b71bfb9af33c737f529cbc88f17` | [44870307627](https://github.com/demolidorreddragons/NR/actions/runs/44870307627) / `136743115128` | success; validação de aritmética GNM, Configure, Build e Test passaram | CI_VALIDATED; host apenas |

Relatório estático: [RELATORIO_AUDITORIA_INPUT_USAGE_OFFSET_2026-11-21.md](RELATORIO_AUDITORIA_INPUT_USAGE_OFFSET_2026-11-21.md), commit `020388e973dc4c3e982475e0cc3bc209123ba0e7`.

**Conclusão limitada:** PSBC pinned `28697f007862427ebec54388ec972ce6e3545912` calcula explicitamente o campo como distância de `OrbShdr` ao início da tabela de slots; bytes dos seis shaders concordam. fpPS4 observado subtrai novamente a contagem de slots. A evidência favorece o contrato de emissão PSBC/OpenGNM, mas não substitui ABI independente nem validação no PS4. Manter `runtime_semantics=UNRESOLVED`; não declarar runtime validado.
