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
