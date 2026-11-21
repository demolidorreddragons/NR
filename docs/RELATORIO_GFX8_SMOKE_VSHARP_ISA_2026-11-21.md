# GFX8 smoke shader — inspeção ISA do contrato V#

Data: 2026-11-21  
Artifact: `nr-gcn8-compute-shaders-gfx8` (ID `14691306027`)  
Workflow: https://github.com/demolidorreddragons/NR/actions/runs/44863812886  
SHA-256 do ZIP baixado: `4fa78cf2dbdc0a5e74bb7d4a7bc2d755a8829ea2403e5e62d6b38e0cd1a40379`  
Base do build: commit `2cae50d15c3a4532a330e2e75dc0e1fde21d1759`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**.

## Shader efetivamente inspecionado

Arquivo: `compute_smoke_add.isa`  
Tamanho: 365 bytes.

ISA disassemblada:

```asm
s_mov_b32 s0, s3
s_mov_b32 s3, 0
s_load_dwordx4 s[4:7], s[2:3], 0x0
s_load_dwordx4 s[8:11], s[2:3], 0x40
s_lshl_b32 s0, s0, 6
v_add_u32_e32 v0, vcc, s0, v0
v_lshlrev_b32_e32 v0, 2, v0
s_waitcnt lgkmcnt(0)
buffer_load_dword v1, v0, s[4:7], 0 offen
s_waitcnt vmcnt(0)
v_add_f32_e32 v1, 1.0, v1
buffer_store_dword v1, v0, s[8:11], 0 offen
s_endpgm
```

## Constatações

1. O shader faz dois loads escalares de quatro dwords (16 bytes) da resource table:
   - descritor de entrada no byte offset `0x00`;
   - descritor de saída no byte offset `0x40` (64).
2. Em seguida usa o primeiro descritor em `buffer_load_dword` e o segundo em `buffer_store_dword`.
3. Isso confirma, para **este shader smoke exato**, que a tabela precisa conter dois V# descriptors de quatro dwords nos offsets 0 e 64 e que o segundo descritor precisa permitir store.
4. O espaçamento de 64 bytes usado pelo harness corresponde aos offsets observados na ISA. O tamanho de 16 bytes de cada descriptor corresponde ao load de quatro dwords.
5. A ISA não codifica por si só os valores corretos dos campos de endereço, formato, stride, records, mtype ou bypass. Portanto, não prova que o descritor manual atual seja válido para firmware.
6. O shader põe o dword alto do endereço da tabela em zero antes do `s_load_dwordx4`; isso é consistente com o contrato atual de ponteiro de tabela abaixo de 4 GiB. Não demonstra que o endereço derivado do ponteiro CPU seja realmente um GPU VA.

## Efeito na investigação

- **Confirmado:** tamanho/offset dos dois descritores no shader smoke: PASS estrutural.
- **Confirmado:** há operação de escrita `buffer_store_dword` no descriptor de saída.
- **Não confirmado:** se `GNM_MEMORY_UNCACHED` e os bits L1/L2 escolhidos estão corretos para o store.
- **Não confirmado:** se a base CPU mapeada é GPU VA utilizável.
- **NOT_RUN:** qualquer execução no PS4 Pro.

Próximo passo seguro: adicionar teste host para a codificação binária do descritor exatamente como produzido pelo harness e revisão independente GCN8/GNM. Não mudar a política de cache nem executar o PKG por tentativa.
