# Arquitetura do PKG híbrido — instalação inicial e atualização do Neural Engine

**Estado:** Release 009 (APP_VER 01.05) foi compilado, aprovado por QA independente PC-only e staged em `/data/pkg/PS4-Neural-Engine-009.pkg`. Diagnóstico do PS4 confirmou arquivos PRX/config regulares e temporários antigos preservados. A correção usa `open(O_NOFOLLOW)` + `fstat` somente para bits de tipo. Não instalado nem ativado; hardware/runtime ainda não testados. Não instalar até haver autorização explícita.  
**Alvo:** PS4 Pro / Orbis + GoldHEN.  
**Branch:** `feature/gcn8-numeric-kernels`.  
**Identidade atual:** TITLE_ID `PNEU00001`, CONTENT_ID `IV0000-PNEU00001_00-PS4NEURALENG0000`.

## 1. Objetivo

Entregar um único PKG distribuível que contenha o instalador e o payload do Neural Engine. O mesmo instalador deve reconhecer os dois cenários e executar a operação adequada:

- **Instalação inicial:** não existe PRX do Neural Engine no destino; instalar o payload e registrar o plugin no GoldHEN.
- **Atualização:** já existe uma versão gerenciada do PRX; validar a nova versão, preservar a anterior, substituir o payload e manter a configuração do GoldHEN sem duplicar entradas.

O usuário não deve precisar escolher ou baixar um PKG diferente para instalar ou atualizar o **plugin**.

## 2. Limite importante do formato PS4

O termo “híbrido” aqui descreve o comportamento do instalador e do payload do plugin, não uma categoria híbrida do sistema de pacotes Sony.

O PKG atual é um **full/base application** (`CATEGORY=gde`). Um PKG patch PS4 usa outra categoria e depende de um pacote base compatível. Um único PARAM.SFO não deve ser anunciado como simultaneamente base e patch, nem o pacote atual deve ser renomeado como patch para simular compatibilidade.

Assim, há duas operações diferentes:

1. **Instalar/atualizar o plugin GoldHEN:** pode ser feito pelo mesmo PKG instalador, com lógica idempotente no eboot. É o objetivo desta especificação.
2. **Atualizar o próprio aplicativo instalado pelo Package Manager:** é o mecanismo de atualização de aplicações PS4 e exige fluxo base/patch correto. Não está comprovado que reinstalar o atual `gde` substitua uma instalação existente em todos os ambientes. Não prometer essa capacidade até testá-la num ambiente descartável e controlado.

## 3. Conteúdo do único PKG

O pacote full/base deve conter, no mínimo:

- `eboot.bin`: instalador Orbis;
- `ps4_neural_engine.prx`: payload GoldHEN;
- `sce_sys/param.sfo`: identidade e versão do aplicativo instalador;
- ícone e módulos runtime exigidos pelo OpenOrbis.

Adicionar um manifesto versionado, por exemplo `pne-release.ini`, com:

- schema do manifesto;
- versão do payload PRX;
- versão do instalador;
- TITLE_ID/CONTENT_ID esperados;
- SHA-256 do PRX empacotado;
- ABI/interface do plugin;
- firmware/GoldHEN mínimos e máximos **somente quando comprovados**;
- canal/tipo de release.

O instalador deve validar o manifesto e o hash do PRX antes de tocar nos destinos persistentes. Versões desconhecidas, manifesto inválido, hash divergente ou ABI incompatível devem abortar sem modificar o plugin ativo.

## 4. Fluxo de decisão

1. Ler e validar manifesto e PRX empacotado.
2. Verificar os destinos e backups existentes; não seguir links simbólicos nem truncar arquivos temporários preexistentes.
3. Identificar o estado:
   - **NEW:** não existe PRX gerenciado;
   - **UPDATE:** existe PRX e manifesto de instalação válido;
   - **LEGACY:** existe PRX, mas não há manifesto confiável;
   - **RECOVERY:** há backup pendente de uma operação anterior;
   - **CONFLICT:** arquivos, versões ou identidade não correspondem ao esperado.
4. NEW e UPDATE podem prosseguir após validação.
5. LEGACY exige migração conservadora: preservar o arquivo anterior e não inferir versão/ABI pelo nome.
6. RECOVERY deve parar e solicitar inspeção/restauração; nunca sobrescrever backup.
7. CONFLICT aborta sem modificar o estado ativo.

## 5. Commit seguro

A implementação deve usar arquivos temporários exclusivos no mesmo filesystem dos destinos:

- criar com `O_CREAT | O_EXCL` (e `O_NOFOLLOW` se suportado pelo SDK);
- nunca abrir temporário preexistente com truncamento;
- escrever, verificar tamanho por fluxo e SHA-256;
- fazer `fsync` do arquivo e, se suportado, do diretório após renames;
- preservar cópias recuperáveis do PRX e de `plugins.ini`;
- promover os arquivos somente após todas as validações;
- em falha, tentar rollback e registrar separadamente a falha original e cada falha de restauração.

PRX e `plugins.ini` ficam em caminhos distintos; POSIX não fornece uma troca atômica conjunta. Portanto o procedimento é uma transação recuperável com janela residual de falha, não atomicidade absoluta. O instalador deve documentar esse limite e não apagar backups automaticamente.

## 6. Idempotência e atualização

Executar o instalador duas vezes com o mesmo payload deve produzir o mesmo estado final:

- uma única entrada do PRX na seção `[default]`;
- mesmo hash e versão do payload;
- nenhuma duplicação de seção/linha;
- nenhuma substituição desnecessária de arquivos se o hash já for igual;
- nenhum backup novo se não houve alteração;
- logs indicando `NEW_INSTALL`, `UPDATE`, `ALREADY_CURRENT`, `RECOVERY_REQUIRED` ou `ABORTED`.

Se a versão empacotada for igual à instalada e os hashes coincidirem, retornar `ALREADY_CURRENT`. Se for inferior, recusar downgrade por padrão. Downgrade deve exigir procedimento separado e explícito, com backup preservado.

## 7. Compatibilidade e proteção

- Não assumir que copiar o PRX prova compatibilidade ABI ou que GoldHEN o carregará.
- Não modificar saves, dados de jogos, plugins não relacionados ou outras configurações.
- Preservar todo o conteúdo preexistente de `plugins.ini`; editar somente a entrada gerenciada.
- Não iniciar, habilitar ou reiniciar o plugin automaticamente durante a instalação.
- Manter instalação do PKG, execução do eboot, ativação do PRX e reboot como etapas distintas.
- Não realizar operação FTP ou no console como parte do build/QA automatizado.

## 8. Plano de implementação

| Etapa | Trabalho | Critério de saída |
|---|---|---|
| H1 | Manifesto de release e hash do PRX no build/runtime | **IMPLEMENTADO — CI REVALIDAÇÃO PENDENTE** | Build e eboot verificam identidade e hash; teste host do SHA-256 passou. |
| H2 | Máquina de estados NEW/UPDATE/LEGACY/RECOVERY/CONFLICT | **IMPLEMENTADO — HOST CI PASS** | Eboot classifica presença/validade do PRX, manifesto instalado e marcador transacional; erro de inspeção e destino não regular/symlink são conflito. |
| H3 | Temporários exclusivos e proteção contra symlink | **IMPLEMENTADO — HOST CI PASS; ORBIS BUILD PASS** | `O_CREAT+O_EXCL`, caminhos exclusivos, inspeção por `open(O_NOFOLLOW)`/`fstat` dos bits de tipo e marcador de transação. Falta fault injection Orbis. |
| H4 | Idempotência e detecção de versão/hash | **IMPLEMENTADO — HOST CI PASS; ORBIS BUILD PASS** | Hash igual + release igual retorna `ALREADY_CURRENT` sem backup; release menor é recusado; release igual com hash diferente é conflito. |
| H5 | Fault injection de cada write/fsync/rename/rollback | **PENDENTE** — adicionar harness de falhas e provar restauração de PRX, manifesto e `plugins.ini`. |
| H6 | QA independente do PKG exato | **PASS — RELEASE 009 (PC-ONLY)** | QA independente validou PKG SHA-256 `242e032dc19ae9fc22cdd0b977241bd9ef8edf93bc5eb31777063575430699ba`, estrutura, digests, SFO semântico e PRX embarcado; não é teste de hardware. |
| H7 | Teste controlado de instalação inicial e atualização | Evidência de console, logs, rollback e carregamento |
| H8 | Decisão sobre atualização do próprio app via Package Manager | Somente após prova separada do fluxo base/patch ou reinstalação |

## 9. Estado atual e bloqueios

- O build atual gera um PKG full/base `gde`, não um patch.
- O build gera `pne-release.ini` com identidade e SHA-256 do PRX; o eboot agora valida schema, TITLE_ID, CONTENT_ID, nome do PRX e SHA-256 antes de criar diretórios ou alterar destinos. A validação usa SHA-256 implementado localmente e vetores de teste host.
- Temporários de PRX, manifesto e `plugins.ini` usam nomes exclusivos com `O_CREAT | O_EXCL`; arquivos temporários preexistentes não são truncados. `plugins.ini` não é regravado quando a entrada gerenciada já existe.
- A transação PRX + configuração preserva backups e tenta rollback, mas os dois caminhos não podem ser trocados atomicamente em conjunto.
- O release 009 (APP_VER 01.05) integra a máquina de estados, manifesto instalado ao lado do PRX, marcador de transação persistente, migração LEGACY com backup, recusa de downgrade e `ALREADY_CURRENT` por release/hash. Host CI passou no run `44849066490`; build Orbis/PKG passou e QA independente do artefato exato passou no run `44849066524`.
- QA independente: PKG SHA-256 `242e032dc19ae9fc22cdd0b977241bd9ef8edf93bc5eb31777063575430699ba`, 6.619.136 bytes, `FULL_BASE_APPLICATION_NOT_PATCH`, `CATEGORY=gde`, `APP_VER=01.05`, `VERSION=01.05`, `TITLE_ID=PNEU00001`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`, `st_type=digital50`, PRX SHA-256 `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`, `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- O staging PS4 passou no run `44849611210`: arquivos PKG antigos Neural Rendering foram arquivados com hash e read-back verificados em `/data/pkg/old/`; somente `PS4-Neural-Engine-009.pkg` ficou na raiz `/data/pkg/`; pacotes de terceiros foram preservados. PKG final foi relido do console e SHA/bytes conferidos. `INSTALLATION_PERFORMED=FALSE`.
- Permanecem pendentes fault injection Orbis e teste de hardware/runtime. O resultado do PKG 006/008 não substitui QA do 009.
- Não há evidência de instalação/atualização do pacote no PS4 Pro nem de carregamento bem-sucedido do PRX.

**Decisão:** seguir com um único PKG full/base cujo eboot faz instalação inicial e atualização idempotente do plugin GoldHEN. Não rotular o arquivo como base+patch do Package Manager até existir evidência técnica de um mecanismo PS4 válido para isso. O pacote permanece bloqueado para instalação real até fault injection aplicável e autorização explícita.
