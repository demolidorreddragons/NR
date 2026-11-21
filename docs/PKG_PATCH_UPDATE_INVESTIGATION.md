# PS4 application patch investigation

**Status:** patch build pipeline implemented; CI execution and independent QA pending. No patch PKG has yet been validated or represented as installable.  
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
- The repository now generates a patch GP4 from the 011 update payload, sets `pkg_ps4_patch` and `digital25`, and builds it with LibOrbisPkg's DP/subsequent-patch header path. The independent QA job checks package type, `CATEGORY=gpe`, `APP_VER=01.07`, base identity/version and payload digests. These checks have not yet executed successfully.

## 3. Implemented build path and required validation

- `tools/build_patch_pkg.sh` builds release 011 as a patch against the preserved release 010 PKG. It refuses a base PKG whose SHA-256 differs from the known installed base and refuses a patch version that is not newer than 01.06.
- `.github/workflows/ps4-prx.yml` now runs `build-patch` only after the current base build and independent base QA pass, then runs `independent-patch-qa` on Windows. The patch is not staged to the PS4 and no install action exists in this pipeline.
- The pipeline is pending execution. A workflow being present is not evidence that a patch package was generated or validated.

## 4. Required validation before safe use

1. The exact installed base is release 010 (`APP_VER=01.06`), confirmed by the prior project deployment history. Its preserved build artifact is workflow run `44851147057`, artifact `ps4-neural-engine-build`, PKG SHA-256 `015c9e2c266561fda005c15766fe4833412c658d9a8d0b7334fd2cfe3fec8ae2` (6,619,136 bytes). Release 011 was not installed.
2. A patch-capable builder path that supports `pkg_ps4_patch`, the correct non-game patch SFO category (`gpe`), `digital25`, the same application identity/passcode, and explicit association to that exact base.
3. A version rule verified against the target base and package builder. The patch's `APP_VER` must be accepted as an update to the installed base; do not assume that incrementing it alone suffices.
4. Independent QA that reads the final PKG and GP4, verifies package type is patch (not app/base), confirms SFO values, content/title identity, base association, package hashes/signatures and extracted eboot/payload hashes.
5. Only after PC QA passes, stage the patch separately. Installation on the console remains a separate user-authorized operation. Never uninstall the base app as a workaround.

## 5. Fail-closed acceptance checklist

A candidate is **not ready** unless all checks pass:

- [ ] Base package matches the app version installed on the PS4.
- [ ] GP4 volume type is exactly `pkg_ps4_patch`.
- [ ] GP4 package storage type and patch association are verified against a known-good patch project.
- [ ] PARAM.SFO `CATEGORY=gpe` for this non-game mini-app family.
- [ ] `TITLE_ID` and `CONTENT_ID` match the intended installed base identity.
- [ ] Patch version relationship is verified and newer than the installed app version.
- [ ] Builder output is independently identified as a patch, not a full/base application.
- [ ] Package validation and extraction pass; embedded SFO and all payload hashes match the audited inputs.
- [ ] No installation, uninstallation, PRX activation, or reboot is performed by build/QA automation.

## 6. Current decision

Continue validating the implemented patch path in CI. The hybrid PRX installer remains a single full/base PKG for initial installation and plugin updates. A PS4 Package Manager update of the installer app is a separate patch-package operation. Even if PC QA passes, the candidate must not be installed without the user's explicit authorization; no uninstall workaround is permitted.
