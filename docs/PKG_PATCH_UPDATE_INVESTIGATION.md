# PS4 application patch investigation

**Status:** research completed to the point of identifying the correct package family and metadata requirements; no patch PKG has been generated or represented as installable.  
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
- The currently pinned `PkgTool.Core` workflow has been used to build and validate the base package. No verified evidence has yet been established that this pinned version can generate and correctly associate this application's patch package end-to-end.
- OrbisPkgTool has a `gp4gen --patch` option and represents `pkg_ps4_patch` in its GP4 model. However, its current `FromFolder(..., isPatch: true)` implementation only selects the patch volume type; its defaults remain `storage_type=digital50`, `app_type=full`, and it does not convert the embedded SFO category from `gde` to `gpe`. Its README also labels package generation experimental and says hardware installation has not been tested. It is therefore a research/reference tool, not a drop-in trusted patch builder for this project.
- The upstream patch-GP4 route and metadata must be verified against a known-good patch project or a compatible publishing implementation before building the candidate. Do not silently adapt an app GP4 and assume the result is valid.

## 3. Required inputs before a safe build

1. The exact base PKG corresponding to the version actually installed on the PS4, plus its SFO and GP4/build provenance. The latest known base candidate in the repository is release 011 (APP_VER 01.07), but the user did not install it; the installed version must not be guessed.
2. A patch-capable builder path that supports `pkg_ps4_patch`, the correct non-game patch SFO category (`gpe`), `digital25`, the same application identity/passcode, and explicit association to that exact base.
3. A version rule verified against the target base and package builder. The patch's `APP_VER` must be accepted as an update to the installed base; do not assume that incrementing it alone suffices.
4. Independent QA that reads the final PKG and GP4, verifies package type is patch (not app/base), confirms SFO values, content/title identity, base association, package hashes/signatures and extracted eboot/payload hashes.
5. Only after PC QA passes, stage the patch separately. Installation on the console remains a separate user-authorized operation. Never uninstall the base app as a workaround.

## 4. Fail-closed acceptance checklist

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

## 5. Current decision

Continue engineering the genuine patch path, but do not create a speculative PKG by changing a few metadata strings. The hybrid PRX installer remains a single full/base PKG for initial installation and plugin updates. A PS4 Package Manager update of the installer app is a separate patch-package operation.
