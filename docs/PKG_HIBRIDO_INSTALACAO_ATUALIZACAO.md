# Arquitetura do PKG híbrido — instalação inicial e atualização do Neural Engine

**Estado:** Release 008 foi staged; diagnóstico PS4 confirmou arquivos PRX/config regulares e temporários antigos preservados. Release 009 corrige a detecção de arquivos gerenciados com `open(O_NOFOLLOW)` + `fstat` somente para bits de tipo; build, QA independente e teste de hardware pendentes. Não instalar até todos os gates aplicáveis passarem e haver autorização explícita.  
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
| H3 | Temporários exclusivos e proteção contra symlink | **IMPLEMENTADO — CI PENDENTE** | `O_CREAT+O_EXCL`, caminhos exclusivos, rejeição de destinos não regulares e marcador de transação. Falta fault injection Orbis. |
| H4 | Idempotência e detecção de versão/hash | **IMPLEMENTADO — CI PENDENTE** | Hash igual + release igual retorna `ALREADY_CURRENT` sem backup; release menor é recusado; release igual com hash diferente é conflito. |
| H5 | Fault injection de cada write/fsync/rename/rollback | **PENDENTE** — adicionar harness de falhas e provar restauração de PRX, manifesto e `plugins.ini`. |
| H6 | QA independente do PKG exato | **PENDENTE — RELEASE 008** | O QA anterior do PKG 006 não valida este novo eboot. Exigir inspeção independente do artefato 008 exato. |
| H7 | Teste controlado de instalação inicial e atualização | Evidência de console, logs, rollback e carregamento |
| H8 | Decisão sobre atualização do próprio app via Package Manager | Somente após prova separada do fluxo base/patch ou reinstalação |

## 9. Estado atual e bloqueios

- O build atual gera um PKG full/base `gde`, não um patch.
- O build gera `pne-release.ini` com identidade e SHA-256 do PRX; o eboot agora valida schema, TITLE_ID, CONTENT_ID, nome do PRX e SHA-256 antes de criar diretórios ou alterar destinos. A validação usa SHA-256 implementado localmente e vetores de teste host.
- Temporários de PRX, manifesto e `plugins.ini` usam nomes exclusivos com `O_CREAT | O_EXCL`; arquivos temporários preexistentes não são truncados. `plugins.ini` não é regravado quando a entrada gerenciada já existe.
- A transação PRX + configuração preserva backups e tenta rollback, mas os dois caminhos não podem ser trocados atomicamente em conjunto.
- O release candidato 008 integra a máquina de estados, manifesto instalado ao lado do PRX, marcador de transação persistente, migração LEGACY com backup, recusa de downgrade e `ALREADY_CURRENT` por release/hash. Host CI passou no run `44847096221` no commit `b05968a`; build do PKG 008 passou no run `44847096234`; QA independente do artefato exato continua sendo gate separado.
- O workflow de build preserva GP4/SFO/manifest e QA independente; o resultado do PKG 006 não pode ser reaproveitado como evidência para o PKG 008.
- Não há evidência de instalação/atualização do pacote no PS4 Pro nem de carregamento bem-sucedido do PRX.

**Decisão:** seguir com um único PKG full/base cujo eboot faz instalação inicial e atualização idempotente do plugin GoldHEN. Não rotular o arquivo como base+patch do Package Manager até existir evidência técnica de um mecanismo PS4 válido para isso. O pacote permanece bloqueado para instalação real até CI, QA independente do artefato exato, fault injection e autorização explícita.
