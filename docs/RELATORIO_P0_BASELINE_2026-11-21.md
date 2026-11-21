# Relatório P0 — baseline Neural Rendering
**Data:** 2026-11-21  
**Repositório:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Commit auditado:** `f602a436d6efe87e2a54fd7e5185c2e583c7473f`  
**Escopo:** evidência GitHub/CI, inventário estático e reconciliação dos artefatos GFX8.  
**Classificação geral:** baseline parcial; P0 permanece IN_PROGRESS.

## 1. Identidade e rastreabilidade
- HEAD confirmado pela referência Git: `f602a436d6efe87e2a54fd7e5185c2e583c7473f`.
- PR #5 permanece aberto e draft.
- Base do PR observada: `main` em `1db85e652f02eace895906387afb0e00eb745737`.
- O PR contém alterações de implementação, smoke e documentação. Não é release-ready.

## 2. CI host
- Workflow: `.github/workflows/host-tests.yml`, blob `cd9aa7ecd4fc5095fab002d0706b657828adb34c`.
- Run: [44862586387](https://github.com/demolidorreddragons/NR/actions/runs/44862586387), run number 1220.
- Job: `telemetry`, ID `136719063324`, conclusão `success`.
- Steps 3–12 passaram: validação de escapes do instalador, state machine, manifesto/SHA-256, referência numérica GCN8, offsets GNM, fórmulas de layout 71 blocos, intake verifier, CMake Configure, Build e Test.
- Classificação: **CI_VALIDATED / PASS** para esse commit e para o escopo executado.
- Artefatos do run host consultados: nenhum publicado. Logs completos ainda não foram anexados a este relatório; os nomes/estados de steps e conclusão foram obtidos pelo endpoint de jobs.

## 3. GFX8 shader build — evidência histórica, não HEAD
- Workflow: `.github/workflows/gcn8-shader-build.yml`, blob atual `36e8158e94ae37aff4f08435d5e26e6eb4e294a9`.
- Run confirmado: [44743915113](https://github.com/demolidorreddragons/NR/actions/runs/44743915113), job `build-gfx8-compute`, ID `136314194759`, conclusão `success`.
- Commit do run: `5141ed44fe3202dbdec78f0a69700d34ef76e039`, **267 commits atrás** do HEAD auditado. A comparação GitHub indica 267 commits à frente, sem commits atrás.
- O job passou build PSBC pinado, compilação de cinco shaders, validação SPIR-V, geração de manifest, conversão GFX8, parser C++ e verificador independente de bytes.
- Artifact ID `14631648737`, ZIP baixado e hash verificado localmente: `a722c2e67e8138b086cd8c6569850bb054277298339b4a23ea8e45d9ce337586`. O ZIP contém 22 arquivos, incluindo cinco `.sb`, cinco SPIR-V, GCN/ISA e `gnm-container-report.json`.
- Importante: os arquivos shader, workflow e backend sofreram alterações posteriores; o artifact histórico não comprova que o HEAD atual produza os mesmos bytes. **GFX8 build no HEAD = NOT_RUN / não comprovado.**
- O workflow atual tem `workflow_dispatch` e gatilhos de path específicos, mas não há ferramenta de dispatch disponível nesta sessão. Não foi forçado um commit irrelevante só para dispará-lo.

## 4. Inventário técnico reconciliado
### Presente no HEAD
- `platform/ps4/gcn8/compute_smoke.cpp/.h`: harness experimental de compute com alocação/mapa de direct memory, cópia de shader, construção manual de dois V# (input/output), DCB, CS state, ponteiro indirect resource table, dispatch, EOP fence, submit, polling e comparação de 64 floats.
- `platform/ps4/gcn8/gnm_resource_binding.cpp/.h`: writers locais PM4 para shader state, dispatch, indirect resource-table pointer e EOP fence.
- `platform/ps4/gcn8/smoke_main.cpp`: app runner chama o harness, com caminho alternativo startup-only.
- `tools/build_compute_smoke_pkg.sh`: constrói PKG de smoke compute ou startup, valida e extrai payload, compara SHA do shader e gera manifesto que declara hardware_status=not_run.
- `tests/test_gcn8_gnm_packets.cpp`: testes host para words/limites de pacotes PM4.
- `tests/gcn8_gnm_api_smoke.cpp`: compile/link-only API/ABI smoke.
- `CMakeLists.txt`: cinco CTest targets: telemetry writer, telemetry bridge, GCN8 compute contract, resource-table contract e shader-binary contract. O teste PM4 adicional é invocado em workflow específico; não consta como CTest padrão neste CMake.
### Correção importante da narrativa
O código tem um **harness experimental que contém uma tentativa de dispatch**, portanto não é correto dizer que não existe qualquer código de dispatch. Entretanto, ele não constitui backend GNM aprovado ou validado: não há evidência de execução no PS4 Pro, e os próprios contratos deixam endereçamento, descritores e coerência como não resolvidos.

## 5. Riscos técnicos identificados por inspeção estática
1. **GPU VA:** o harness deriva endereços GPU diretamente de `reinterpret_cast<uintptr_t>(memory.mapped)`. O contrato do projeto registra que essa equivalência não foi provada no PS4. Bloqueador.
2. **V# writable:** descriptor output é construído manualmente e marcado `GNM_MEMORY_UNCACHED`; não há prova de que os campos/flags sejam corretos para RW SSBO no firmware alvo. Bloqueador.
3. **Cache/coerência:** chamadas a `sceGnmFlushGarlic()` antes de submit e após fence são usadas como transição, mas não há evidência experimental de que cubram as operações CPU→GPU e GPU→CPU necessárias para estes buffers. Bloqueador.
4. **Retornos dos writers:** chamadas a `pne_gcn8_set_cs_shader`, `pne_gcn8_dispatch_direct` e `pne_gcn8_emit_eop_fence` não verificam diretamente o enum de retorno; o harness verifica apenas delta de dwords em parte desses caminhos. Precisa endurecer.
5. **Resource table:** o smoke usa dois descriptors nos offsets 0 e 64 (80 bytes totais) e checa pointer <4 GiB. O teste host confirma layout; não valida descriptor byte encoding ou GPU visibility.
6. **Fence/polling:** há timeout de aproximadamente 10 s e retenção de alocação em alguns caminhos de falha; requer revisão de semântica de submit, fence e lifetime antes de hardware.
7. **Revisão independente:** REA-5 continua pendente. Esta inspeção é documental/estática, não independente.

Nenhum destes riscos foi resolvido por esta auditoria. Não foi executado app, dispatch, instalação ou alteração no console.

## 6. Workflows e ferramentas
- `gcn8-shader-build.yml`: Ubuntu 24.04, instala build-essential, Python/Mako, glslang, SPIR-V tools e LLVM 18; clona dependências pinadas e compila PSBC. GFX8 host build reprodutível demonstrado no commit histórico indicado acima.
- `gcn8-ps4-probe.yml`: runner Windows self-hosted `[self-hosted, Windows, X64, ps4-lan]`; procura glslang/PSBC e verifica reachability TCP FTP 2121. Seu trigger é push em paths de shader/README/workflow. Não foi disparado nesta coleta; ferramentas no runner = **NOT_RUN**, não AUSENTES.
- O workflow GFX8 atual compila shader de smoke e pode gerar smoke PKG; nenhum workflow de instalação/execução do smoke foi disparado.

## 7. Matriz de estado
| Área | Estado | Evidência/limite |
|---|---|---|
| Branch/PR | COMPROVADO | HEAD e PR #5 consultados |
| CI host | PASS | Run 44862586387, job 136719063324 |
| CTest padrão | PASS no CI | Step Test passou; log detalhado não anexado |
| PM4 packet tests | Histórico/precisa reconciliar | fonte atual presente; run HEAD específico não confirmado |
| PSBC/GFX8 compiler | PASS histórico | run 44743915113, commit antigo |
| GFX8 build no HEAD | NOT_RUN | não há run vinculado ao HEAD nesta coleta |
| Runner Windows toolchain | NOT_RUN | probe não executado |
| Harness de dispatch | IMPLEMENTADO EXPERIMENTALMENTE | fonte presente; não validado no console |
| GPU VA / writable V# / cache | BLOCKED | contrato ainda não comprovado |
| GPU dispatch/readback PS4 Pro | NOT_RUN | nenhuma evidência hardware |
| Modelo/grafo completo 71 blocos | PARCIAL | fórmulas/layout host testados; execução completa ausente |
| REA-5 independente | NOT_RUN | relatório independente não existe |

## 8. Próximas ações desbloqueadas
- **GNM-001:** revisão estática e compile/link do harness no HEAD; corrigir checagem de retornos e documentar prova de GPU VA, V# e cache sem executar no console.
- **P0-001:** obter evidência completa do job CTest, identificar se workflow de shader pode ser executado por dispatch autorizado, e reconciliar todos os testes/workflows no HEAD.
- **E0-001:** fechar contrato de tarefas, dependências, responsáveis por papel, revisão independente e painel.
- Não iniciar teste no PS4 até o plano reversível ser revisado/aprovado e autorização específica ser obtida.

## 9. Declaração de revisão
**REVISÃO INTERNA — NÃO INDEPENDENTE.** Este relatório foi produzido a partir da inspeção do repositório e metadados GitHub/Actions. Não substitui auditoria por executor separado.
