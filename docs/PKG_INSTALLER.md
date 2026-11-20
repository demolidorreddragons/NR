# PS4 PKG installer

PKG 006 is a full base-application installer (not a patch) intended for a jailbroken PS4. It carries APP_VER/VERSION 01.02 and a freshly built eboot containing the corrected byte-stream PRX copier. This is deliberate: the audited console installation was an older `CATEGORY=gd`, `APP_VER=1.00` base, not the `gde` PKG 002 that patch 003 targeted. Install it through GoldHEN/Debug Package Installer, then launch **PS4 Neural Engine Installer** once.

The installer first validates `/app0/pne-release.ini` (schema, release identity, PRX filename and SHA-256 of `/app0/ps4_neural_engine.prx`) before creating the GoldHEN destination directory or changing persistent plugin/configuration files. The SHA-256 implementation is included in the installer and covered by host test vectors.

The installer:
1. Copies `/app0/ps4_neural_engine.prx` to `/data/GoldHEN/plugins/ps4_neural_engine.prx` using a temporary file.
2. Verifies the copied PRX by reading both files back and comparing every byte; it does not trust PS4 `stat`/`fstat` size metadata. Interrupted reads (`EINTR`) are retried, and short reads are accumulated independently on each stream before byte comparison.
3. Creates or updates `/data/GoldHEN/plugins.ini`, preserving existing content and adding the PRX path under the exact `[default]` section.
4. Writes `/data/GoldHEN/ps4_neural_engine_install.log` with the result for FTP diagnostics. This file can distinguish a successful installer exit from a failure before the PRX copy or loader registration.

After a successful install, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.


For troubleshooting, the installer log records the failing operation and the platform `errno` value (for example, source lookup/open, temporary destination creation, write, verification, or final rename). Capture the log after the latest launch attempt; older log contents do not describe a newer attempt.

## Release 006 — status da auditoria estática (20/11/2026)

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

Fonte canônica esperada no repositório: `assets/neural-rendering-icon-source.png`. `tools/make_icon.py` falha explicitamente se essa imagem não estiver presente; não produz fallback. O arquivo aprovado foi localizado na Library do projeto como `Medusa Neural Rendering PS4 Pro.png` (1254×1254), mas a integração ainda depende de disponibilizar seus bytes originais no repositório. Nenhum novo PKG com ícone substituto pode ser aprovado.
