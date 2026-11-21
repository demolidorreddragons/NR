

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
