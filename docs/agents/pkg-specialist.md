# PS4 PKG Specialist — Patch Compatibility Audit

## Operating contract
- Distinguish base applications from patch packages. Never label a base PKG as an update.
- Never ask the user to install a package until its patch relationship, metadata, structure, and validation are proven.
- Do not touch PS4 application data, savedata, the canonical PRX, or plugins.ini during package-format investigation.
- QA reports findings; engineering changes the build; QA independently validates the result.

## Base package under audit (2026-11-19)
Source artifact: build run 44623808730, artifact 14578192913.
- File: `PS4-Neural-Engine-002.pkg`
- Size: 6,619,136 bytes.
- PkgTool.Core `pkg_validate --verbose`: all reported hashes/signatures passed.
- `pkg_makegp4` successfully extracted the package and generated `Project.gp4`.
- GP4 volume type: `pkg_ps4_app`.
- GP4 package storage type: `digital50`.
- GP4 app_type: `full`.
- PARAM.SFO: TITLE_ID `PNEU00001`, CONTENT_ID `IV0000-PNEU00001_00-PS4NEURALENG0000`, CATEGORY `gde`, APP_VER `1.00`, VERSION `1.00`, FORMAT `obs`.
- Therefore build 002 is conclusively a base application package, not a patch. It must not be presented as an update installer.

## Patch-generation findings
- The established patch project flow requires `pkg_ps4_patch` and an explicit association to the base application package. Fake PKG Tools documents this in Project Settings > Patch > Application Package; the command-line GP4 generator exposes an `--app_path` option for patch projects.
- The OpenOrbis PkgTool.Core CLI supports `pkg_build`, `pkg_makegp4`, and validation, but its documented CLI does not expose an Application Package association or a patch-project creation command. Do not assume changing only `volume_type` is sufficient.
- A current independent implementation, OrbisPkgTool, documents `gp4gen --patch` and `pkg_ps4_patch` generation. Its own README explicitly says packages have not been tested on jailbroken PS4 hardware. Treat it as an experimental candidate, not a validated installer path.
- A non-game mini-app patch category is expected to differ from the game patch category. Do not blindly copy `gp` from game examples; verify category compatibility against the base `gde` application and a known-good patch sample before building.
- Do not invent the patch CONTENT_ID, APP_VER, VERSION, storage_type, or base-app reference. These must be derived from a documented tool output or a known-good non-game patch.

## Current safe state
- PRX 005 exists at its unique path but is not registered.
- No package has been installed as a result of this audit.
- No plugins.ini, canonical PRX, application data, or saves were modified.

## Next engineering gates
1. Produce a patch GP4 with an explicit base-application association using a tool that supports the full patch project metadata.
2. Verify its PARAM.SFO category and version relationship against the base package.
3. Build a numbered candidate PKG and run structural validation plus independent metadata inspection.
4. Reject it if the tool cannot prove the base-package association.
5. Only after PC-side validation, plan a controlled PS4 installation test that preserves user data and does not replace the existing app or saves.

## Experimental GP4 audit result (2026-11-19, run 44631789567)
- OrbisPkgTool CLI compiled successfully (61 warnings, 0 errors); the base PKG artifact downloaded with matching SHA-256.
- The PKG extracted successfully: 14/14 files.
- `gp4gen --patch` completed, but its output is **not a valid update candidate**:
  - `volume_type=pkg_ps4_patch`
  - `content_id=""`
  - `app_type=full`, `version=01.00`, `app_version=01.00`
  - It enumerated the base application's own 14 files, including its `eboot.bin`, PRX and base metadata.
- Source inspection of OrbisPkgTool `Gp4Project.cs` confirms the project model has no Application Package / `app_path` association field. Its `gp4gen --patch` flag changes the volume type and does not establish the base-package relationship.
- Therefore the audit workflow passing means only that the experimental generator ran; it does **not** mean a patch was generated. Reject this GP4 and do not build or install it.
- Host tests on the same commit passed (run 44631789523).
- Supporting references: Fake PKG Tools' documented Patch tab > Application Package and the `orbis-pub-cmd gp4_proj_create --app_path` flow are described in the external references linked in the engineering investigation. The required builder path must expose equivalent functionality.


## Non-game patch metadata and isolated toolchain probe (2026-11-19)
- PS4DevWiki's PARAM.SFO category table maps `gde` to a non-game mini app and `gpe` to a non-game mini-app patch. This is stronger platform-specific evidence than extrapolating the game-only `gp` category. It remains necessary to verify the generated PARAM.SFO and GP4 against the patch builder output; do not hand-edit metadata and call it validated.
- A disposable GitHub-hosted Windows workflow was added at `.github/workflows/ps4-isolated-patch-toolchain-probe.yml`. It downloads the pinned CyB1K Fake PKG Tools v7 release asset (6,674,278 bytes), extracts it only on an ephemeral hosted runner, records SHA-256 hashes and tool versions, and probes the CLI/patch-generator usage. It does not run on the self-hosted PS4 bridge and has no PS4 network access or deployment secrets.
- Probe workflow run: 44633179181. Awaiting completion and evidence review.
- The release asset is a third-party precompiled archive; a computed hash is provenance evidence for this run, not an independent authenticity guarantee. Do not copy these binaries to the PS4 bridge. Any subsequent PKG build remains an isolated experiment until its patch association, category, version, and structure are independently inspected.


### Isolated Fake PKG Tools probe result (2026-11-19, run 44633288784)
- Hosted Windows runner completed successfully; artifact `ps4-patch-toolchain-probe-evidence` (artifact ID `14583107349`) retained the evidence.
- Pinned release asset size: 6,674,278 bytes; SHA-256 `206FF9CD1199AE3943944FE61FB9247EC0B398B079AB125543FB1AD115E76ED7`.
- Extracted tools:
  - `gengp4_patch.exe` v1.4.2.23, 674,304 bytes, SHA-256 `65042F9982E6DB4C8610E8E735CA296E685518917CB71E7DB42451B5069846E6`.
  - `orbis-pub-cmd.exe`, 3,274,784 bytes, SHA-256 `9D136A27A0739555329E46B61208B7105D4C5AEA5644CBF8052BFED4250D0E1B`.
  - `orbis-pub-gen.exe`, 116,768 bytes, SHA-256 `D9A1A80D79188066DE5D0C893F76823C44F34F2C5D2426435BDF76A4DA0D1FA5`.
- `orbis-pub-cmd gp4_proj_create --help` returns a concrete parser response: the command requires `--volume_type`; the next probe supplies `pkg_ps4_patch` to reveal the remaining required arguments and test the documented `--app_path` association.
- Running `gengp4_patch.exe` with no arguments did not return within 15 seconds and was terminated by the harness. This is not evidence of failure; it may be GUI-driven. The next probe tests help switches and extracts relevant ASCII strings without invoking extra commands.
- Important: these hashes establish what the hosted runner downloaded and inspected, not independent publisher authenticity. No binaries were copied to the self-hosted bridge or PS4. No package was built or installed.
- Follow-up probe workflow run: 44633403669.


### Real base-package patch association proven (2026-11-19, run 44633531153)
- Downloaded the actual PKG 002 artifact into a disposable GitHub-hosted Windows runner and ran the pinned `orbis-pub-cmd.exe` against it.
- Command completed with exit code 0 using `gp4_proj_create --volume_type pkg_ps4_patch --content_id IV0000-PNEU00001_00-PS4NEURALENG0000 --passcode 00000000000000000000000000000000 --entitlement_key 00000000000000000000000000000000 --storage_type digital25 --app_path <actual-base.pkg> <output.gp4>`.
- Generated GP4 (810 bytes) contains `<volume_type>pkg_ps4_patch</volume_type>` and a package node with `app_path="base-artifact\\pkg\\PS4-Neural-Engine-002.pkg"`, proving the project is explicitly associated with the real base package rather than merely changing the volume type.
- Evidence artifact ID: `14581593366`; workflow run: `44633531153`.
- This is a successful association-only GP4, not yet a complete patch project: its `<files img_no="0"/>` and `<rootdir/>` are empty. No PKG was built or installed.
- PS4 PARAM.SFO reference table explicitly maps `gde` to “Non-Game Mini App” and `gpe` to “Non-Game Mini App Patch”. The next step is to stage the changed payload, set patch SFO metadata, add files to this associated GP4, and audit the complete project before any PKG build.
- The generator's `gengp4_patch.exe --help` is not a usable CLI help switch (it treats `--help` as a directory); no-argument and `/?` invocations can open/block, so the workflow bounds and terminates them. Use the command-line `orbis-pub-cmd` route for reproducible automation.


### Patch build blocker identified (2026-11-19, run 44634035504)
- The associated GP4 is valid and contains four planned payload entries (eboot, PRX, PARAM.SFO, changeinfo) after the next workflow revision; the initial package-build attempt reached `img_create` but failed with: `Volume Type/Storage Type ... (app:unknown, patch:digital25)`.
- The base PKG header is `content_type=0x1A` (PS4 game-data/application base) and its SFO is `CATEGORY=gde`, `APP_VER=01.00`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`. The OrbisPkgTool viewer's label `Addon` is not authoritative here: its classifier only recognizes `gd/gp/ac` and falls back to Addon for `gde`; it does not change the header's 0x1A application type.
- The failure indicates Fake PKG Tools' publisher generator cannot currently resolve the base/patch storage-type pairing from this base package. Its SFO `PUBTOOLINFO` lacks an explicit `st_type` token even though the original GP4 was `storage_type=digital50`. This is the leading hypothesis, not yet proven.
- The patch format requires `sce_sys/changeinfo/changeinfo.xml`; the initial candidate omitted it and the generator warned that the package would not be importable into GEMS. The follow-up adds a valid changeinfo file and probes `digital50` rather than `digital25`.
- Follow-up run: 44634453533. Do not install any candidate from these failed builds. No PKG was produced, and the PS4 remains untouched.


### Category-pair diagnostic rejected (2026-11-19, run 44636671853)
- The isolated hosted Windows run successfully built OrbisPkgTool, downloaded the exact PKG 002 artifact, verified and extracted the pinned Fake PKG Tools v7 archive, and generated an associated patch GP4 with four payload entries.
- The run then failed at Sony's img_create with the exact error: `Format of the param file is not valid. (param.sfo(in app pkg))`. No patch PKG was produced.
- Crucial QA finding: this run is not evidence that the real gde -> gpe pairing works or fails. The workflow attempted to create a synthetic gd diagnostic base, but its locally modified OrbisPkgTool builder maps base-app categories to gde; the read-back of the synthetic base SFO confirms CATEGORY = gde, not gd. The test then attempted a game-style patch SFO (gp) and subsequently rewrote it to gpe. This mixed diagnostic path is invalid and must not be used to infer compatibility.
- The real base PKG 002 remains CATEGORY=gde, APP_VER=1.00, CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000. The GP4 association command did accept the real base path in the earlier association-only probe, but a complete package build against that real base has not passed.
- The workflow also contains a misleading report line that labels the real base path even though the GP4 command in this diagnostic run used the synthetic base path. Do not treat that line as association evidence.
- Artifact 14584861239 contains only the diagnostic report/SFO evidence; it is not an installable PKG.
- Host tests passed (run 44636671798). Package build failed (run 44636671853). No package was installed and the PS4 was not contacted.
- Engineering action: retire this mixed game/non-game diagnostic. The next experiment must use the exact original PKG 002 as --app_path, preserve the base category, use only the hypothesized non-game patch category gpe, and test the publisher metadata as an isolated variable. If the tool rejects the original base SFO, stop and investigate the required official metadata rather than rewriting categories or fabricating a base package.


### Engineering correction: use the real base package (2026-11-19)
- Log review of run 44634453533 showed `orbis-pub-cmd` rejected `--storage_type digital50` as an invalid CLI value. The patch builder's accepted probe value remains `digital25`; do not pass `digital50` to this command.
- Review of workflow 44636671853 confirmed that the test built and associated a synthetic diagnostic base instead of the real PKG 002. Its `param.sfo` was deliberately removed before building that synthetic base, making the subsequent `param.sfo(in app pkg)` failure a contaminated diagnostic, not proof that the real base is incompatible.
- Engineering commit `5522336d1440ba6d1492f4a0d738a6f6662b71dc` removes the synthetic-base/game-category experiment. The candidate workflow now uses the exact original PKG 002 as `--app_path`, retains the real base's `gde` metadata untouched, changes only the candidate patch SFO to the hypothesized `gpe` category, and verifies that the GP4 association names `PS4-Neural-Engine-002.pkg`.
- New build run 44637080299 and host-test run 44637080302 were queued against commit 5522336d. Await their logs and independent QA. This is an engineering correction, not a successful PKG build; no candidate has been approved or installed.


### Exact-base build result and next diagnostic (2026-11-19, run 44637307373)
- The workflow used the exact original PKG 002 as `--app_path`; its package inspection reported Title ID `PNEU00001`, Content ID `IV0000-PNEU00001_00-PS4NEURALENG0000`, header content type `0x1A`, and SFO category `gde`. The patch SFO readback correctly preserved `TITLE_ID`/Content ID, set `CATEGORY=gpe`, `APP_VER=01.01`, and integer `ATTRIBUTE=0x00800002`.
- The generated GP4 had `volume_type=pkg_ps4_patch`, four payload entries, and an explicit `app_path` ending in `PS4-Neural-Engine-002.pkg`. Thus the previous false-negative caused by a synthetic base and the malformed ATTRIBUTE have been removed.
- Sony `img_create` still failed: `The following combination of Volume Type/Storage Type is not allowed or not supported. (app:unknown, patch:digital25)`. No PKG candidate was created. The report artifact is diagnostic only (artifact ID `14585026226`).
- PS4DevWiki's SFO category table confirms `gde` = Non-Game Mini App and `gpe` = Non-Game Mini App Patch. The category mapping is therefore grounded; do not switch to game categories `gd/gp`.
- Leading issue to isolate now: the publisher tool is not resolving the base package's storage type while consuming the GP4's relative `app_path`. Engineering changed the workflow to pass the fully resolved absolute base-PKG path and record it in the report (commit `516769b80a973240471fc21a75c694f52d1e5dbc`). The first run triggered by the intermediate commit is 44637565124; the final absolute-path commit triggers a follow-up run. This is a targeted diagnostic, not a claimed fix.
- No PKG was installed, no PS4 connection was made, and no saves, app data, canonical PRX, or `plugins.ini` were modified.


### Root cause isolated: base storage type absent from PARAM.SFO (2026-11-19)
- The exact-base run 44640499534 again reached Sony `img_create` and failed with `(app:unknown, patch:digital25)`. Its report explicitly records the original base SFO's `PUBTOOLINFO` without any `st_type=` token.
- This is not a generic category-classification problem. The PS4 PARAM.SFO reference documents `st_type` as a PUBTOOLINFO storage-type field, with `digital50` and `digital25` as distinct values. A public PS4 package utility independently determines a package's storage type by searching its PUBTOOLINFO string for those tokens. Therefore the base GP4's `storage_type=digital50` does not guarantee the built PKG carries the metadata needed by Fake PKG Tools' base-package resolver.
- The original `tools/build_pkg.sh` creates CATEGORY `gde` but never explicitly writes PUBTOOLINFO. The built base package consequently lacks `st_type=digital50`, which explains the resolver's `app:unknown` result more directly than the earlier OrbisPkgTool viewer label.
- Engineering commit `2707fd4affadeb3f1bda0d2310d02f28d68af3ff` adds an explicit 512-byte PUBTOOLINFO value containing `st_type=digital50` to the base SFO generation step. This is a targeted base-package metadata correction, not yet validated.
- Engineering commit `c3c1fd2a6f158be3b21c66e3e1c230aa85ca9023` adds a patch-build preflight that rejects a base package whose extracted PUBTOOLINFO has no `st_type`, preventing another pointless `app:unknown` build.
- Next gate: inspect the new base PKG 002 artifact after run 44641114863. Confirm the extracted SFO contains `st_type=digital50`, and that Sony package validation and independent PKG inspection still pass. Only then update the patch workflow's pinned base-artifact run ID and attempt patch generation again.
- No existing PS4 installation, saves, application data, canonical PRX, or `plugins.ini` was touched.
