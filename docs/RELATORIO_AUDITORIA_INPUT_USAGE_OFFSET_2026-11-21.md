# Auditoria do offset InputUsage — OpenGNM × fpPS4

Data: 2026-11-21  
Branch NR: `feature/gcn8-numeric-kernels`  
Commit-base auditado: `9ed6f93ca366c8ad7daa65e6c0af350a5a56ac53`  
Classificação: **INVESTIGAÇÃO ESTÁTICA — SEM VALIDAÇÃO DE RUNTIME/HARDWARE**.

## Resultado

A divergência de quatro dwords (16 bytes) foi localizada em duas interpretações diferentes do mesmo campo, não em uma diferença de contagem do shader:

- OpenGNM documenta `chunkusagebaseoffsetdwords` como o deslocamento, em dwords, a partir de `GnmShaderBinaryInfo` até o início dos dados de chunk usage: `OrbShdr - offset * 4`.
- fpPS4 calcula primeiro esse mesmo endereço e, em seguida, subtrai `numInputUsageSlots * sizeof(TInputUsageSlot)` para retornar o início da lista de slots.

Para o artefato smoke analisado, `numInputUsageSlots = 1`; portanto, as duas fórmulas diferem exatamente em 1 dword (4 bytes), não em quatro dwords. O valor 88 versus 84 relatado na rodada anterior deve ser interpretado como offsets absolutos distintos no layout observado, e não como uma prova de que os dois parsers contam slots de forma diferente. A fórmula fpPS4, em termos gerais, desloca pelo número de slots; para um slot, o delta aritmético é 4 bytes.

## Evidência primária

1. OpenGNM pinned pelo workflow NR: commit `4b295ca54c82c83acf308d1c646a2dfa9ae57350`.
   - `docs/reference/shaderbinary.md`, estrutura `GnmShaderBinaryInfo`: comentário do campo `chunkusagebaseoffsetdwords`: “starts at ((uint32_t*)&ShaderBinaryInfo) - chunkusagebaseoffsetdwords”.
   - `docs/reference/shader.md`: `GnmInputUsageSlot` tem 4 bytes.
   - `GnmShaderCommonData.numinputusageslots` é documentado como número de input usage slots.
   - O código de `src/shader.c` consome `ci->inputusages[i]` a partir de uma lista já fornecida; não demonstra a interpretação do ponteiro dentro do blob `OrbShdr`.

2. fpPS4 observado em `red-prig/fpPS4`, branch `trunk`, commit `04cefd43e6fddd1ab033e7980cd356d14c964905`.
   - `chip/ps4_shader.pas`, `TShaderBinaryInfo`: define `chunkUsageBaseOffsetInDW` e `numInputUsageSlots`.
   - `_calc_shader_slot` (linhas 472–483): `usageMasks := Pointer(info) - chunkUsageBaseOffsetInDW * 4`; depois `Result := PInputUsageSlot(usageMasks) - numInputUsageSlots`.
   - `TInputUsageSlot` é um registro packed de 4 bytes.
   - Esta referência fpPS4 não é pin do workflow NR; foi consultada em HEAD da branch indicada e deve ser tratada como evidência upstream não fixada.

3. Implementação NR atual:
   - `tools/verify_gfx8_report.py` calcula e reporta a hipótese OpenGNM como `OrbShdr - chunkOffset*4` e a hipótese fpPS4 como esse endereço menos `slotCount*4`.
   - `platform/ps4/gcn8/host/shader_binary_contract.cpp` exige que o endereço obtido pela fórmula OpenGNM coincida com a posição esperada da tabela de slots no container produzido pelo PSBC.
   - O fixture em `tests/test_gcn8_shader_binary.cpp` confirma apenas o contrato do layout sintético/artefato esperado pelo parser NR; não é uma prova independente da semântica do formato em todos os produtores/firmwares.

## Decisão técnica

**Não alterar o offset nem o parser nesta rodada.** A documentação OpenGNM e o código fpPS4 não são semanticamente equivalentes: um descreve o endereço-base dos chunk usage data; o outro retorna o começo da sequência de slots após descontar a quantidade. Ainda falta uma fonte normativa/terceira implementação que esclareça se o campo aponta para o começo ou para o fim da sequência no formato efetivamente emitido pelo PSBC, ou uma amostra de shader oficial validada por runtime.

A validação byte-level NR pode continuar como **PASS sob a interpretação OpenGNM usada pelo próprio pipeline**, mas `runtime_semantics` permanece **UNRESOLVED**. Não elevar isso a PASS semântico, nem usar o resultado para autorizar dispatch/readback no PS4.

## CI confirmada nesta rodada

- Host Tests `44869804248`: success; job `136741553417`; incluindo `Validate GNM input-usage offset arithmetic`.
- Host Tests `44869807636`: success; job `136741563581`; incluindo o mesmo teste de aritmética.
- GCN8 build `44869051626`: success; jobs `136739746528` e `136740942984`.
- Esses resultados são host/CI; não representam execução em hardware.

## Próxima evidência necessária

1. Fixar um commit fpPS4 em dependência de auditoria e comparar a semântica com outra implementação independente/SDK documentado.
2. Inspecionar bytes e limites de tabelas de mais de um shader, incluindo zero, um e múltiplos slots, sem inferir a posição somente do parser que está sendo testado.
3. Só então ajustar o parser/contrato e adicionar fixtures derivadas de artefatos independentes.
4. Manter dispatch/readback PS4 bloqueado até que o plano reversível seja aprovado e a execução autorizada.
