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


## DEC-2026-11-21-03 — Aviso e procedimento obrigatório após staging de PKG

**Estado:** APROVADA PELO USUÁRIO  
**Data:** 2026-11-21

**Decisão:** sempre que um PKG aprovado que realmente precise ser instalado for colocado na pasta correta do PS4, o assistente deve avisar explicitamente o usuário após concluir o readback final e verificar tamanho/SHA-256. O aviso deve vir acompanhado, na mesma mensagem, do procedimento manual de instalação.

**Conteúdo obrigatório do aviso:** nome e caminho exatos do PKG, tamanho, SHA-256, resultado do QA, indicação de instalação limpa ou atualização e pré-requisitos que cabem ao usuário (incluindo remoção manual de versão anterior, quando aplicável).

**Limites:** staging não é instalação. O assistente não instala automaticamente; instalação continua sendo uma operação separada que exige autorização explícita e específica. Não avisar que está pronto para instalar antes do staging/readback verificado. PKGs diagnósticos com gates técnicos não resolvidos não devem ser recomendados para instalação; nesse caso, comunicar o bloqueio em vez de fornecer instruções para instalar.

**Integração:** regra operacional em `docs/REGRAS_OPERACIONAIS_DO_PROJETO.md` §2.1 e regra obrigatória de execução em `AGENTS.md`.


## DEC-2026-11-21-04 — Coleta imediata de feedback pós-instalação e correção

**Estado:** APROVADA PELO USUÁRIO  
**Data:** 2026-11-21

**Decisão:** depois que o usuário instalar manualmente o PKG e enviar feedback, iniciar imediatamente a coleta não destrutiva das evidências disponíveis, correlacioná-las com o build/commit exato, diagnosticar e corrigir os problemas encontrados. Não encerrar com simples agradecimento ou status.

**Coleta:** logs, crash/coredumps, timestamps, metadados e hashes; usar runner Windows bridge, preservar originais e registrar origem/tamanho/SHA-256. Solicitar apenas dados específicos que não estejam acessíveis.

**Correção:** identificar causa com evidência, implementar correção mínima, executar testes e registrar resultados. Se for necessário novo teste no console, cumprir novamente os gates de QA, staging/readback, aviso e autorização específica de instalação/execução.

**Limites:** feedback não autoriza instalação adicional, execução destrutiva, ativação de PRX, reinício GoldHEN ou alteração de dados/configurações. A coleta deve ser read-only e preservar os dados do usuário.

**Integração:** `docs/REGRAS_OPERACIONAIS_DO_PROJETO.md` §2.2 e `AGENTS.md`.

## 2026-11-22 — OpenOrbis GNM linker stub is not a Sony runtime import

- Evidence: artifact `14752435074`, `libSceGnmDriver.so` SHA-256 `21d38ac5e7d79ddc9ee9bc7b9a84547f43ea3144883f6ac787d16704c1b5121f`.
- The OpenOrbis v0.5.4 file is a generic ELF64 shared-object **link stub** with 203 named exports, no `DT_SCE_*`/Sony NID metadata, and infinite-loop bodies; it is not a runtime implementation.
- Scope correction: the main PRX built with `create-fself --lib` **does** contain Sony NID import metadata. Its `sceGnmSubmitAndFlipCommandBuffers` import is `xbxNatawohc#C#C`, maps to `libSceGnmDriver` and matches the public shadPS4 registration. This is a static metadata PASS, not hardware validation.
- The older GNM-Link eboot built with `create-fself --eboot` had no GNM import metadata and a null GOT slot. This failure is specific to that artifact/path and must not be generalized to PRX `--lib` mode.
- Decision: keep compute eboot free of static `-lSceGnmDriver` and use module load plus `sceKernelDlsym` (already observed successful in GNM-Load-Only). The main PRX static import may remain; runtime resolution, hook ABI, and detour safety still require hardware validation.
- Limits: static OELF metadata was audited, but this is not a complete reverse engineering of the FW 14.02 loader.