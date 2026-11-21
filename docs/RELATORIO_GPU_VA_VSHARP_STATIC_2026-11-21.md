

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
