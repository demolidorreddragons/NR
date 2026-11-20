# PS4 Pro — plano de teste reversível (PKG 006)

**Estado: RASCUNHO — NÃO AUTORIZADO PARA EXECUÇÃO.** Este documento define pré-condições e recuperação. Não concede permissão para instalar o PKG, executar o EBOOT, alterar arquivos persistentes, reiniciar GoldHEN, ativar o PRX ou fazer dispatch GNM.

## 1. Escopo e limites

O PKG 006 é um full base application `CATEGORY=gde`, não um patch. Seu EBOOT, quando iniciado, valida `/app0/pne-release.ini` e o SHA-256 do PRX antes de gravar. Depois, cria/usa `/data/GoldHEN/plugins`, copia o PRX para `/data/GoldHEN/plugins/ps4_neural_engine.prx` e altera `/data/GoldHEN/plugins.ini`. A instalação do app e a execução do instalador são duas operações distintas.

Este primeiro teste, se posteriormente autorizado, deve limitar-se a provar a instalação/cópia/registro e a capacidade de recuperação. Não inclui:
- alterar saves, dados de usuário, jogos ou outros plugins;
- editar `plugins.ini` manualmente como parte do teste;
- ativar/reiniciar GoldHEN automaticamente ou alterar configurações;
- executar shaders, GNM dispatch, inferência neural ou benchmark;
- atualizar firmware ou GoldHEN;
- remover backups automaticamente.

## 2. Identidade do artefato

Antes de qualquer ação, fixar um único PKG por SHA-256 e verificar:
- PKG 006 e GP4 correspondentes ao mesmo workflow run;
- `TITLE_ID=PNEU00001`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`;
- `CATEGORY=gde`, `APP_VER=01.02`, `VERSION=01.02`, `storage=digital50`;
- manifesto com `schema=1`, nome PRX esperado e hash correspondente ao PRX dentro do PKG;
- QA independente PC-only PASS para estrutura, digests, assinaturas PFS, extração completa e consistência do SFO/manifesto/PRX.

Um artefato diferente exige nova validação e novos hashes. Não reutilizar aprovação de um build anterior.

## 3. Pré-condições read-only

Coletar e arquivar, sem alterar o console:
1. Modelo exato (PS4 Pro/NEO), firmware e versão/build do GoldHEN.
2. Identidade e versão da instalação atual do Neural Engine, se existir.
3. Hash e cópia externa do PRX existente em `/data/GoldHEN/plugins/ps4_neural_engine.prx`, se existir.
4. Cópia externa byte a byte de `/data/GoldHEN/plugins.ini`, se existir, mais SHA-256.
5. Estado dos caminhos `*.pne-backup`, `*.pne-tmp`; se qualquer um existir, parar e investigar — não apagar nem sobrescrever.
6. Cópia do log atual `/data/GoldHEN/ps4_neural_engine_install.log`, se existir.
7. Espaço livre e capacidade de recuperar os arquivos originais por um canal já testado.

Se não for possível obter e verificar cópias de recuperação, parar. Não presumir que FTP está disponível nem que o console pode ser acessado remotamente.

## 4. Gate de autorização

A execução requer aprovação explícita separada para:
- instalar/substituir o aplicativo PKG 006 pelo Package Manager;
- iniciar o aplicativo instalador, que grava em `/data/GoldHEN/plugins` e `/data/GoldHEN/plugins.ini`;
- qualquer reinício do GoldHEN/console ou tentativa de carregamento do PRX.

A aprovação de um item não autoriza os demais. Até essa aprovação, somente leitura e análise de artefatos são permitidas.

## 5. Execução controlada (somente após aprovação)

1. Confirmar novamente hashes e backups read-only imediatamente antes do teste.
2. Instalar o PKG pelo mecanismo apropriado ao console. Não assumir que o Package Manager fará upgrade seguro de qualquer app existente; se houver conflito de identidade/versão, parar.
3. Iniciar o instalador uma única vez e aguardar retorno/log.
4. Capturar o log e os hashes dos arquivos resultantes.
5. Comparar o PRX instalado byte a byte com o PRX do PKG e verificar a entrada do plugin apenas na seção `[default]`.
6. Não reiniciar GoldHEN nem permitir carregamento até revisar os resultados e obter aprovação separada.
7. Se houver qualquer divergência, erro, backup inesperado ou estado ambíguo, parar e executar recuperação supervisionada.

## 6. Recuperação

Os backups usados pelo instalador são:
- `/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-backup`
- `/data/GoldHEN/plugins.ini.pne-backup`

Não apagar esses backups após sucesso. Em falha:
1. Não repetir o instalador.
2. Preservar o log, os arquivos atuais, temporários e backups para análise.
3. Comparar hashes com as cópias externas pré-teste.
4. Restaurar somente os arquivos que foram efetivamente alterados, a partir das cópias verificadas, preservando outros plugins e entradas.
5. Revalidar os hashes restaurados e a configuração.
6. Se o estado não puder ser determinado com certeza, não improvisar comandos de remoção/renomeação; interromper e exigir recuperação manual supervisionada.

O rollback do instalador é best-effort: duas renomeações em caminhos diferentes não são uma transação atômica. Falha de energia/kernel durante o commit ainda pode exigir recuperação manual.

## 7. Critérios de resultado

**PASS limitado:** PKG instalado sem conflito, manifesto/hash aceitos, PRX copiado idêntico, entrada correta registrada, log de sucesso e backups originais preservados.

**FAIL:** qualquer hash divergente, alteração de entradas alheias, erro de manifesto/cópia/registro, backup sobrescrito, ou recuperação incompleta.

**BLOCKED:** firmware/GoldHEN desconhecido, cópias de recuperação ausentes, conflito de app, caminhos temporários/backups preexistentes, log inconclusivo, ou impossibilidade de verificar o estado.

Um PASS limitado não prova carregamento/ABI do PRX, compatibilidade de firmware, runtime GNM, shader compute, inferência ou renderer neural. Esses são gates separados.
