# Auditoria do offset InputUsage — OpenGNM × fpPS4 × PSBC

Data: 2026-11-21  
Branch NR: `feature/gcn8-numeric-kernels`  
Commit-base da investigação: `6403956658223b71bfb9af33c737f529cbc88f17`  
Classificação: **INVESTIGAÇÃO ESTÁTICA — SEM VALIDAÇÃO DE RUNTIME/HARDWARE**.

## Resultado

A divergência observada nos seis shaders smoke é de **um slot (4 bytes / 1 dword)**: início da tabela em 88 dwords pela interpretação usada pelo OpenGNM/PSBC e candidato em 84 dwords pela aritmética literal do fpPS4. Não são quatro dwords de diferença.

A inspeção adicional do compilador PSBC upstream esclareceu um ponto importante: o PSBC calcula explicitamente `chunkusagebaseoffsetdwords` como a distância de `OrbShdr` até o **início da tabela de input usage slots**. Isso coincide com o endereço que o verificador NR encontra nos bytes emitidos. Portanto, a interpretação OpenGNM/PSBC está consistente com o formato que o PSBC pinned efetivamente produz.

O fpPS4, por sua vez, calcula esse endereço e subtrai novamente `numInputUsageSlots * sizeof(TInputUsageSlot)`. Para o fixture de um slot isso recua quatro bytes antes da tabela observada. Isso torna o cálculo fpPS4 um candidato a bug ou a uma convenção/layout diferente, mas não prova sozinho qual implementação corresponde ao ABI de todos os shaders/firmwares.

## Evidência primária

1. **OpenGNM**, commit pinned pelo workflow NR: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`.
   - `docs/reference/shaderbinary.md`, `GnmShaderBinaryInfo`: comentário de `chunkusagebaseoffsetdwords`: “starts at ((uint32_t*)&ShaderBinaryInfo) - chunkusagebaseoffsetdwords”.
   - `docs/reference/shader.md`: `GnmInputUsageSlot` tem 4 bytes; `numinputusageslots` é a contagem de slots.
   - `src/shader.c` consome `ci->inputusages[i]` a partir de uma lista já fornecida; não resolve diretamente o offset dentro do blob `OrbShdr`.

2. **PSBC**, commit pinned pelo workflow NR: `28697f007862427ebec54388ec972ce6e3545912`, arquivo `libpsbc/psbc_compile.c`.
   - Linhas 793–809 calculam `inputslots_offset = sizeof(PsslBinaryHeader) + sizeof(GnmShaderFileHeader) + headershsize(...)`.
   - O comentário declara que o campo é o offset de `OrbShdr` de volta à tabela de slots.
   - `chunkusageoffset = (orbshdr_offset - inputslots_offset) / 4`, com zero quando não há slots.
   - Linhas 825–837 gravam esse valor em `GnmShaderBinaryInfo.chunkusagebaseoffsetdwords`.
   - É evidência direta da intenção/implementação do produtor pinned; não é validação de um SDK Sony nem de runtime PS4.

3. **fpPS4**, repositório `red-prig/fpPS4`, branch `trunk`, commit observado `04cefd43e6fddd1ab033e7980cd356d14c964905`.
   - `chip/ps4_shader.pas`, `TShaderBinaryInfo`, define os dois campos.
   - `_calc_shader_slot` (linhas 472–483): calcula `usageMasks := Pointer(info) - chunkUsageBaseOffsetInDW * 4`; depois retorna `PInputUsageSlot(usageMasks) - numInputUsageSlots`.
   - `TInputUsageSlot` é packed e tem 4 bytes.
   - Este commit foi consultado diretamente, mas não é dependência pinned pelo workflow NR; preservar como referência upstream observada.

4. **Artefatos NR**:
   - Seis shaders emitidos pelo PSBC têm `numInputUsageSlots = 1`.
   - O verificador byte-level confirma que `OrbShdr - chunkOffset*4` aponta exatamente para o slot table start derivado do cabeçalho PS4/NEO e que os quatro bytes correspondem ao slot declarado no relatório.
   - O verificador confirma os bytes e o contrato do produtor; não demonstra aceitação pelo driver/firmware do PS4.

## Decisão técnica

**Não alterar o offset nem o parser nesta rodada.** A evidência atual favorece a interpretação OpenGNM/PSBC para os containers gerados pelo PSBC pinned. O cálculo fpPS4 literal recua pelo número de slots e não aponta para a tabela observada nesses artefatos. Ainda falta uma fonte normativa independente (SDK/ABI) ou uma amostra oficial validada em runtime que permita classificar definitivamente a diferença como bug fpPS4 ou diferença de formato/contexto.

A validação byte-level continua **PASS sob o contrato de emissão OpenGNM/PSBC**. A semântica de runtime continua **UNRESOLVED**; nenhum dispatch/readback no PS4 foi executado. Não promover o resultado para validação de hardware.

## CI confirmada

- Host Tests `44869804248`: success; job `136741553417`.
- Host Tests `44869807636`: success; job `136741563581`.
- Host Tests `44870307627`: success; job `136743115128`; validação de aritmética GNM, Configure, Build e Test concluídos.
- GCN8 build `44869051626`: success; jobs `136739746528` e `136740942984`.
- Todos os resultados acima são host/CI, não execução no PS4.

## Próximas evidências necessárias

1. Obter referência independente/SDK documentado ou amostra oficial de shader com metadados verificáveis.
2. Inspecionar shaders oficiais com zero, um e múltiplos slots e comparar o posicionamento de tabela e metadados.
3. Se houver prova de que o ABI exige outra interpretação, ajustar produtor/parser juntos e adicionar fixtures independentes.
4. Manter dispatch/readback PS4 bloqueado até contrato de memória/V#, plano reversível aprovado e autorização explícita de execução.
