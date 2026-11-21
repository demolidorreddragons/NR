

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
