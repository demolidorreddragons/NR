# Auditoria de participação externa e dependência estática GNM

Data: 2026-11-22  
Repositório: `demolidorreddragons/NR`  
Branch: `feature/gcn8-numeric-kernels`  
Classificação: **REVISÃO INTERNA — NÃO INDEPENDENTE**  
Escopo: evidências públicas de colaboração/revisão externa e correção da cadeia de link do compute smoke.  
Limite: esta auditoria não prova ausência absoluta de acesso externo ao repositório; registra somente os eventos e metadados consultados.

## 1. Participação externa — evidência verificada

Foram consultados os PRs de delegação e as submissões de revisão:

| PR | Escopo | Autor exibido pelo GitHub | Revisões formais consultadas | Estado observado |
|---|---|---|---|---|
| #9 | Reconstrução do grafo DLSS-NR 310.8.3 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara handoff, não implementação |
| #10 | Runtime compute GNM para PS4 Pro GFX8 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara handoff, não backend funcional |
| #11 | QA clean-room independente dos shaders GCN8 | `demolidorreddragons` | Nenhuma | Aberto; descrição declara apenas contrato de tarefa |

Links:
- https://github.com/demolidorreddragons/NR/pull/9
- https://github.com/demolidorreddragons/NR/pull/10
- https://github.com/demolidorreddragons/NR/pull/11

As consultas de reviews e review threads para os três PRs não retornaram revisões formais. Os corpos dos PRs #9–#11 explicitam que são tarefas/handoffs e não alegam implementação ou aprovação.

**Conclusão delimitada:** não há evidência verificável, nos PRs e revisões examinados, de entrega técnica ou revisão independente por colaborador externo. Isso não permite afirmar que ninguém externo teve acesso ou realizou qualquer atividade fora desses registros. Nenhum nome de especialista externo será creditado sem commit/review/relatório identificável.

## 2. Achado de engenharia: dependência GNM ainda ligada estaticamente

Na inspeção do commit anterior, o modo compute usava quatro exports GNM via `sceKernelDlsym`, mas o comando de link ainda continha `-lSceGnmDriver`. Isso preservava uma dependência estática `DT_NEEDED` desnecessária, incompatível com o objetivo de isolar o carregamento/resolução dinâmica e evitar a classe de falha observada no GNM-Link-Only.

O código-fonte não mostrava chamadas diretas às quatro funções de runtime, mas a presença de uma biblioteca na linha de link não deve ser tratada como inofensiva sem inspeção do ELF final.

### Correção aplicada

Commit: [83d15edf1c3411752f75fdc98172f9e2f8094865](https://github.com/demolidorreddragons/NR/commit/83d15edf1c3411752f75fdc98172f9e2f8094865)

Arquivo: `tools/build_compute_smoke_pkg.sh`

1. Removido `-lSceGnmDriver` somente do link do modo `compute`. Os modos diagnósticos que testam explicitamente link estático permanecem separados.
2. Adicionada auditoria do ELF antes de gerar o OELF/PKG:
   - falha se `DT_NEEDED` mencionar `libSceGnmDriver`;
   - falha se a tabela de símbolos mostrar símbolo `sceGnm*` indefinido;
   - emite `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only` somente quando ambos os testes passam.
3. A resolução das quatro funções continua dinâmica e atômica pelo resolver existente.


### Verificação adicional do setter de memória

Foi inspecionado diretamente `PS4-OpenGNM/opengnm/include/gnm_buffer.h` no commit pinado `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, linhas 90–98. `sceGnmBufSetMemoryType` é `static inline` e apenas escreve os campos `mtype`, `mtype_l1s` e `mtype_l2`; não chama uma exportação GNM externa. Portanto, a chamada existente no harness não justifica manter `-lSceGnmDriver` no link. Os setters de base address/formato do mesmo header podem chamar `sceGnmWriteMsg` em caminhos de erro, mas o harness constrói esses campos manualmente e não chama esses setters. Esta leitura de fonte reduz a hipótese de dependência implícita, mas o ELF final ainda precisa passar pela auditoria automatizada.

### Critério de aceitação da correção

- Build compute do PS4 conclui sem depender de `-lSceGnmDriver`.
- Auditoria do ELF emite PASS e não encontra `DT_NEEDED libSceGnmDriver` nem símbolo GNM indefinido.
- PKG continua contendo o shader e passa validação/extração independente.
- Host tests e demais jobs obrigatórios passam no commit corrigido.
- O artefato e seus hashes são correlacionados ao commit exato.

**Estado nesta data:** correção versionada; validação CI do novo commit ainda não confirmada nesta auditoria. Não inferir PASS até inspecionar os jobs e o log contendo o marcador da auditoria.

## 3. Bloqueios que permanecem

1. **GPU VA / V# writable:** sem contrato autoritativo demonstrando que o ponteiro mapeado usado pelo harness é o GPU VA válido e representável para a tabela de recursos.
2. **InputUsage:** sem prova do parser Sony no firmware-alvo para a semântica do offset em compute GFX8; `runtime_semantics=UNRESOLVED`.
3. **Coerência e sincronização:** política de cache/flush/invalidate e semântica fence ainda não validadas em hardware.
4. **Revisão externa:** não há revisão independente registrada para este escopo.
5. **Hardware:** dispatch/readback no PS4 permanece `NOT_RUN`.

Nenhuma destas lacunas é compensada por build verde, compilação de shader, validação estrutural ou teste host.

## 4. Segurança e operações no console

Nenhum PKG foi staged, instalado ou executado nesta rodada. Nenhum PRX foi ativado; nenhuma configuração, save, log ou dado do PS4 foi alterado. O compute smoke permanece bloqueado para instalação/execução até que os gates técnicos e de revisão sejam satisfeitos e exista autorização específica.

## 5. Próximas ações

1. Inspecionar o resultado CI do commit `83d15edf1c3411752f75fdc98172f9e2f8094865`, incluindo o marcador de auditoria e ELF/OELF/PKG produzidos.
2. Se o build/link falhar por símbolo que dependia da biblioteca removida, identificar o símbolo concreto e sua origem; não reintroduzir a dependência inteira sem análise.
3. Obter evidência primária para GPU VA/V# e InputUsage ou manter os respectivos gates bloqueados.
4. Solicitar revisão externa real do commit e do pacote neutro de critérios, registrando identidade, escopo, conflitos, achados e decisão somente quando houver submissão verificável.

**Decisão:** `IN_PROGRESS`. A correção de dependência estática foi aplicada, mas não está aprovada até CI/artefato confirmarem o critério. Revisão externa e validação de hardware continuam `NOT_RUN`/ `BLOCKED`.
