# PS4 PKG installer

PKG 009 is the current full base-application installer (not a patch). It carries APP_VER/VERSION 01.05 and corrects PS4 managed-file conflict detection by probing opened descriptors with `O_NOFOLLOW` and using `fstat` only for file-type bits (never `st_size`). Existing regular PRX/config files are recognized without following symlinks; inaccessible, symlink, or non-regular paths remain fail-closed. Existing stale `.pne-tmp` and backup files are preserved and bypassed by exclusive numbered staging names.

PKG 007 is a full base-application installer (not a patch) intended for a jailbroken PS4. It carries APP_VER/VERSION 01.03 and a freshly built eboot with atomic, collision-resistant temporary-file reservation for both the PRX and plugins.ini, in addition to byte-accurate PRX copy verification. This is deliberate: the audited console installation was an older `CATEGORY=gd`, `APP_VER=1.00` base, not the `gde` PKG 002 that patch 003 targeted. Install it through GoldHEN/Debug Package Installer, then launch **PS4 Neural Engine Installer** once.

The installer first validates `/app0/pne-release.ini` (schema, release identity, PRX filename and SHA-256 of `/app0/ps4_neural_engine.prx`) before creating the GoldHEN destination directory or changing persistent plugin/configuration files. The SHA-256 implementation is included in the installer and covered by host test vectors.

The installer:
1. Copies `/app0/ps4_neural_engine.prx` to `/data/GoldHEN/plugins/ps4_neural_engine.prx` using a temporary file.
2. Verifies the copied PRX by reading both files back and comparing every byte; it does not trust PS4 `stat`/`fstat` size metadata. Interrupted reads (`EINTR`) are retried, and short reads are accumulated independently on each stream before byte comparison.
3. Creates or updates `/data/GoldHEN/plugins.ini`, preserving existing content and adding the PRX path under the exact `[default]` section.
4. Writes `/data/GoldHEN/ps4_neural_engine_install.log` with the result for FTP diagnostics. This file can distinguish a successful installer exit from a failure before the PRX copy or loader registration.

After a successful install, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.


For troubleshooting, the installer log records the failing operation and the platform `errno` value (for example, source lookup/open, temporary destination creation, write, verification, or final rename). Capture the log after the latest launch attempt; older log contents do not describe a newer attempt.

## Release 006 — histórico da auditoria estática (20/11/2026)

O PKG 006 passou pelo build, validação estrutural e extração do pipeline no run [44669637925](https://github.com/demolidorreddragons/NR/actions/runs/44669637925). Isso não é aprovação para instalação.

A auditoria estática identificou dois bloqueios relevantes antes de qualquer teste no console:
- o workflow de QA independente 006 filtrava somente builds da branch `main`, portanto não inspecionava o artefato produzido na branch `feature/gcn8-numeric-kernels`;
- o instalador substitui o PRX existente sem manter backup/rollback transacional se a atualização de `plugins.ini` falhar.

O filtro do workflow de QA foi ampliado para incluir a branch de trabalho. A próxima execução do build deverá acionar o inspetor independente para o artefato exato. Isso ainda não executa QA retroativamente no PKG 006 já produzido.

**Decisão atual: NÃO APROVADO PARA INSTALAÇÃO NO PS4 PRO.** Não executar o eboot do instalador: ele grava em `/data/GoldHEN/plugins` e altera `/data/GoldHEN/plugins.ini`. Nenhuma operação no console está autorizada por esta auditoria.

Relatório completo: [docs/RELEASE_AUDIT_006.md](RELEASE_AUDIT_006.md).

## Transação e recuperação — revisão em 20/11/2026

O instalador foi alterado para não ativar o PRX imediatamente após a cópia:
1. O PRX novo é copiado para `DEST_TMP` e verificado byte a byte, mas permanece em staging.
2. A nova configuração `plugins.ini` também é preparada em `CONFIG_TMP`, inclusive quando a entrada do plugin já existe.
3. Antes do commit, arquivos existentes são movidos para `*.pne-backup`. Se esse nome já existir, a nova cópia recebe um sufixo numérico (`.001`, `.002` etc.); backups anteriores nunca são apagados nem sobrescritos. O instalador aborta somente se não conseguir reservar um nome livre ou mover os arquivos com segurança.
4. O PRX staged e a configuração staged são promovidos. Se uma promoção falhar, o instalador tenta remover o PRX novo e restaurar os arquivos anteriores.
5. Backups anteriores são retidos mesmo após sucesso; não são apagados automaticamente.

**Limitações:** dois arquivos em caminhos distintos não podem ser trocados atomicamente como uma única operação POSIX. A rotina reduz a janela de inconsistência e tenta rollback, mas falha de energia/kernel no meio do commit ainda pode exigir recuperação manual. Se o rollback também falhar, o log indica que os backups devem ser inspecionados.

**Recuperação manual:** antes de nova tentativa, verificar `/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-backup` e `/data/GoldHEN/plugins.ini.pne-backup`. Não apagar backups automaticamente. Restaurar somente após identificar qual versão está ativa e preservar uma cópia dos logs.

A revisão estática não substitui compilação, testes de falha em filesystem compatível, QA independente do PKG, nem teste de hardware.

### Correção de colisão de backups (2026-11-20)

O commit `d8c55173e4324dbf66e882d82c18a2be039aa7ce` corrige o bloqueio em que a existência de `*.pne-backup` impedia permanentemente uma atualização subsequente. Agora o instalador reserva nomes de backup exclusivos com sufixo numérico e preserva integralmente os backups anteriores. O rollback usa os caminhos efetivamente reservados. Nenhum backup existente é removido ou sobrescrito. A correção precisa passar pelo build/QA do PKG e continua sem autorização para instalação no console. O instalador continua **não aprovado para instalação** até esses gates passarem.

A verificação automatizada de invariantes do instalador foi adicionada ao workflow `host-tests` e está sendo executada também na branch `feature/gcn8-numeric-kernels`. A aprovação desse teste de host não representa execução do binário no FreeBSD/Orbis nem validação de operações `rename` no filesystem do PS4.

### Manifest integrity preflight (2026-11-20)

`installer/release_manifest.c` now rejects missing/duplicate required fields, unsupported schema, mismatched TITLE_ID/CONTENT_ID, unexpected PRX filename, malformed SHA-256, and a PRX whose computed SHA-256 differs from the manifest. This check runs before `mkdir` or staging. Host tests cover a valid manifest, SHA mismatch, wrong CONTENT_ID, duplicate hash key, missing field, malformed hash, and the SHA-256 `abc` test vector. Host CI run `44678921838` passed this test and the rest of the host suite. This does not establish PRX ABI compatibility, GoldHEN loading, or hardware readiness.


## Ícone oficial do aplicativo — regra obrigatória (2026-11-20)

O SVG simplificado foi reprovado e não pode voltar a ser usado como substituto da arte aprovada. O PKG deve incorporar a imagem raster original aprovada pelo usuário, sem redesenho, composição alternativa, logos vetoriais aproximados ou geração por IA. A única transformação permitida é o redimensionamento proporcional da imagem quadrada original para 512×512 com filtro Lanczos, preservando toda a composição.

Fonte canônica esperada no repositório: `assets/neural-rendering-icon-source.png`. `tools/make_icon.py` falha explicitamente se essa imagem não estiver presente; não produz fallback. A imagem oficial foi fornecida pelo usuário e a prévia 512×512 foi explicitamente confirmada por ele como o ícone correto. A composição aprovada é a água-viva/UFO de fumaça monocromática em tons de prata e branco sobre fundo preto, logotipo PlayStation e identificação PS4 Pro na cúpula, e a inscrição “NEURAL RENDERING” na base. O upload original é JPEG 1536×1536, SHA-256 `3559eba63add88b4145982d99a6d696719e4a76bb4f3d08c26a02c89fc4fcbf5`. O PNG final aprovado para o aplicativo é 512×512, SHA-256 `7fe6a11ad7b4d37e5741c77822730a82487237dbed4a3eca529e995a29421ae4`. Este PNG confirmado deve ser incorporado sem qualquer nova transformação em `assets/neural-rendering-icon-source.png`; não substituir, redesenhar, recolorir, reinterpretar ou gerar outra versão. A integração binária no repositório ainda precisa ser feita antes do próximo PKG. Nenhum novo PKG com ícone substituto pode ser aprovado.


## Release 007 — correção de colisão de staging (21/11/2026)

O diagnóstico remoto do console identificou errno=17 (EEXIST) ao criar ps4_neural_engine.prx.pne-tmp. A listagem confirmou que esse temporário antigo permaneceu no diretório de plugins. A versão 007 corrige a causa sem apagar esse arquivo nem qualquer backup.

- PRX e plugins.ini agora reservam nomes temporários exclusivos por open(O_CREAT|O_EXCL), tentando o nome-base e depois sufixos .001 até .9999.
- A repetição ocorre somente em EEXIST; outros erros são reportados imediatamente.
- Os caminhos efetivamente reservados são transportados pela cópia, verificação, commit e rollback. Arquivos temporários antigos permanecem intactos.
- PKG_BUILD_NUMBER=007; APP_VER=01.03; TITLE_ID e CONTENT_ID permanecem estáveis para a mesma aplicação.

### Arquitetura híbrida: instalar e atualizar o plugin

A especificação normativa é [PKG_HIBRIDO_INSTALACAO_ATUALIZACAO.md](PKG_HIBRIDO_INSTALACAO_ATUALIZACAO.md). O objetivo acordado é **um único PKG full/base (CATEGORY=gde) cujo eboot instala o plugin GoldHEN na primeira execução e atualiza o plugin nas execuções seguintes**, preservando a versão anterior, mantendo plugins.ini sem duplicatas e operando de forma idempotente. Para esse fluxo do plugin, não se deve exigir um PKG patch separado nem desinstalação do aplicativo.

É essencial distinguir duas camadas:
- **Plugin GoldHEN:** instalação inicial e atualização são responsabilidade do mesmo eboot híbrido, conforme a especificação acima.
- **Aplicativo instalador no Package Manager do PS4:** o PKG atual é base (gde), não patch. A substituição do próprio aplicativo sem desinstalar é um fluxo separado do sistema de pacotes PS4 e ainda não foi comprovada. Não confundir essa limitação com a atualização do PRX pelo instalador híbrido.

O usuário relatou que o Package Manager exigiu desinstalar a versão anterior do aplicativo. Isso evidencia que a atualização do próprio app ainda não está resolvida; não altera o requisito já acordado de que o eboot do único PKG deve instalar/atualizar o plugin. Não declarar H2/H4 concluídas até a máquina de estados NEW/UPDATE/LEGACY/RECOVERY/CONFLICT, detecção de versão/hash e idempotência estarem implementadas e testadas.


## Release 008 — instalador híbrido do plugin

O candidato 008 integra a máquina de estados NEW/UPDATE/LEGACY/RECOVERY/CONFLICT, persiste `pne-release.ini` ao lado do PRX, usa marcador de transação para detectar interrupção, preserva backups, recusa downgrade e retorna `ALREADY_CURRENT` quando release e SHA-256 coincidem. Entradas duplicadas do Neural Engine em `[default]` são normalizadas; se já houver exatamente uma, `plugins.ini` não é regravado. O estado de validação e os gates pendentes estão em `docs/PKG_HIBRIDO_INSTALACAO_ATUALIZACAO.md`. Isso não transforma o full/base PKG em patch de atualização do aplicativo no Package Manager.
