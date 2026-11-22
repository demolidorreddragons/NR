

## 5. Addendum — confronto direto entre helper, kernel mapping e V# (2026-11-21)

Foi feita uma segunda leitura do código e da documentação do mesmo commit OpenGNM `4b295ca54c82c83acf308d1c646a2dfa9ae57350` para eliminar ambiguidade entre os três domínios de endereço.

### Evidência confirmada

- `src/helpers.c` declara `sceKernelAllocateDirectMemory(..., int64_t* outoffset)` e `sceKernelMapDirectMemory(void** outaddr, ..., int64_t directmemoryoffset, ...)`. Em `sceGnmDirectMemoryAllocate`, o retorno de Allocate é guardado em variável `directmemory`; Map recebe esse offset e devolve `mapped`. A struct recebe `memory->mapped = mapped` e `memory->directmemory = (uint64_t)directmemory`. Portanto, no backend Orbis do helper, são valores distintos e não se deve tratar o offset como CPU pointer.
- `docs/reference/helpers.md` descreve `mapped` como CPU-mapped pointer e `directmemory` como GPU-side direct memory address. Isso esclarece a semântica pretendida pelo próprio projeto OpenGNM para o campo `directmemory`, mas não especifica uma transformação de offset para o GPU VA de um shader.
- `include/gnm_buffer.h` implementa `sceGnmBufSetBaseAddress(GnmBuffer*, void*)` copiando os bits do argumento para `baseaddress` e `baseaddresshi`. `docs/reference/buffer.md` chama esses campos de GPU virtual base address. A assinatura aceita `void*`, mas o nome do campo/contrato documental é GPU VA; o tipo C, por si só, não prova que qualquer ponteiro de CPU seja válido para GPU.
- O backend genérico de `src/helpers.c` atribui `directmemory = (uint64_t)(uintptr_t)mapped`. Esse ramo não deve ser usado como evidência para o comportamento Orbis: é uma implementação de host/genérica e mascara a distinção entre os domínios.

### Consequência para o harness NR

O harness usa `reinterpret_cast<uintptr_t>(memory.mapped)` como endereço-base para PM4 e V#. A leitura confirma que `mapped` é o ponteiro CPU de uma região também mapeada com proteção CPU/GPU; **não confirma** que seu valor numérico seja o GPU VA correto no firmware alvo. O campo `directmemory` é um offset/identificador de direct memory retornado pelo kernel, documentado pelo OpenGNM como GPU-side direct-memory address; não foi encontrada nesta fonte uma API de tradução que autorize simplesmente substituir `mapped` por `directmemory` nos descritores.

Assim, a hipótese central permanece **BLOCKED**. Não há base para uma correção por troca de campo, cast ou soma de base especulativa. É necessário obter contrato primário da API de mapeamento GPU/firmware (ou implementação de referência que demonstre a conversão) e revisão independente antes de qualquer execução.

### Estado após addendum

| Item | Estado |
|---|---|
| Offset retornado por ` sceKernelAllocateDirectMemory ` separado do CPU pointer de ` sceKernelMapDirectMemory ` | CONFIRMADO no código OpenGNM |
| `mapped` é CPU pointer | CONFIRMADO pela documentação OpenGNM |
| `directmemory` é campo GPU-side direct-memory address | DOCUMENTADO pelo OpenGNM; detalhes de tradução/uso não especificados |
| CPU pointer numérico equivale a GPU VA para V#/PM4 | NÃO PROVADO — BLOCKED |
| Offset `directmemory` pode ser gravado diretamente em V#/PM4 | NÃO PROVADO — NÃO FAZER |
| Execução em PS4 | NOT_RUN; sem autorização para instalação/execução |

Fontes adicionais fixadas no mesmo commit:
- `src/helpers.c`
- `include/gnm_helpers.h`
- `docs/reference/helpers.md`
- `include/gnm_buffer.h`
- `docs/reference/buffer.md`


## 6. Addendum — documentação Libkernel e precedente de uso no smoke OpenGNM (2026-11-21)

### 6.1 Contrato Libkernel publicado pelo OpenOrbis

Foi consultada a documentação `docs/MD/PS4 Libraries/Libkernel.md` do OpenOrbis Toolchain, commit `0a1aaf9dd4a92695538bdeb09fb056d06dd11725`:

- `sceKernelAllocateDirectMemory` recebe `off_t *physicalAddrDest` e documenta que grava ali o endereço físico da memória direta alocada.
- `sceKernelMapDirectMemory` recebe esse endereço físico e grava em `virtualAddrDest` o endereço virtual final do mapeamento.
- `sceKernelReleaseDirectMemory` recebe o endereço físico alocado.

Isso dá suporte mais claro à separação entre endereço físico/offset de backing e endereço virtual mapeado. Entretanto, a documentação Libkernel usa o termo **virtual address** genericamente e não declara, por si só, que esse endereço virtual seja o GPU VA efetivo do espaço de endereçamento GCN8 para qualquer firmware. A distinção continua necessária: endereço virtual CPU e GPU VA podem compartilhar valor em uma implementação, mas isso precisa de contrato GNM/firmware ou prova funcional, não de inferência pelo nome.

### 6.2 Precedente de implementação no OpenGNM

No mesmo commit OpenGNM, `tests/hardware_smoke.c`:
- aloca Garlic direct memory e obtém separadamente `mapped` e `direct_memory`;
- inicializa o command buffer em `mapped`;
- forma o endereço do EOP label a partir de `mapped`;
- passa o endereço do command buffer para `sceGnmSubmitCommandBuffers` e o endereço do label para `sceGnmDrawCmdEventWriteEop`.

É um precedente de código relevante para o uso do mapeamento virtual retornado por `MapDirectMemory` em endereços consumidos pelo caminho GNM. **Não encontrei, nesta inspeção, um resultado de execução/log com firmware e console identificados para esse teste.** Logo, isso demonstra a convenção implementada pelo OpenGNM, mas não é uma prova empírica reproduzível de que o mesmo contrato vale para o firmware 14.02 do alvo NR.

### 6.3 Ajuste da conclusão

A evidência agora permite dizer:
1. `AllocateDirectMemory` produz um endereço físico de backing (termo da documentação OpenOrbis); `MapDirectMemory` produz um endereço virtual de mapeamento.
2. OpenGNM usa o endereço `mapped` como endereço de command buffer e EOP em seu smoke de hardware, consistente com a hipótese atual do harness NR.
3. Ainda falta evidência específica e autoritativa de que o mapeamento virtual retornado seja o endereço válido para descritores V# e PM4 compute no firmware 14.02/GoldHEN alvo, e falta resultado de execução do smoke em hardware com logs identificáveis.
4. Não se deve usar `directmemory` como substituto numérico do `mapped` para V#/PM4: a documentação o descreve como endereço físico de backing, e o código OpenGNM passa o virtual mapping nos caminhos GNM observados.

**Decisão:** manter a implementação NR sem alteração especulativa. O uso de `mapped` ganha suporte de precedente de implementação, mas o gate de GPU VA específico para V# compute continua **BLOCKED / NOT PROVEN** até evidência de firmware/hardware e revisão independente.

### Fontes fixadas

- OpenOrbis Toolchain `docs/MD/PS4 Libraries/Libkernel.md`, commit `0a1aaf9dd4a92695538bdeb09fb056d06dd11725`.
- OpenGNM `tests/hardware_smoke.c`, commit `4b295ca54c82c83acf308d1c646a2dfa9ae57350`.
- OpenGNM `src/driver_orbis.c`: assinaturas de `sceGnmMapComputeQueue` e parâmetros de ring/read pointer; não oferecem conversão explícita de direct-memory physical address para GPU VA.

Esta análise permanece estática e interna. Não foi executado teste no PS4.



## 7. Confronto adicional com o hardware smoke atual do OpenGNM (2026-11-21)

Foi inspecionada a implementação pública atual de `PS4-OpenGNM/opengnm/tests/hardware_smoke.c` e `src/helpers.c`. Esta leitura é complementar: o código consultado na branch pública atual não foi fixado ao commit usado pelo workflow NR e não deve ser tratado como ABI Sony nem como resultado reproduzido pelo NR.

### Evidência de implementação

- O helper Orbis do OpenGNM chama `sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), size, alignment, memorytype, &directmemory)`, depois `sceKernelMapDirectMemory(&mapped, size, protection, 0, directmemory, alignment)`; em caso de falha do map, libera o offset físico. Mantém `mapped` e `directmemory` em campos distintos.
- O hardware smoke upstream aloca um bloco Garlic de 2 MiB com alinhamento de 2 MiB, usando `ORBIS_KERNEL_WC_GARLIC`; pede proteção CPU read/RW e GPU read/write.
- O smoke usa o ponteiro `mapped` para o command buffer e para um label EOP de 64 bits localizado no fim do bloco.
- O evento EOP usa `GNM_CACHE_FLUSH_AND_INV_TS_EVENT`, `GNM_DATA_SEL_SEND_DATA64` e espera o valor escrito no label. O teste é um draw de contagem zero, não compute; a fonte inspecionada não fornece log de execução com identificação de firmware/console nesta consulta.

### Comparação com o harness NR

| Item | NR atual | Precedente OpenGNM consultado | Conclusão |
|---|---|---|---|
| Alocação | Garlic WC, 1 MiB, 64 KiB | Garlic WC, 2 MiB, 2 MiB | Ambos são candidatos de alocação; alinhamento/pool do smoke de display não aprova SSBO compute |
| Proteção | `GNM_PROT_CPU_GPU_RW` | CPU read/RW + GPU read/write | Compatível em intenção, mas confirmar valores/semântica dos macros pinned |
| DCB | endereço derivado de `mapped` | `mapped` | Precedente de implementação reforça o uso para DCB |
| EOP label | 32 bits no mesmo bloco Garlic | 64 bits no mesmo bloco Garlic | Precedente para Garlic/EOP, mas tamanho/data selection diferem |
| Evento | Flush+invalidate TS event | Flush+invalidate TS event | Mesma classe de evento |
| V#/SSBO compute | V# writable manual | Não exercitado | Continua sem evidência |
| GPU VA abaixo de 4 GiB | pressupõe valor numérico de `mapped` | não exige/relata esse limite | Continua sem prova para o ponteiro de tabela 32-bit PSBC |

### Achados e decisão

1. A separação `mapped` versus `directmemory` permanece correta; não substituir o primeiro pelo segundo.
2. O uso de `mapped` em DCB/EOP ganha mais um precedente de código upstream. Isso ainda não demonstra que o valor numérico de `mapped` seja a GPU VA correta para os V# do shader NR, nem que possa satisfazer o requisito de tabela abaixo de 4 GiB.
3. O EOP NR usa o mesmo tipo de evento de flush/invalidate, porém seu label é de 32 bits e sua região é Garlic. A referência upstream usa 64 bits; não mudar a largura sem alinhar o packet e o contrato do harness.
4. O hardware smoke upstream não executa compute, não cria V# writable e não valida cache policy de SSBO. Não resolve os bloqueios principais.
5. O harness NR não deve ser executado no PS4 neste estado. A próxima correção segura é obter um contrato específico para GPU VA/low-4-GiB e V# writable, além de validar a sincronização e a política de cache do output. A evidência atual não autoriza trocar macros, endereços ou descritores por tentativa.

**Classificação:** STATIC_REVIEW adicional. GPU VA para V# / tabela 32-bit = BLOCKED; V# writable/cache = BLOCKED; compute dispatch/readback = NOT_RUN.

## 8. Rechecagem documental independente do offset de InputUsage (2026-11-21)

Foi feita nova consulta às referências públicas atuais, separando explicitamente documentação de implementação:

- A referência pública atual do OpenGNM para `GnmShaderBinaryInfo` declara que `chunkusagebaseoffsetdwords` aponta para o início da tabela: `((uint32_t*)&ShaderBinaryInfo) - chunkusagebaseoffsetdwords`. A página também mantém `numinputusageslots` como campo separado.
- O código atual do fpPS4 executa duas operações: primeiro recua `chunkUsageBaseOffsetInDW * 4` bytes a partir de `OrbShdr`, depois recua mais `numInputUsageSlots` elementos de `TInputUsageSlot`. Isso é uma interpretação de fim-da-tabela e contradiz literalmente o comentário da documentação OpenGNM.
- A documentação OpenGNM de shaders descreve a tabela de InputUsage como dados colocados imediatamente após a estrutura de estágio, antes do código. Ela não demonstra, por si só, qual das duas convenções o parser Sony aceita em todos os binários.
- Os exemplos públicos freegnm avisam que os shaders compilados por psbc não carregam reflection e que seus exemplos não foram testados em PS4 Pro/NEO; portanto não são uma amostra independente apta a resolver o contrato GFX8/NEO.

**Conclusão desta rodada:** a divergência não foi resolvida. A documentação OpenGNM favorece a interpretação de início da tabela; fpPS4 implementa a interpretação de fim da tabela. Nenhuma das duas fontes, isoladamente, prova o comportamento do parser Sony no firmware 14.02. Não mudar o cálculo do PSBC nem relaxar o bloqueio com base apenas nesta documentação. Manter os shaders com offset não representável como rejeitados; exigir amostra real de shader PS4 com tabela conhecida, parser independente adicional ou validação controlada no hardware antes de decidir o layout.

Referências consultadas:
- OpenGNM Shader Binary: https://ps4-opengnm.github.io/opengnm/reference/shaderbinary/
- OpenGNM Shader Guide: https://ps4-opengnm.github.io/opengnm/guides/shaders/
- fpPS4 resolver: https://github.com/red-prig/fpPS4/blob/trunk/chip/ps4_shader.pas
- freegnm examples caveats: https://github.com/lateleite/freegnm-examples

**Limite:** esta atualização é auditoria documental. Não houve compilação GFX8, dispatch, readback, execução no PS4, nem validação do parser Sony.

## 9. Nova trilha de evidência: RE do parser no firmware FW 9.00 (2026-11-21)

A revisão do plano público de reescrita do OpenGNM revelou uma trilha mais forte que os exemplos de parser emulador: o projeto declara ter feito engenharia reversa de libSceGnmDriver.sprx do firmware 9.00 e marca o item RE-6 (shader binary parser) como concluído. O plano identifica como fonte tools/gnm_driver_fw900_analysis.md e afirma que essa análise cobre offsets de metadados, algoritmo CRC32 e layout da resource table. Também declara que a autoridade do comportamento do driver vem do firmware disassemblado, com shadPS4 como referência cruzada.

**O que foi possível verificar publicamente nesta rodada:**
- O plano público documenta explicitamente a existência e o escopo da RE do parser e identifica FW 9.00, incluindo o módulo NEO/Pro libSceGnmDriverForNeoMode.sprx.
- A referência pública do formato OpenGNM continua descrevendo o campo como início da tabela: OrbShdr - chunkusagebaseoffsetdwords*4.
- A página pública do PSBC diz que o hardware validation DONE corresponde a VS+PS em PS4 FW 9.00, com renderização de triângulo por 600 frames. Isso é evidência de validação de shader gráfico no FW 9.00, mas não é validação de compute GFX8/PS4 Pro nem prova de qual convenção o parser Sony aplica ao compute.
- O arquivo detalhado tools/gnm_driver_fw900_analysis.md não está acessível no repositório público consultado (resposta 404). Portanto, nesta rodada não foi possível auditar as instruções/pseudocódigo da função parser, seus offsets exatos, nem confirmar se o trecho foi analisado no caminho NEO ou apenas no driver comum.

**Impacto na hipótese InputUsage:** a documentação do OpenGNM tem uma alegação de proveniência mais forte (RE do firmware) do que fpPS4, mas o material primário necessário para conferir o detalhe do ponteiro não está publicado/acessível. A alegação de RE não será convertida em prova independente sem o trecho da análise, pseudocódigo, endereço/offset da função e descrição do fluxo que resolve a tabela. O comentário do cabeçalho, o cálculo do produtor PSBC e a validação VS/PS FW 9.00 são coerentes com o ponteiro para início, mas ainda não provam o caso CS GFX8 no FW 14.02.

**Próximo passo técnico delimitado:** solicitar/publicar a evidência RE-6 relevante (função, pseudocódigo e referências aos campos chunkusagebaseoffsetdwords/numinputusageslots) ou localizar um shader real com metadados e parser firmware observável. Até lá, preservar a hipótese de início como melhor sustentada documentalmente, manter runtime_semantics=UNRESOLVED e não alterar o parser/PSBC nem liberar o dispatch.

Referências:
- OpenGNM rewrite plan / RE-6: https://github.com/PS4-OpenGNM/opengnm/blob/main/OPENGNM_REWRITE_PLAN.md
- OpenGNM shader binary reference: https://ps4-opengnm.github.io/opengnm/reference/shaderbinary/
- PSBC plan and stated FW 9.00 VS+PS validation: https://github.com/PS4-OpenGNM/opengnm-psbc/blob/main/OPENGNM_PSBC_PLAN.md

**Classificação:** STATIC_REVIEW. Sem build novo, artefato GFX8, dispatch ou teste no PS4 nesta rodada.


## 10. Análise estrutural do limite de 8 bits de InputUsage (2026-11-21)

Foi reaberto o código-fonte pinned do PSBC (28697f007862427ebec54388ec972ce6e3545912) e os headers OpenGNM pinned (4b295ca54c82c83acf308d1c646a2dfa9ae57350) para delimitar o que realmente precisa mudar para recuperar o shader tiled.

### Fatos confirmados no código

- GnmShaderBinaryInfo.chunkusagebaseoffsetdwords é declarado como uint8_t no header OpenGNM; portanto o campo serializado representa somente 0–255 dwords (0–1020 bytes).
- O PSBC pinned serializa os InputUsage slots imediatamente após o cabeçalho específico do estágio, antes do código GCN.
- O mesmo PSBC coloca o bloco OrbShdr imediatamente depois do código GCN e calcula a distância retroativa até o início dessa tabela.
- Para o tiled GEMM, o relatório byte-level registra tabela em 0x58, OrbShdr em 0x990: distância 0x938 bytes = 590 dwords. A conversão para uint8 resulta em 78, que aponta para endereço incorreto.
- Para os quatro shaders curtos, as distâncias observadas são 215, 136, 92 e 74 dwords, todas representáveis e resolvendo para 0x58.

### Consequência de engenharia

O erro não é apenas uma falha de verificação: a disposição atual do produtor torna matematicamente impossível representar o ponteiro para a tabela inicial quando a distância excede 255 dwords. A rejeição de overflow evita emitir um container silenciosamente corrompido, mas deixa o shader tiled sem artefato válido.

A correção real exige resolver o layout do container, não alargar localmente o campo nem truncá-lo. Antes de implementar qualquer rearranjo, é necessário provar como o parser GNM determina a tabela de slots, quais regiões do container podem ser movidas/duplicadas, e como o CRC cobre a nova disposição. Os helpers OpenGNM que calculam a tabela a partir do cabeçalho de estágio também precisam ser considerados; não se deve assumir que uma cópia adicional próxima a OrbShdr será consumida pelo driver.

### Próximo experimento seguro

Construir um fixture sintético de container, sem PS4, que varie o tamanho do código ao redor do limite de 1020 bytes e compare: (1) tabela no local canônico após o cabeçalho CS; (2) qualquer layout alternativo proposto, com os mesmos slots e código; (3) resolução do offset, limites de todos os segmentos e CRC; (4) comportamento do parser host NR e dos helpers OpenGNM.

Esse experimento pode provar consistência estrutural do formato modelado, mas não provar aceitação pelo parser Sony. A evidência pública atualmente disponível não basta para escolher um layout alternativo. Manter o tiled rejeitado e o runtime bloqueado até essa análise.

**Estado:** overflow reproduzido/explicado estaticamente; mitigação por rejeição é válida; solução estrutural ainda BLOCKED; nenhum dispatch ou teste PS4 realizado.

## 11. Rechecagem de endereçamento e política V# para compute — 2026-11-22

### 11.1 Separação entre offset DirectMemory e endereço mapeado

Foi reaberto o código do OpenGNM no commit exato pinado pelo projeto (`4b295ca54c82c83acf308d1c646a2dfa9ae57350`), especialmente `src/helpers.c`, `include/gnm_helpers.h` e `include/gnm_buffer.h`.

- `sceKernelAllocateDirectMemory` retorna um offset/endereço físico de backing em `directmemory`; `sceKernelMapDirectMemory` recebe esse valor e devolve separadamente `mapped`.
- O helper Orbis conserva ambos: `memory->mapped = mapped` e `memory->directmemory = (uint64_t)directmemory`. O release usa `directmemory` para `sceKernelReleaseDirectMemory`; o mapping CPU é `mapped`.
- `sceGnmBufSetBaseAddress` grava os bits do argumento nos campos `baseaddress` e `baseaddresshi` (12 bits), e a referência OpenGNM descreve o resultado como GPU virtual base address.
- O hardware smoke do OpenGNM usa o ponteiro `mapped` para command buffer e EOP label. Isso é precedente de implementação para DCB/EOP, não demonstra por si só a semântica de V# writable compute.
- Um renderer PS4 público (Moonlight DX PS4) também usa buffers alocados a partir de `mapped` para `sceGnmCreateConstBuffer` e `sceGnmTexSetBaseAddress`; é evidência de uso em caminho gráfico/constante, não prova para SSBO gravável em compute.

**Conclusão delimitada:** não trocar `mapped` por `directmemory`; o último é o offset de backing e também é usado para release. O uso de `mapped` como endereço de recurso é coerente com a API V# e precedentes de renderização. Ainda não existe prova direta de que o valor retornado seja um GPU VA utilizável para V# compute no firmware 14.02, nem de que a alocação/mapa satisfaça a tabela de user-SGPR abaixo de 4 GiB. O gate GPU VA/low-4-GiB permanece BLOCKED.

### 11.2 V# gravável e política de cache

No header OpenGNM pinado, `GnmMemoryType` define `GNM_MEMORY_READONLY=0x10`, `GNM_MEMORY_PRIVATE=0x60`, `GNM_MEMORY_CACHE_UNCOHERENT=0x6d`, `GNM_MEMORY_CACHE_COHERENT=0x6e` e `GNM_MEMORY_UNCACHED=0x6f`. `sceGnmBufSetMemoryType` codifica o tipo nos campos `mtype`, `mtype_l1s` e `mtype_l2`; os parâmetros `l1cachebypass` e `kcachebypass` são bits separados.

O harness atual escolhe `GNM_MEMORY_READONLY` para input e `GNM_MEMORY_UNCACHED` para output, ambos em direct memory Garlic com proteção CPU/GPU RW. A proteção de mapeamento (`GNM_PROT_CPU_GPU_RW`) e o `GnmMemoryType` do descriptor são domínios distintos: a primeira controla permissões do mapping; o segundo programa atributos de acesso/cache no V#. A existência dos enums e a codificação de bits confirmam apenas que os valores são representáveis no header; não demonstram que `UNCACHED` seja a política correta/aceita para um SSBO writable GFX8/NEO, nem que ela resolva visibilidade CPU↔GPU.

O OpenGNM smoke fixado executa um draw de contagem zero, usa Garlic para DCB/EOP e não exercita V# writable compute. A documentação pública de tipos também lista os enums, mas não fornece contrato de writeback/invalidação para esse cenário.

**Decisão:** não alterar `GNM_MEMORY_UNCACHED`, os bypass bits, o pool Garlic/Onion ou a sequência de flush/fence/readback por tentativa. O output V# writable e a política de cache continuam BLOCKED. Para avançar, é necessária evidência primária de ABI/firmware ou um teste mínimo controlado e autorizado que valide separadamente: descriptor aceito, store GPU, conclusão/fence e leitura CPU coerente.

### 11.3 Próxima sequência técnica sem tocar no console

1. Preparar um teste host de codificação V# que compare os 4 dwords construídos manualmente com os campos definidos no header pinado, incluindo base low/high, stride, record count, format, channel selectors e memory-type bits. O resultado só certificará a codificação estrutural, não a semântica de hardware.
2. Manter o gate para firmware até obter uma fonte autoritativa para writable V#/cache ou revisão externa técnica identificável.
3. Só depois da evidência estática e da revisão, submeter um plano mínimo de probe reversível; qualquer deploy/execução no PS4 exigirá autorização explícita separada.

**Classificação:** STATIC_REVIEW. Sem alteração de código, sem novo build, sem staging e sem execução no PS4. GPU VA para V# compute = BLOCKED; V# writable/cache = BLOCKED; dispatch/readback = NOT_RUN.

### Fontes primárias e de implementação consultadas

- OpenGNM pinned `src/helpers.c`: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/helpers.c
- OpenGNM pinned `include/gnm_helpers.h`: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/include/gnm_helpers.h
- OpenGNM pinned `include/gnm_buffer.h`: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/include/gnm_buffer.h
- OpenGNM pinned `include/gnm_types.h`: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/include/gnm_types.h
- OpenOrbis Libkernel mapping semantics: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/docs/MD/PS4%20Libraries/Libkernel.md
- OpenGNM hardware smoke: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/tests/hardware_smoke.c
- OpenGNM public GnmMemoryType reference: https://ps4-opengnm.github.io/opengnm/reference/types/
- Example PS4 graphics renderer using mapped memory in GNM descriptors: https://git.fourletterworkshop.com/moonlight-dx-ps4/blob/v2.00/src/platform/ps4/GpuVideoRenderer.cpp
### 11.4 Codec V# compartilhado entre host e harness — 2026-11-22

Foi identificado que o repositório já possuía `host/vsharp_codec.*` e o teste `test_gcn8_vsharp_codec`; a lacuna era que o harness PS4 ainda construía o V# manualmente, sem reutilizar o código testado. A correção não altera os valores/semântica do descriptor: faz o runtime usar o mesmo packer de quatro dwords validado em host.

- Commit `d67e18f38ba1ca3e2cd136727cdb64c82c3f40bd`: `compute_smoke.cpp` passou a construir `VSharpBufferFields` e chamar `pack_vsharp_buffer`, com falha fechada e log específico para input/output descriptor inválido.
- Commit `fb639157294063853a01f430d59c69ac8f6ba4e9`: o build PS4 compute passou a compilar e linkar `host/vsharp_codec.cpp` no ELF compute. Startup-only permanece sem esse objeto.
- Commit `2fe2757455a9985177e20ab0af633ee5c2df04ef`: o teste host recebeu golden cases para os campos exatos do `GNM_FMT_R32_FLOAT` usado no smoke, tanto `GNM_MEMORY_READONLY` (input) quanto `GNM_MEMORY_UNCACHED` (output), verificando os quatro dwords completos.
- Host CI run `45015718289`, job `137187475115`: Configure/Build e CTest concluíram com **9/9 testes PASS**, incluindo `gcn8_vsharp_codec` e os novos dwords de input/output.
- A primeira tentativa PRX no run `45015711431` falhou porque o include `vsharp_codec.h` não estava no caminho de busca do Makefile; o mesmo erro apareceu no run `45015714818` (commit anterior à correção). Correção do include para `host/vsharp_codec.h`: commit `82719c1392bab7fe00fb2301220c3b80de82e7d7`. A inspeção do Makefile mostrou que também faltava compilar/linkar o objeto no PRX; regra e objeto adicionados no commit `c31d3ab2a70e8e7e548a4931323f1627ed2d7396`. O run PS4 PRX/PKG atual `45015947338` está em execução e o run GCN8 shader/PKG `45015947332` está pendente na última consulta. Link do codec no alvo PS4 permanece **PENDING CI** até inspeção dos logs desses runs.

- Host CI mais recente no commit `c31d3ab2...`: run `45015947339`, job `137188178578`; Configure/Build e CTest **9/9 PASS**, incluindo os testes de codificação de memory type.
- PS4 PRX/PKG run `45015947338`: build job `137188202767` SUCCESS; log confirma compilação de `vsharp_codec.o` e `compute_smoke.o`, link do PRX com ambos os objetos e build do PKG. Job `independent-pkg-qa` `137188551284` SUCCESS; relatório classifica QA como `PASS_PC_ONLY_NOT_HARDWARE_TESTED`. O QA cobre o app principal e os pacotes Startup/GNM-Load, não substitui a QA do PKG compute que está no workflow GCN8 separado.
- O run GCN8 shader/compute PKG `45015947332` está em execução; a auditoria final do novo ELF/OELF/PKG depende desse run.
 
Este avanço fecha a lacuna de divergência entre o packer testado em host e a montagem manual usada pelo harness. Não valida que `GNM_MEMORY_UNCACHED` seja a política correta de write/cache no firmware, nem que o endereço mapeado seja GPU VA válido, nem a execução do shader. Os gates GPU VA, V# writable/cache e dispatch/readback permanecem BLOCKED/NOT_RUN. O bloqueio compile-time de compute continua ativo; nenhum staging, instalação ou execução no console foi feito.