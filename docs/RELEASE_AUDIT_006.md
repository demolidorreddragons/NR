# Auditoria estática de release — PKG 006

**Data:** 20/11/2026  
**Branch:** `feature/gcn8-numeric-kernels`  
**Build examinado:** [GitHub Actions run 44669637925](https://github.com/demolidorreddragons/NR/actions/runs/44669637925)  
**Commit de origem do build:** `a14bb99dd9eef131f3768cd2c9beca0fb3a08119`  
**PKG:** `PS4-Neural-Engine-006.pkg`  
**SHA-256:** `7bd4b2b31fcadcc549ccb81186cad56dca515a02bb3384a53d09f5b7799d6422`

## Escopo e limites

Esta é uma revisão estática dos scripts, workflows e logs disponíveis no repositório. Não foi feita instalação, ativação de PRX, alteração de `plugins.ini`, escrita FTP, nem execução no console. Não equivale a teste independente nem a certificação Sony.

## Evidências positivas

- Job de build terminou com conclusão `success`.
- `PkgTool.Core` foi compilado e o PRX foi gerado.
- PKG foi construído; o validador do próprio pipeline aceitou os hashes e assinaturas reportados.
- O PKG foi extraído e os arquivos `eboot.bin`, `ps4_neural_engine.prx`, `keystone`, `right.sprx`, `libc.prx` e `libSceFios2.prx` foram encontrados.
- O artefato `ps4-neural-engine-build` foi enviado ao GitHub Actions.

## Achados que impedem aprovação para instalação

### R1 — Não há QA independente associado a este build (ALTO)

O workflow `.github/workflows/ps4-base-pkg-qa-006.yml` só aceita eventos `workflow_run` do build em `branches: [main]`. O build 44669637925 foi produzido na branch `feature/gcn8-numeric-kernels`. Portanto, a execução bem-sucedida do build não demonstra que o inspetor independente OrbisPkgTool validou este artefato específico.

**Ação:** permitir explicitamente a branch de trabalho no workflow de QA e exigir que o relatório contenha o SHA-256 exato do PKG antes de qualquer staging.

### R2 — Instalador substitui o PRX existente sem backup/rollback (ALTO)

Em `installer/main.c`, `copy_prx()` verifica a cópia byte a byte e então faz `rename(DEST_TMP, DEST_PRX)`. Se já existir um PRX instalado, ele é substituído. Não há backup recuperável do PRX anterior. Se `configure_loader()` falhar depois da substituição, o novo PRX permanece no destino, embora a configuração do loader não tenha sido atualizada. Uma versão incompatível pode ser carregada em um reinício futuro se a entrada já existia.

**Ação:** implementar backup atômico do PRX anterior, transação para PRX + configuração, rollback em qualquer falha e procedimento de restauração documentado. Criar testes de falha simulada antes de autorizar instalação real.

### R3 — O instalador modifica a configuração persistente do GoldHEN (ALTO / operação sensível)

Ao ser iniciado, o eboot cria diretórios, copia o PRX para `/data/GoldHEN/plugins` e adiciona uma entrada em `/data/GoldHEN/plugins.ini`. Isso é comportamento esperado do instalador, mas significa que simplesmente abrir o aplicativo não é um teste somente de leitura.

**Ação:** não executar o instalador no console sem autorização explícita, backup verificado e plano de rollback. Instalar o PKG e executar o eboot são operações diferentes; não confundir uma com a outra.

### R4 — Verificação de cópia não é validação de compatibilidade do PRX (MÉDIO)

O instalador compara os bytes copiados com os bytes de origem, mas não valida identidade, build ID, versão de ABI, dependências ou hash esperado do PRX antes de substituir o arquivo ativo.

**Ação:** adicionar manifesto de release com hash esperado, versão do plugin, dependências e compatibilidade declarada; rejeitar artefatos não correspondentes.

### R5 — Build do PRX não comprova carregamento no GoldHEN (ALTO)

O README do plugin declara que carregamento no console e comportamento em firmware 14.02 permanecem sem verificação. A presença do PRX no PKG e sua compilação não comprovam que o GoldHEN consegue carregá-lo, nem que hooks e ciclo de vida são estáveis.

**Ação:** somente após QA do pacote e plano de recuperação, realizar teste controlado autorizado de carregamento, com captura de logs e capacidade de desativação/rollback.

### R6 — Produto ainda é scaffold de diagnóstico (BLOQUEADOR DE RELEASE DO RENDERER)

O PRX registra telemetria de apresentação; o README declara explicitamente que não há backend GNM compute, inferência neural, reconstrução temporal ou adaptador genérico de framebuffer. Os shaders GCN8 são bring-up numérico e não um renderer completo.

**Ação:** não rotular PKG 006 como release funcional de Neural Rendering. Classificar como instalador/scaffold de diagnóstico.

## Decisão desta auditoria

**DECISÃO: NÃO APROVADO PARA INSTALAÇÃO NO PS4 PRO NESTA ETAPA.**

O build é aprovado somente no escopo de geração e validação estrutural feita pelo pipeline. Permanecem pendentes a QA independente deste hash, o hardening transacional do instalador e a validação real de carregamento no hardware.

## Próximas ações autorizadas sem tocar no console

1. Ajustar o workflow independente para inspecionar builds da branch de trabalho.
2. Acrescentar testes de falha/rollback ao instalador usando ambiente host/sandbox.
3. Criar manifesto de release com hashes e identidade do pacote.
4. Reexecutar build e QA independente; comparar o hash validado com o artefato exato.
5. Só depois apresentar um plano separado de teste de console para autorização explícita.

**Regra:** nenhum workflow de staging, instalação, PRX deployment, alteração de `plugins.ini` ou reinício do GoldHEN será disparado como parte desta auditoria.


## Reavaliação incremental — 20/11/2026

O relatório acima permanece válido para o artefato antigo (run `44669637925`, SHA-256 `7bd4b2b31fcadcc549ccb81186cad56dca515a02bb3384a53d09f5b7799d6422`). Foram feitas correções de engenharia posteriores, ainda sem autorização para instalação:

- **R1 — QA independente:** foi adicionado ao workflow de build um job separado em runner Windows que baixa o artefato do mesmo run e usa OrbisPkgTool fixado em commit imutável. O job verifica GP4/CONTENT_ID/arquivos associados, PKG, SFO, manifesto, hash do PRX e extração. A primeira execução sobre o build atualizado ainda está pendente; R1 não é considerado PASS.
- **R2 — rollback:** o instalador agora mantém backups de PRX e `plugins.ini`, prepara ambos os arquivos e tenta restaurá-los em falhas de promoção. A transação ainda não é atômica entre dois caminhos e não foi submetida a fault injection no filesystem Orbis; R2 está parcialmente mitigado, não fechado.
- **R4 — manifesto/hash:** o eboot valida schema, TITLE_ID, CONTENT_ID, nome do PRX e SHA-256 do PRX antes de criar diretórios ou alterar destinos. Testes host de vetores SHA-256 e casos inválidos passaram no run `44678921838`. Isso autentica a integridade/identidade declarada, não a ABI nem a compatibilidade de carregamento.
- **R3/R5/R6:** permanecem inalterados. Executar o eboot continua sendo uma operação persistente; não há prova de carregamento no GoldHEN nem renderer neural operacional.

**Decisão permanece: NÃO APROVADO PARA INSTALAÇÃO NO PS4 PRO.** O artefato do run `44677990694` foi construído antes da nova validação SHA-256 no eboot e antes do novo job QA; não o tratar como substituto do próximo build auditado. Nenhuma operação no console foi realizada.

- A primeira tentativa de compilar o novo eboot com SHA-256 (run `44678927683`) chegou à etapa de PKG e falhou porque `-std=c11` ocultou as declarações POSIX `fdopen`/`fileno` no sysroot Orbis. O build foi ajustado para `-std=gnu11` (commit `748539b1805069efa69c45c24e1d35bb796b204e`); é necessária nova execução antes de avaliar o artefato. Nenhum PKG atualizado foi produzido por essa tentativa.
