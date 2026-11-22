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

**Estado inicial desta seção:** correção versionada; a validação foi concluída posteriormente e está registrada na seção 2.1 abaixo.

## 2.1. Resultado da validação CI — commit corrigido

A validação foi localizada diretamente na lista de execuções do GitHub Actions, não inferida a partir de commits anteriores:

| Verificação | Run / Job | Resultado observado |
|---|---|---|
| Build de shaders GFX8 + compute smoke PKG | [45005750367](https://github.com/demolidorreddragons/NR/actions/runs/45005750367), job `137156845303` + `137157762811` | SUCCESS |
| Host tests | [45005750309](https://github.com/demolidorreddragons/NR/actions/runs/45005750309), job `137156845061` | SUCCESS; Configure, Build e Test concluídos |
| Coleta read-only do logger | [45005750309](https://github.com/demolidorreddragons/NR/actions/runs/45005750309), job `137156844849` | SUCCESS; FTP read-only e upload de evidência concluídos |

O run de build tem `head_sha=83d15edf1c3411752f75fdc98172f9e2f8094865`, branch `feature/gcn8-numeric-kernels`, evento `push`. No log do job `137157762811` foram inspecionados:
- `COMPUTE_GNM_STATIC_IMPORT_AUDIT=PASS; runtime_dlsym_only` — o script não encontrou `DT_NEEDED libSceGnmDriver` nem símbolo `sceGnm*` indefinido no ELF;
- `COMPUTE_SMOKE_PKG_PAYLOAD=PASS`;
- PKG compute: `PS4-Neural-Rendering-GCN8-compute-45005750367.pkg`, SHA-256 `97b866dfa5cc5bbb655417a4d6ec10700dd22a50055d6e22f06813dde793e5fb`;
- PKG startup-only separado: `PS4-Neural-Rendering-GCN8-startup-45005750367.pkg`, SHA-256 `e6e1553399f91bf96d03790a5efc6677856d0f8819279f8d1c2144c2da16c1a1`.

O workflow também executou validação e extração do PKG pelo PkgTool, verificando o payload compute e o shader esperado. O artefato ZIP `nr-gcn8-compute-smoke-ps4-pkg` é o ID `14747203138`, 25.268.050 bytes, digest SHA-256 `630e1a9a1f314837796fae560953733b808116aa9fb1b173cf76119d950c5612`; o ZIP foi baixado e seu SHA-256 recalculado, com correspondência. O PKG compute dentro dele tem 6.619.136 bytes e SHA-256 `97b866dfa5cc5bbb655417a4d6ec10700dd22a50055d6e22f06813dde793e5fb`, recalculado diretamente após extração do ZIP. O artefato de shaders GFX8 é o ID `14747912741`, 26.065 bytes, digest SHA-256 `047e83a8cecaaf4799f7ef14ea969ae2c9dbe20d6c6358890bede230429e1717`.

**Lacuna de rastreabilidade do binário:** o ZIP de PKG contém os PKGs compute/startup, manifests, GP4 e SFO, mas não contém `smoke.elf`, `smoke.oelf` nem `smoke.map`. O manifest registra apenas seus hashes (`ELF ffc40a370ada4d920a5034cf1bad3f1695b60ad630111d0c9d7b4bab31bdb309`; `OELF c84dd06e796b0e77371ee2c427ad836f926110879b4a3cc5e35e83ecd55fc721`; `MAP ba46e6aac1088a0013e0e6b835081b62f939f0bfccecacf2de07f8476c57fcc5`). Assim, o marcador de build prova que a auditoria ELF passou no runner, mas não foi possível reabrir o ELF nem inspecionar os Sony import metadata do OELF a partir do artefato preservado. Isso permanece pendente de publicação dos binários de auditoria ou de evidência equivalente.

**Classificação correta:** build/shader pipeline = `CI_VALIDATED`; testes host = `HOST_VALIDATED`; validação do PKG executada no job = validação automatizada local do próprio pipeline, não revisão independente clean-room. Não houve job independente de QA de pacote neste workflow. O marcador PASS cobre o ELF no runner; o ZIP não preservou os binários ELF/OELF/map para re-inspeção. Portanto, Sony import metadata do OELF continua não auditada independentemente.

O compute harness permanece bloqueado por padrão: a compilação não define `PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE`, e `compute_smoke.cpp` retorna `RESULT=BLOCKED; REASON=COMPUTE_HARDWARE_GATES_NOT_CLEARED` antes de carregar GNM ou tentar dispatch. Portanto, o PKG compute é artefato de diagnóstico/CI e **não deve ser instalado** para testar dispatch.

Esta validação não resolve GPU VA/V#, InputUsage runtime, cache/coerência, nem constitui execução de hardware. Nenhum PKG foi staged, instalado ou executado.

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

1. Completar a inspeção binária do artefato: conferir OELF e metadados Sony de importação, além da auditoria ELF já aprovada.
2. Se o build/link falhar por símbolo que dependia da biblioteca removida, identificar o símbolo concreto e sua origem; não reintroduzir a dependência inteira sem análise.
3. Obter evidência primária para GPU VA/V# e InputUsage ou manter os respectivos gates bloqueados.
4. Solicitar revisão externa real do commit e do pacote neutro de critérios, registrando identidade, escopo, conflitos, achados e decisão somente quando houver submissão verificável.

**Decisão:** `IN_PROGRESS`. A remoção da dependência estática está aprovada no nível ELF/CI do commit `83d15edf`; o build, o marcador de auditoria e o payload do PKG passaram. Não se promove a aprovação para OELF/Sony import metadata, QA independente clean-room ou runtime. Revisão externa e validação de hardware continuam `NOT_RUN`/ `BLOCKED`.
