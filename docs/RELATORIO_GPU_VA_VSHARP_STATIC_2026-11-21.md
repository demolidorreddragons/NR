# GCN8 — investigação estática de GPU VA e descritores V#

Data: 2026-11-21  
Branch: `feature/gcn8-numeric-kernels`  
Base: `56203ef123a88843106982915c3925947d858c14`  
Referência OpenGNM: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**  
Estado: **BLOCKED para hardware**; análise estática complementar.

## Escopo e segurança

Inspeção somente de código-fonte e documentação versionada. Nenhum PKG foi instalado/executado; nenhum PRX foi carregado; nenhum arquivo de configuração do PS4 foi alterado; nenhum dispatch foi submetido. Esta investigação não autoriza teste no console.

## 1. GPU VA — resultado inconclusivo, bloqueio mantido

O harness atual deriva `base` de `reinterpret_cast<uintptr_t>(memory.mapped)` e soma offsets para tabela, shader, entrada, saída e fence. Em seguida, esses valores são usados nos pacotes PM4 e nos descritores V#.

No OpenGNM pinado:
- `sceGnmDirectMemoryAllocate` chama `sceKernelAllocateDirectMemory` e depois `sceKernelMapDirectMemory`.
- O objeto `GnmDirectMemory` mantém separadamente `directmemory` (offset de direct memory) e `mapped` (ponteiro CPU mapeado).
- `sceGnmBufSetBaseAddress` grava o ponteiro recebido nos campos `baseaddress` e `baseaddresshi`; a documentação descreve o campo como GPU virtual base address.

O material público consultado confirma o fluxo allocate/map e a representação do endereço nos descritores. **Não foi encontrada, nessa revisão, uma garantia explícita de que o ponteiro CPU retornado por `sceKernelMapDirectMemory` é, sem transformação, o GPU VA consumível pelo shader/PM4 em todos os firmwares alvo.** O helper OpenGNM não implementa uma conversão separada, mas isso, isoladamente, também não é prova normativa do firmware.

Consequência: não substituir o endereço atual por uma fórmula especulativa nem remover as guardas. A hipótese permanece aberta até obter evidência de firmware/API primária ou uma validação controlada e reversível no hardware.

## 2. V# — layout conhecido; semântica RW/cache ainda não provada

O header OpenGNM define `GnmBuffer` como descritor de 16 bytes:
- endereço baixo: 32 bits;
- endereço alto: 12 bits;
- stride: 14 bits;
- número de records: 32 bits;
- formato e seleção de canais;
- campos de memory type/cache nos registradores 1 e 3.

O setter `sceGnmBufSetMemoryType` distribui `GnmMemoryType` pelos campos `mtype`, `mtype_l1s` e `mtype_l2`, com opções separadas de bypass. A enumeração documentada lista `READONLY`, `PRIVATE`, `CACHE_UNCOHERENT`, `CACHE_COHERENT` e `UNCACHED`; ela descreve atributos de acesso/cache, não uma prova de que o descritor criado pelo harness corresponde ao contrato de storage-buffer write do shader compilado.

O harness:
- cria input e output manualmente como `GnmBuffer`;
- configura ambos com stride de `sizeof(float)`, 64 records e formato R32_FLOAT;
- usa `GNM_MEMORY_READONLY` na entrada;
- usa `GNM_MEMORY_UNCACHED` na saída;
- copia os descritores para a resource table em offsets 0 e 64.

A inspeção confirma coerência estrutural do tamanho e dos campos, mas não confirma:
1. se os bits de formato/stride/records correspondem exatamente ao lowering GFX8 gerado para os loads/stores;
2. se `GNM_MEMORY_UNCACHED` é a política correta para o buffer de saída neste tipo de alocação;
3. se os flags L1/L2 escolhidos são corretos;
4. se a resource table e a tabela indireta estão no endereço e alinhamento exigidos pelo shader em runtime.

**Não alterar o descritor RW por tentativa.** Próxima prova necessária: confrontar a ISA GFX8 exata do kernel com a codificação dos campos V# e obter revisão GCN8/GNM independente. A confirmação final de escrita e readback exige teste hardware controlado.

## 3. Conclusão técnica

| Área | Resultado estático | Estado |
|---|---|---|
| Direct memory allocate/map | Sequência corroborada no OpenGNM | Parcial |
| CPU mapped pointer = GPU VA | Sem garantia primária suficiente | BLOCKED |
| GnmBuffer layout 16 bytes | Corroborado pelo header | PASS estrutural |
| Endereço V# 44-bit | Campo disponível no layout | PASS estrutural |
| V# RW para shader SSBO | Não demonstrado | BLOCKED |
| Memory/cache policy | Enum e setter conhecidos; política não provada | BLOCKED |
| GPU dispatch/readback | Nenhuma execução | NOT_RUN |

## 4. Ações seguintes

1. Extrair e versionar a ISA GFX8 do kernel exato usado pelo smoke; identificar instruções e campos de buffer usados nos loads/stores.
2. Adicionar testes host que decodifiquem cada palavra dos descritores gerados e comparem com valores esperados; isso valida codificação, não runtime.
3. Revisão independente por especialista GCN8/GNM sem participação na implementação.
4. Obter evidência de GPU VA específica do firmware alvo; não inferir de ponteiro CPU ou apenas de headers.
5. Somente após aprovação dos gates e procedimento reversível aprovado, realizar teste controlado no PS4. O usuário autorizou esta investigação estática; não houve autorização para instalar ou executar o smoke PKG.

## Fontes fixadas

- OpenGNM `src/helpers.c`: allocate/map e armazenamento separado de `directmemory` e `mapped`.
- OpenGNM `include/gnm_buffer.h`: layout e setters V#.
- OpenGNM `docs/reference/buffer.md`: descrição dos campos e API V#.
- OpenGNM `docs/reference/types.md`: enumeração `GnmMemoryType`.
- OpenGNM `src/drawcommandbuffer.c`: emissão de V# user data.
- OpenGNM commit: https://github.com/PS4-OpenGNM/opengnm/tree/4b295ca54c82c83acf308d1c646a2dfa9ae57350

Esta nota é evidência de revisão de fonte, não validação independente, firmware, GPU ou hardware.
