# GCN8 — auditoria de descritores, submit, fence e readback

Data: 2026-11-21  
Branch: `feature/gcn8-numeric-kernels`  
Base inspecionada: HEAD da branch após `0a5151f4b3127af368ff8fcfa4f3d3408b0b3b9c`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**  
Estado geral: **BLOCKED para execução em hardware**.

## 1. Descritor V# e tabela indireta

### Confirmado estaticamente
- `GnmBuffer` é usado como descritor de 16 bytes.
- A ISA smoke carrega 4 dwords em offsets 0 e 0x40 e executa `buffer_load_dword` e `buffer_store_dword`.
- O harness zera 80 bytes da tabela e copia descritores nos offsets 0 e 64.
- O contrato host usa stride de binding 64 e calcula 80 bytes para os bindings 0 e 1.
- O ponteiro indireto é restringido a endereço não nulo, alinhado a 4 bytes, e inteiramente abaixo de 4 GiB; o pacote escreve um único SGPR, consistente com a ISA smoke observada que mantém o SGPR adjacente como parâmetro escalar.

### Não confirmado
- Codificação semântica correta de `mtype`, L1/L2, formato e flags para a saída gravável.
- Validade GPU do endereço derivado de `memory.mapped`.
- Compatibilidade exata da tabela de recursos com todos os shaders futuros. O resultado atual é específico do shader smoke e do ABI PSBC usado.

### Nota
O teste host atual de resource table valida offsets, tamanho e range do ponteiro indireto; não serializa nem valida o conteúdo binário dos 16 bytes de um V# real. Portanto, não deve ser descrito como teste completo do descritor.

## 2. Submit e fence

### Sequência observada no harness
1. Constrói DCB dentro da alocação direct memory.
2. Emite estado compute, binding do shader, ponteiro da tabela, dispatch e EOP fence.
3. Chama `sceGnmFlushGarlic()`.
4. Submete um DCB por `sceGnmSubmitCommandBuffers`.
5. Verifica retorno de submit.
6. Chama `sceGnmSubmitDone()`.
7. Faz polling do valor de fence, com até 10.000 iterações de 1 ms.
8. Só após fence confirmado compara 64 floats.
9. Em falha de submit-done ou timeout, mantém a alocação viva até o fim do processo.

### Pontos positivos
- Retornos de submit, submit-done, writers de shader/dispatch/fence são verificados.
- O fence tem endereço alinhado a 4 bytes e abaixo de 2^48.
- A memória que contém DCB, shader, tabela, buffers e fence é preservada nas duas saídas de timeout/submit-done.
- O readback não é feito antes da confirmação do fence.

### Riscos ainda abertos
- A semântica e o contrato de sincronização de `sceGnmSubmitDone()` para o ambiente/firmware alvo ainda precisam ser confrontados com documentação/API de referência.
- `sceGnmFlushGarlic()` é chamado antes da submissão e depois do fence, mas esta revisão não estabelece documentalmente se cobre todos os domínios/cache e direções CPU→GPU e GPU→CPU necessários para esses buffers.
- A operação EOP usa `GNM_CACHE_FLUSH_AND_INV_TS_EVENT` e escreve um valor de 32 bits; a codificação estrutural do pacote tem teste host, mas não há confirmação de conclusão/visibilidade em hardware.
- O timeout de aproximadamente 10 segundos é um limite de espera, não uma prova de que a GPU parou de acessar memória. A estratégia de manter a alocação até o fim do processo é conservadora, mas o encerramento do processo e o comportamento do driver precisam ser validados em procedimento controlado.
- A comparação CPU direta após flush/fence não tem teste de coerência real no console.

## 3. Resultado por gate

| Gate | Resultado |
|---|---|
| Geometria V# / resource table smoke | PASS estrutural |
| Conteúdo binário completo do V# | NOT_TESTED |
| Endereço CPU mapped = GPU VA | BLOCKED |
| Política de cache e escrita | BLOCKED |
| Codificação host dos pacotes PM4 | PASS nos testes existentes |
| Semântica runtime submit-done | BLOCKED |
| Visibilidade do EOP fence | NOT_RUN |
| Coerência/readback CPU-GPU | NOT_RUN |
| Execução PS4 Pro | NOT_RUN |

## 4. Próximas correções de baixo risco

1. Criar um contrato host puro para empacotar os 4 dwords V# explicitamente com máscaras/shifts e validar os bytes resultantes, sem depender de bitfields C++ nem de chamadas GNM.
2. Comparar o descritor resultante com a decodificação dos campos documentados e com a ISA GFX8 do shader smoke.
3. Criar teste host para validar todos os endereços/offsets da arena, incluindo overflow, alinhamento, sobreposição e limites de cada região.
4. Documentar com referência primária a semântica de flush, submit-done, EOP e visibilidade de memória para o firmware alvo.
5. Obter revisão independente antes de propor qualquer teste de hardware.

Não alterei o código de runtime, não disparei dispatch, não instalei PKG, não carreguei PRX e não modifiquei o console. A autorização para prosseguir foi aplicada à investigação e às alterações seguras no repositório; ela não elimina os gates de segurança para runtime.
