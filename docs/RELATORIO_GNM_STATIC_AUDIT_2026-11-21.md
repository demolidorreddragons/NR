# GNM-001 — relatório de auditoria estática do smoke GCN8

Data: 2026-11-21  
Base de código: `2cae50d15c3a4532a330e2e75dc0e1fde21d1759`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**  
Estado: **IN_PROGRESS** — revisão estática parcial concluída e integridade dos artefatos de CI verificada; revisão independente e hardware NOT_RUN.

## Escopo

Inspeção de `compute_smoke.cpp`, `gnm_resource_binding.cpp/.h`, helpers e headers do OpenGNM pinado (`4b295ca54c82c83acf308d1c646a2dfa9ae57350`). Sem instalação, deploy de PRX, alteração de `plugins.ini`, ativação de plugin ou dispatch no PS4.

## Correção já aplicada

O writer agora captura e verifica os retornos de:
- `pne_gcn8_set_cs_shader`;
- `pne_gcn8_dispatch_direct`;
- `pne_gcn8_emit_eop_fence`.

A verificação também confere as quantidades de dwords emitidas. Isso corrige o tratamento inconsistente dos retornos, mas não valida a semântica dos pacotes em firmware.

## Comparação estática com OpenGNM

1. **SET_SH_REG de compute:** o writer local emite o bit de compute engine (`PKT3_SHADER_TYPE_S(1)`), offset relativo a `SI_SH_REG_OFFSET`, valores dos registradores e NOP final. A sequência e os tamanhos são compatíveis com `setpersistentregisterrange` e `sceGnmDrawCmdSetCsShader` no commit OpenGNM pinado.
2. **DISPATCH_DIRECT:** o writer local corresponde ao pacote de nove dwords observado em `sceGnmDrawCmdDispatchDirect`: header compute, três dimensões, ordered-append flags e NOP de três dwords.
3. **EOP fence:** o writer local usa `EVENT_WRITE_EOP`, endereço dividido, destination memory, data32 e interrupt após write-confirm. A forma corresponde ao helper OpenGNM de EOP. O evento e a política de flush ainda precisam de validação no PS4.
4. **Alocação/mapeamento:** o helper OpenGNM `sceGnmDirectMemoryAllocate` chama `sceKernelAllocateDirectMemory` e `sceKernelMapDirectMemory`, guardando separadamente o offset de direct memory e o ponteiro mapeado. O harness usa o ponteiro mapeado como endereço candidato para os recursos. A implementação pública confirma o fluxo de alocação/mapeamento, mas não constitui prova suficiente da interpretação GPU-VA em todos os firmwares/condições.
5. **V# buffer:** `GnmBuffer` no header pinado tem 16 bytes e campos de endereço, stride, records, formato e memory type. O harness constrói dois descritores manualmente; a saída usa `GNM_MEMORY_UNCACHED`. A existência do enum e o layout não provam que o descritor RW/flags e a política de cache são corretos para o lowering SSBO do shader.
6. **Submissão/sincronização:** o harness verifica o retorno de `sceGnmSubmitCommandBuffers` e `sceGnmSubmitDone`, aguarda o valor de fence e só então compara o readback. Em timeout/falha de submit-done mantém a alocação até o fim do processo para evitar liberar memória potencialmente em uso. `sceGnmFlushGarlic` é chamado antes da submissão e após fence; suficiência de visibilidade CPU↔GPU continua sem comprovação.

## Riscos ainda abertos

- **R1 — GPU VA:** provar por fonte técnica autoritativa e/ou teste controlado que o endereço mapeado é utilizável como GPU VA no firmware-alvo. A guarda de endereço de 32 bits para a tabela impede dispatch quando a condição exigida pelo shader não é satisfeita.
- **R2 — V# writable:** revisar campo a campo a codificação RW e memory type em relação ao código gerado GFX8 e validar em hardware.
- **R3 — cache/coerência:** determinar flush/invalidate corretos para buffers de entrada, saída e fence; o uso de `sceGnmFlushGarlic` isoladamente ainda é hipótese.
- **R4 — runtime:** nenhum command buffer foi submetido no PS4; nenhum fence ou readback GPU foi observado.
- **R5 — ciclo de vida:** validar semântica de `sceGnmSubmitDone`, fence, timeout e liberação em firmware; a retenção conservadora atual evita liberar memória nos caminhos de timeout.

## CI associado à correção

Commit: `2cae50d15c3a4532a330e2e75dc0e1fde21d1759`.

- GFX8 + smoke PKG: [run 44863812886](https://github.com/demolidorreddragons/NR/actions/runs/44863812886) — concluído `success`; artefatos inspecionados e hashes verificados (ver seção de evidência abaixo).
- PRX/PKG + QA: [run 44863812891](https://github.com/demolidorreddragons/NR/actions/runs/44863812891) — concluído `success` nos jobs de build e QA independente.

A correção do Content ID startup está no mesmo commit: `IV0000-PNEU00003_00-GCN8STARTUP00000` (36 caracteres), com validação de comprimento e formato canônico antes de construir o PKG. CI PASS não significa validação de hardware.

## Decisão

**Não autorizado para hardware.** Manter o smoke harness como experimental. Próxima etapa: concluir os dois workflows, inspecionar os artefatos/hashes, fechar a revisão dos descritores/cache e submeter o relatório a revisão GCN8/QA sem conflito. O plano reversível no console permanece draft e não aprovado; não instalar nem executar este PKG.


## Evidência de artefatos — CI 44863812886

Estado atualizado após conclusão dos workflows: **CI PASS; integridade dos artefatos PASS; hardware NOT_RUN**. Base do build: commit `2cae50d15c3a4532a330e2e75dc0e1fde21d1759`.

- [Workflow GFX8 + smoke PKG](https://github.com/demolidorreddragons/NR/actions/runs/44863812886) — concluído `success`.
- Artifact `nr-gcn8-compute-shaders-gfx8`, ID `14691306027`, 26.065 bytes; SHA-256 do ZIP baixado: `4fa78cf2dbdc0a5e74bb7d4a7bc2d755a8829ea2403e5e62d6b38e0cd1a40379`. ZIP íntegro; contém seis módulos shader em formatos .sb/.spv/.gcn.bin/.isa e relatórios/manifestos. A presença e compilação não provam execução GPU.
- Artifact `nr-gcn8-spirv`, ID `14691206276`, 17.558 bytes; SHA-256 do ZIP baixado: `1e0853482fb3175abe58b5022e38e53c7120cf94c79c0830bb70e7fc9a82f169`. ZIP íntegro; contém seis fontes compute e seis módulos SPIR-V.
- Artifact `nr-gcn8-compute-smoke-ps4-pkg`, ID `14691072641`, 25.267.787 bytes; SHA-256 do ZIP baixado: `c9bd40d90cce73f28f48cf0ce4228bdf61c56da7502924061af0376e4004597d`. ZIP íntegro. Contém variantes compute e startup, cada uma com PKG de 6.619.136 bytes, `package.gp4`, `param.sfo` e manifesto.
- Compute PKG: SHA-256 interno conferido com manifesto `fa9efc2cc7457ff005a05bc697fb437ec930ef33fa86de1879841e86181f04fb`; Content ID `IV0000-PNEU00002_00-GCN8SMOKE0000000` (36 caracteres), também presente no param.sfo.
- Startup PKG: SHA-256 interno conferido com manifesto `0446077f7751965f482287498a9f0eb56793940c249e00302a138d3ff32969ed`; Content ID `IV0000-PNEU00003_00-GCN8STARTUP00000` (36 caracteres), também presente no param.sfo.
- Os dois manifestos declaram explicitamente `hardware_status=not_run`. Os hashes foram recalculados a partir dos bytes baixados; os três ZIPs passaram `zipfile.testzip()`.

## Decisão após inspeção

A integridade e a consistência dos artefatos estão verificadas. Isso fecha apenas a etapa de empacotamento/artefatos de CI. **Não aprova execução no PS4**: GPU VA, V# writable, coerência de cache e ciclo de vida submit/fence/readback continuam bloqueadores de backend. O PKG smoke não deve ser instalado nem executado; o plano reversível de console segue draft/não aprovado. Próxima tarefa: obter revisão técnica independente dos bloqueios GNM e preparar evidência de fonte para GPU VA/V# e cache, sem tocar no console.
