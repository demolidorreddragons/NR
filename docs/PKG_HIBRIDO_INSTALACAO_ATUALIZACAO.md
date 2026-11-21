# Arquitetura do PKG híbrido — instalação inicial e atualização do Neural Engine

**Estado:** Release 011 (APP_VER 01.07) passou host tests, build e QA independente PC-only e foi staged no PS4. O Package Manager recusou o PKG 011 com `Package already installed`; ele não foi instalado e seu eboot não foi executado. O PRX 011 é byte-idêntico ao 010; a alteração 011 foi no eboot/installer, sem correção comprovada para CE-34878-0. O fluxo separado de patch real foi implementado em `tools/build_patch_pkg.sh` e no job `build-patch`/`independent-patch-qa` do workflow `ps4-prx.yml`; aguarda execução e QA CI. Não desinstalar o aplicativo nem repetir a tentativa com o PKG 011.
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
7. CONFLICT normalmente aborta sem modificar o estado ativo. Exceção estrita: se o PRX temporário existente for arquivo regular, o manifesto instalado validar exatamente esse temporário, a versão do manifesto for igual à versão de entrada e o SHA-256 do temporário for idêntico ao PRX do PKG atual, o eboot pode recuperar a transação: cria uma segunda cópia temporária exclusiva, valida novamente, preserva o temporário original e promove PRX/manifesto com backups rotacionados. Se qualquer condição falhar, aborta sem alterar destinos. Marcador transacional presente continua exigindo inspeção manual.

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
- logs indicando `STARTED`, `NEW_INSTALL`, `UPDATE`, `ALREADY_CURRENT`, `RECOVERY_CANDIDATE`, `RECOVERY_COMMIT`, `RECOVERY_REQUIRED` ou `ABORTED`.

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
| H1 | Manifesto de release e hash do PRX no build/runtime | **IMPLEMENTADO — CI PASS RELEASE 011** | Build e eboot verificam identidade e hash; teste host do SHA-256 passou. |
| H2 | Máquina de estados NEW/UPDATE/LEGACY/RECOVERY/CONFLICT | **IMPLEMENTADO — HOST CI PASS** | Eboot classifica presença/validade do PRX, manifesto instalado e marcador transacional; erro de inspeção e destino não regular/symlink são conflito. |
| H3 | Temporários exclusivos e proteção contra symlink | **IMPLEMENTADO — HOST CI PASS; ORBIS BUILD PASS** | `O_CREAT+O_EXCL`, caminhos exclusivos, inspeção por `open(O_NOFOLLOW)`/`fstat` dos bits de tipo e marcador de transação. Falta fault injection Orbis. |
| H4 | Idempotência e detecção de versão/hash | **IMPLEMENTADO — HOST CI PASS; ORBIS BUILD PASS** | Hash igual + release igual retorna `ALREADY_CURRENT` sem backup; release menor é recusado; release igual com hash diferente é conflito. |
| H5 | Fault injection de cada write/fsync/rename/rollback | **PENDENTE** — adicionar harness de falhas e provar restauração de PRX, manifesto e `plugins.ini`. |
| H6 | QA independente do PKG exato | **PASS — RELEASE 011 (PC-ONLY)** | QA independente validou o PKG 011 com SHA-256 `f7eb91f19356f92991536d36e2483a853907ddcd467df9579842383f8b87b7ef`; não é teste de hardware. |
| H7 | Teste controlado de instalação inicial e atualização | Evidência de console, logs, rollback e carregamento |
| H8 | Patch verdadeiro para atualizar o próprio app via Package Manager | **PIPELINE IMPLEMENTADO — CI PENDENTE** — usa base 010 verificada, GP4 `pkg_ps4_patch`, `CATEGORY=gpe`, `digital25` e validação independente; não instalar até QA passar |

## 9. Estado atual e bloqueios

- O build atual gera um PKG full/base `gde`, não um patch.
- O build gera `pne-release.ini` com identidade e SHA-256 do PRX; o eboot agora valida schema, TITLE_ID, CONTENT_ID, nome do PRX e SHA-256 antes de criar diretórios ou alterar destinos. A validação usa SHA-256 implementado localmente e vetores de teste host.
- Temporários de PRX, manifesto e `plugins.ini` usam nomes exclusivos com `O_CREAT | O_EXCL`; arquivos temporários preexistentes não são truncados. `plugins.ini` não é regravado quando a entrada gerenciada já existe.
- A transação PRX + configuração preserva backups e tenta rollback, mas os dois caminhos não podem ser trocados atomicamente em conjunto.
- Release 011 (APP_VER 01.07) passou host tests no run `44852720353`, build no run `44852720357`, QA independente PC-only e staging com read-back SHA-256 no run `44853027094` (attempt 2). PKG: 6.619.136 bytes, SHA-256 `f7eb91f19356f92991536d36e2483a853907ddcd467df9579842383f8b87b7ef`. PRX SHA-256 `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`, idêntico ao 010. O usuário tentou instalar 011, mas o Package Manager respondeu `Package already installed`; instalação e execução do eboot não ocorreram.
- QA independente: PKG SHA-256 `242e032dc19ae9fc22cdd0b977241bd9ef8edf93bc5eb31777063575430699ba`, 6.619.136 bytes, `FULL_BASE_APPLICATION_NOT_PATCH`, `CATEGORY=gde`, `APP_VER=01.05`, `VERSION=01.05`, `TITLE_ID=PNEU00001`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`, `st_type=digital50`, PRX SHA-256 `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`, `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- O staging PS4 passou no run `44849611210`: arquivos PKG antigos Neural Rendering foram arquivados com hash e read-back verificados em `/data/pkg/old/`; somente `PS4-Neural-Engine-009.pkg` ficou na raiz `/data/pkg/`; pacotes de terceiros foram preservados. PKG final foi relido do console e SHA/bytes conferidos. `INSTALLATION_PERFORMED=FALSE`.
- Diagnóstico read-only do PS4 em run `44852190436`: `plugins.ini` tem uma única entrada Neural Engine e `PluginLoader_Enabled=1`; PRX ativo tem 317.216 bytes, SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`; manifesto instalado declara release 010 e PRX SHA `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`; `.pne-tmp` e `.pne-backup.001` têm 224.128 bytes e correspondem ao hash esperado do release 010. Portanto, o PRX 010 está staged, mas o arquivo ativo ainda é outro binário. O log do instalador tem apenas mensagens antigas e não contém `STARTED`/`RECOVERY_COMMIT`; `/data/sce_coredumps/` não listou dumps; `/data/sce_logs/log.bin` tem 16 MiB e é praticamente todo zero, sem mensagens textuais. Nenhuma evidência permite atribuir causalmente o CE-34878-0. Nenhum arquivo do PS4 foi alterado pela coleta.
- Não há evidência de instalação/atualização do pacote no PS4 Pro nem de carregamento bem-sucedido do PRX.

**Decisão:** manter um único PKG full/base híbrido para instalação inicial e atualização do plugin GoldHEN. A atualização do aplicativo via Package Manager segue separada: o pipeline de patch 010→011 está no CI e ainda não tem resultado de build/QA. O release 011 full/base não foi instalado; não pedir nova tentativa e não desinstalar a base. Reiniciar GoldHEN não é recomendado antes de resolver a divergência do PRX ativo.
