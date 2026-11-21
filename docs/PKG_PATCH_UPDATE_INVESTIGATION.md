# PS4 application patch investigation

**Status:** patch 010→011 gerado e aprovado em QA estrutural independente PC-only no workflow run `44856253337` (commit `dcbe06dbacdcb1551702e260f7097d5d6d230f43`). PKG pronto como candidato; ainda não instalado nem testado no PS4. Não alegar compatibilidade de instalação confirmada até teste no console.  
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
- The pinned OpenOrbis LibOrbisPkg source (`92a4e7f8e5914cb06b52ba3ae6b0675da438a051`) explicitly maps GP4 `pkg_ps4_patch` to package content type `DP` and `SUBSEQUENT_PATCH` flags. The patch workflow builds this pinned source with `PUBTOOLINFO st_type=digital25`, preserving patch SFO `CATEGORY=gpe`. The GP4 uses the exact base CONTENT_ID and all-zero passcode used by release 010. Package generation and independent QA passed on PC; console acceptance remains untested.
- OrbisPkgTool has a `gp4gen --patch` option and represents `pkg_ps4_patch` in its GP4 model. However, its current `FromFolder(..., isPatch: true)` implementation only selects the patch volume type; its defaults remain `storage_type=digital50`, `app_type=full`, and it does not convert the embedded SFO category from `gde` to `gpe`. Its README also labels package generation experimental and says hardware installation has not been tested. It is therefore a research/reference tool, not a drop-in trusted patch builder for this project.
- O workflow `ps4-prx.yml` reconstrói uma cópia descartável da base 010 com `CATEGORY=gde`, `APP_VER=01.00`, `VERSION=01.06` e `st_type=digital50`; cria GP4 de patch com `volume_type=pkg_ps4_patch`, `storage_type=digital25` e `app_path` apontando para essa base reconstruída; cria SFO patch `CATEGORY=gpe`, `APP_VER=01.07` e `VERSION=01.07`; adiciona eboot 011, PRX, manifesto e changeinfo; gera o PKG com `orbis-pub-cmd img_create`. O pacote original 010 é preservado sem modificação. O build e a QA independente passaram no run `44856253337`.

## 3. Implemented build path and required validation

- O job `build-associated-patch-against-installed-010` roda em runner Windows descartável, baixa o PKG 010 pelo SHA-256 conhecido e o PKG 011 do mesmo run, extrai/reorganiza a base e reconstrói somente uma cópia de associação. A reconstrução não é o pacote instalado e não altera o PS4.
- A associação Sony é explícita por `--app_path` no GP4 do patch. A versão 011 inclui o novo `eboot.bin` (mudança de instalador), o PRX, o manifesto e `sce_sys/changeinfo/changeinfo.xml`.
- `independent-patch-qa` verificou associação GP4, pacote-base reconstruído, original 010, identidade SFO e hashes dos arquivos extraídos. A QA passou PC-only. O workflow não faz staging, instalação, ativação nem reboot.
- O pipeline concluiu com sucesso: `build-prx-and-pkg`, `independent-pkg-qa`, `build-associated-patch-against-installed-010` e `independent-patch-qa` PASS. Isso comprova estrutura/identidade/conteúdo no PC, não a aceitação pelo Package Manager do PS4.

## 4. Required validation before safe use

1. The exact installed base is release 010 (`APP_VER=01.06`), confirmed by the prior project deployment history. Its preserved build artifact is workflow run `44851147057`, artifact `ps4-neural-engine-build`, PKG SHA-256 `015c9e2c266561fda005c15766fe4833412c658d9a8d0b7334fd2cfe3fec8ae2` (6,619,136 bytes). Release 011 was not installed.
2. Build output with `pkg_ps4_patch`, non-game patch SFO category (`gpe`), `digital25`, matching identity/passcode, and explicit `app_path` association to a Sony-rebuilt copy of the exact 010 base.
3. A versão do patch é `APP_VER=01.07` e `VERSION=01.07`; a base 010 original é `APP_VER=01.06` e `VERSION=01.06`. A base sintética de associação usa `APP_VER=01.00`, `VERSION=01.06`, preservando a identidade e a versão-base para o publisher.
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
- [x] Package validation and extraction pass; embedded SFO and all payload hashes match the audited inputs (PC-only, run `44856253337`).
- [ ] No installation, uninstallation, PRX activation, or reboot is performed by build/QA automation.

## 6. Current decision

The patch 010→011 foi construído e passou QA estrutural independente PC-only. O PKG original 010 e a base instalada não foram alterados. O patch ainda não foi staged nem instalado; a aceitação pelo Package Manager só pode ser confirmada no PS4, com autorização explícita do usuário. O instalador híbrido do plugin continua sendo um único PKG full/base; atualizar o próprio aplicativo pelo Package Manager é operação separada. Não desinstalar a base como workaround.
