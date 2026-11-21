# PS4 application patch investigation

**Status:** pipeline de patch associado à base Sony implementado no workflow; execução do novo pipeline e QA independente pendentes. Nenhum patch foi aprovado para instalação.  
**Target application:** Neural Rendering installer, `TITLE_ID=PNEU00001`, base `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`.  
**Installed application:** already present on the user's PS4. Release 011 was rejected with `Package already installed`; it is a full/base `gde` package, not a patch. Do not uninstall the installed app.

## 1. Confirmed package distinction

- The current build script explicitly writes `CATEGORY=gde` and creates an app GP4 with OpenOrbis `create-gp4`. This is a full/base application.
- PS4 PARAM.SFO category references distinguish `gde` (non-game mini application) from `gpe` (non-game mini-application patch). `gp` is the game-application patch category and is not the matching category for this app family.
- A patch GP4 uses `<volume_type>pkg_ps4_patch</volume_type>`; the commonly documented patch storage type is `digital25`, versus `digital50` for the base application.
- A patch must be associated with the exact compatible base package and preserve the title/content identity and package passcode relationship. Merely changing `APP_VER`, renaming a PKG, or changing only the GP4 volume type is not sufficient.

Sources reviewed:
- PS4 Developer Wiki, PARAM.SFO CATEGORY reference: https://www.psdevwiki.com/ps4/Param.sfo
- OpenOrbis toolchain documentation: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- OrbisPkgTool GP4 generator source: https://github.com/pearlxcore/OrbisPkgTool/blob/master/OrbisPkgTool/Gp4/Gp4Project.cs
- OrbisPkgTool documents its output as experimental and explicitly says installation/launch on real PS4 hardware has not been tested: https://github.com/pearlxcore/OrbisPkgTool

## 2. Tooling findings

- The repository's pinned OpenOrbis `create-gp4` invocation creates the app-style GP4 used by the current full/base build. Its documented interface accepts a content ID and file list; it does not expose a patch/base association option.
- The pinned OpenOrbis LibOrbisPkg source (`92a4e7f8e5914cb06b52ba3ae6b0675da438a051`) explicitly maps GP4 `pkg_ps4_patch` to package content type `DP` and `SUBSEQUENT_PATCH` flags. The patch workflow builds this pinned source with `PUBTOOLINFO st_type=digital25`, while preserving the patch SFO's `CATEGORY=gpe`. The GP4 uses the exact base CONTENT_ID and all-zero passcode used by release 010. This is a source-level and metadata path; package generation and independent QA still must pass.
- OrbisPkgTool has a `gp4gen --patch` option and represents `pkg_ps4_patch` in its GP4 model. However, its current `FromFolder(..., isPatch: true)` implementation only selects the patch volume type; its defaults remain `storage_type=digital50`, `app_type=full`, and it does not convert the embedded SFO category from `gde` to `gpe`. Its README also labels package generation experimental and says hardware installation has not been tested. It is therefore a research/reference tool, not a drop-in trusted patch builder for this project.
- O workflow `ps4-prx.yml` agora segue o fluxo Sony associado: reconstrói uma cópia descartável da base 010 com `CATEGORY=gde`, `APP_VER=01.00` (normalização exigida pelo publisher Sony), `VERSION=01.06` e `st_type=digital50`; cria GP4 de patch com `volume_type=pkg_ps4_patch`, `storage_type=digital25` e `app_path` apontando para essa base reconstruída; cria SFO patch `CATEGORY=gpe`, `APP_VER=01.07` e `VERSION=01.07` no patch final; a base de associação mantém `VERSION=01.06`, `VERSION=01.06` (versão da base); adiciona eboot 011, PRX, manifesto e changeinfo; gera o PKG com `orbis-pub-cmd img_create`. O pacote original 010 é preservado sem modificação. O novo job de QA deve validar separadamente a base associada, GP4, SFO, conteúdo e hashes.

## 3. Implemented build path and required validation

- O job `build-associated-patch-against-installed-010` roda em runner Windows descartável, baixa o PKG 010 pelo SHA-256 conhecido e o PKG 011 do mesmo run, extrai/reorganiza a base e reconstrói somente uma cópia de associação. A reconstrução não é o pacote instalado e não altera o PS4.
- A associação Sony é explícita por `--app_path` no GP4 do patch. A versão 011 inclui o novo `eboot.bin` (mudança de instalador), o PRX, o manifesto e `sce_sys/changeinfo/changeinfo.xml`.
- `independent-patch-qa` deve verificar a associação GP4, o pacote-base reconstruído, o original 010, a identidade SFO e os hashes dos arquivos extraídos. O workflow não faz staging, instalação, ativação nem reboot.
- O novo pipeline está pendente. A implementação no repositório não prova que o PS4 aceitará o pacote.

## 4. Required validation before safe use

1. The exact installed base is release 010 (`APP_VER=01.06`), confirmed by the prior project deployment history. Its preserved build artifact is workflow run `44851147057`, artifact `ps4-neural-engine-build`, PKG SHA-256 `015c9e2c266561fda005c15766fe4833412c658d9a8d0b7334fd2cfe3fec8ae2` (6,619,136 bytes). Release 011 was not installed.
2. Build output with `pkg_ps4_patch`, non-game patch SFO category (`gpe`), `digital25`, matching identity/passcode, and explicit `app_path` association to a Sony-rebuilt copy of the exact 010 base.
3. A version rule verified against the target base and package builder. The patch's `APP_VER=01.07` must be accepted as newer than installed 010 (`APP_VER=01.06`), while `VERSION=01.06` remains the app version; do not assume that incrementing it alone suffices.
4. Independent QA that reads the final PKG and GP4, verifies package type is patch (not app/base), confirms SFO values, content/title identity, base association, package hashes/signatures and extracted eboot/payload hashes.
5. Only after PC QA passes, stage the patch separately. Installation on the console remains a separate user-authorized operation. Never uninstall the base app as a workaround.

## 5. Fail-closed acceptance checklist

A candidate is **not ready** unless all checks pass:

- [ ] Base package matches the app version installed on the PS4.
- [ ] GP4 volume type is exactly `pkg_ps4_patch`.
- [ ] GP4 package storage type is `digital25` and `app_path` points to the audited Sony-rebuilt base derived from exact 010.
- [ ] PARAM.SFO `CATEGORY=gpe` for this non-game mini-app family.
- [ ] `TITLE_ID` and `CONTENT_ID` match the intended installed base identity.
- [ ] Patch version relationship is verified and newer than the installed app version.
- [ ] Builder output is structurally validated as a patch and independently extracted; GP4 volume and base association both agree.
- [ ] Package validation and extraction pass; embedded SFO and all payload hashes match the audited inputs.
- [ ] No installation, uninstallation, PRX activation, or reboot is performed by build/QA automation.

## 6. Current decision

Continue validating the implemented patch path in CI. The hybrid PRX installer remains a single full/base PKG for initial installation and plugin updates. A PS4 Package Manager update of the installer app is a separate patch-package operation. Even if PC QA passes, the candidate must not be installed without the user's explicit authorization; no uninstall workaround is permitted.
