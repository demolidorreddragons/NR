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
- Engineering action: retire this mixed game/non-game diagnostic. The next experiment must use the exact original PKG 002 as `--app_path`, preserve the base category, use only the hypothesized non-game patch category gpe, and test the publisher metadata as an isolated variable. If the tool rejects the original base SFO, stop and investigate the required official metadata rather than rewriting categories or fabricating a base package.

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
- The PS4 PARAM.SFO reference documents `st_type` as a PUBTOOLINFO storage-type field, with `digital50` and `digital25` as distinct values. A public PS4 package utility independently determines a package's storage type by searching its PUBTOOLINFO string for those tokens. Therefore the base GP4's `storage_type=digital50` does not guarantee the built PKG carries the metadata needed by Fake PKG Tools' base-package resolver.
- The original `tools/build_pkg.sh` creates CATEGORY `gde` but never explicitly writes PUBTOOLINFO. The built base package consequently lacks `st_type=digital50`, which explains the resolver's `app:unknown` result more directly than the earlier OrbisPkgTool viewer label.
- Engineering commit `2707fd4affadeb3f1bda0d2310d02f28d68af3ff` adds an explicit 512-byte PUBTOOLINFO value containing `st_type=digital50` to the base SFO generation step. This is a targeted base-package metadata correction, not yet validated.
- Engineering commit `c3c1fd2a6f158be3b21c66e3e1c230aa85ca9023` adds a patch-build preflight that rejects a base package whose extracted PUBTOOLINFO has no `st_type`, preventing another pointless `app:unknown` build.
- Next gate: inspect the new base PKG 002 artifact after run 44641114863. Confirm the extracted SFO contains `st_type=digital50`, and that Sony package validation and independent PKG inspection still pass. Only then update the patch workflow's pinned base-artifact run ID and attempt patch generation again.
- No existing PS4 installation, saves, application data, canonical PRX, or `plugins.ini` was touched.

### Correction: the immediate failure was a stale pinned base artifact
- The base build run `44641114863` succeeded after commit `2707fd4`. Its build log confirms `PUBTOOLINFO = c_date=20261119,...,st_type=digital50` before package creation, and PKG hash/signature validation passed. Artifact ID: `14585159593`.
- The first patch attempt after that change still downloaded the older artifact from run `44623808730`. Its extracted SFO correctly lacked `st_type`, so the new preflight failed as designed. The base-generation change itself was present and working; the patch workflow's pinned artifact reference was stale.
- Commit `41d5b4a4b0172ef7664ab287a759067211c6be4a` updates the patch workflow to download base PKG 002 from run `44641114863`. Its associated patch build is run `44641439173`; inspect its final logs and artifact before making any further metadata changes.
- Do not infer success from the preflight alone. The required next evidence is that the new base SFO readback contains `st_type=digital50`, then Fake PKG Tools accepts the base/patch storage pairing and Sony `img_create` completes, followed by independent validation and readback.

### Engineering root-cause correction: PkgTool.Core overwrites PUBTOOLINFO during package build (2026-11-19)
- Patch run `44641439173` completed with failure; host tests run `44641439250` passed.
- The patch workflow downloaded the rebuilt artifact from run `44641114863` successfully, extracted the exact PKG 002, and read its embedded `Sc0/param.sfo`. The embedded value is `PUBTOOLINFO = c_date=20261119,img0_l0_size=7,img0_l1_size=0,img0_sc_ksize=512,img0_pc_ksize=832` — no `st_type`. The preflight correctly stopped before patch GP4 generation.
- This disproves the prior claim that the new PKG 002 artifact contained the storage-type field. The base build log only inspected the staged SFO **before** `PkgTool.Core pkg_build`; it did not inspect the final PKG's embedded SFO. The successful PKG hash/signature validation did not validate the intended custom SFO metadata.
- Source inspection of the LibOrbisPkg `PkgBuilder` implementation shows why: after reading the staged `sce_sys/param.sfo`, the builder unconditionally assigns a new `PUBTOOLINFO` value composed from the date and image-size fields, replacing the entire previous value. Thus `st_type=digital50` is discarded during `pkg_build`. Reference: https://github.com/maxton/LibOrbisPkg/blob/master/LibOrbisPkg/PKG/PkgBuilder.cs
- Do not patch the SFO before `pkg_build` and assume it survives. The next engineering route must use a builder that preserves the field, or a controlled source-level change to the package builder followed by full rebuild and final-PKG SFO readback. Do not hand-edit the built PKG's SFO without rebuilding all dependent PKG hashes/signatures.
- The candidate artifact from run `44641439173` is a 1,318-byte diagnostic report only; it contains no PKG. It is not installable.
- No PKG was installed, the PS4 was not contacted, and PRX activation, plugins.ini, saves, and app data remain untouched.


### Follow-up: exact SFO copy-order defect in LibOrbisPkg (2026-11-20)
- The first source-level correction appended `,st_type=digital50` to the local parsed SFO and compiled successfully, but patch run 44642976292 still extracted PKG 002 with no `st_type`. That candidate was rejected by the preflight; artifact 14586768216 is diagnostic only.
- Further source inspection found the decisive ordering bug in `LibOrbisPkg/PKG/PkgBuilder.cs`: `pkg.ParamSfo = new SfoEntry(sfo)` copied the parsed SFO **before** the subsequent PUBTOOLINFO and PUBTOOLVER assignments. The builder therefore serialized the stale copy even though its log showed the modified local SFO.
- Engineering commits 7dc31f39, 942db1f3, and 625611db refine the build-time patch. The final patch removes the early SFO copy and inserts exactly one `pkg.ParamSfo` construction after both metadata fields are finalized. The workflow script also verifies the copy-order transformation before compiling the tool.
- The next build gate is not merely a successful compiler or a staged-SFO log: extract the completed PKG 002 and confirm its embedded `PUBTOOLINFO` contains `st_type=digital50`. Then rerun the associated non-game patch build against that exact artifact and inspect its PKG, GP4 association, SFO, hashes, and independent validation.
- No patch candidate from runs 44642976292 or earlier is installable. No PS4 installation or PRX activation occurred.

### QA finding: publisher rejects the associated base SFO (2026-11-20, run 44643810774)
- The patch workflow used the rebuilt PKG 002 artifact and independently extracted its Sc0 PARAM.SFO. It contains CATEGORY `gde`, TITLE_ID `PNEU00001`, the expected CONTENT_ID, and `PUBTOOLINFO ... st_type=digital50`. The preflight `BASE_SFO_HAS_STORAGE_TYPE=True` passed.
- The candidate patch SFO readback showed CATEGORY `gpe`, APP_VER `01.01`, ATTRIBUTE `0x00800002`, and the inherited PUBTOOLINFO. The associated GP4 has `volume_type=pkg_ps4_patch`, `storage_type=digital25`, and an `app_path` to PKG 002. Four payload entries were present.
- Despite those checks, Sony `img_create` failed with `Format of the param file is not valid. (param.sfo(in app pkg))`. No candidate PKG was created. Artifact 14586893902 contains only the diagnostic report, SFO text readbacks, and GP4.
- This is not evidence that `gpe` is invalid, nor that changing patch storage type fixed the issue. The message explicitly identifies the PARAM.SFO in the associated application package. The base PKG was produced by LibOrbisPkg/PkgTool.Core, not by Sony's publisher builder; parser readability and the PkgTool validator do not establish that Sony's `img_create` accepts that base SFO/package as an Application Package.
- Next isolated engineering test: rebuild the base app GP4 from the same extracted payload using the pinned Fake PKG Tools publisher builder (`orbis-pub-cmd img_create`), independently inspect its embedded SFO and package header, then use that Sony-built base as `--app_path` for the patch attempt. Keep this isolated on a disposable hosted runner; do not replace PKG 002 or install anything on the PS4.
- The SFO storage-alignment change in commit 567b0bb has host tests passing (run 44643896718), but it has not yet been exercised by a patch-build run using that exact workflow revision. Run 44643810774 executed the earlier workflow revision and still used `digital50` in the patch SFO.

### Sony publisher base-rebuild diagnostic (2026-11-20, run 44644299623)
- The experiment isolated the earlier patch failure one step further: Sony `img_create` did not reach patch creation. It rejected the disposable rebuilt base app at `sce_sys/param.sfo` with `Param VERSION: Invalid value` and `unexpected Application Version (APP_VER)`.
- The extracted, previously validated PKG 002 SFO readback showed `APP_VER = 1.00`, `VERSION = 1.00`, `CATEGORY = gde`, and `PUBTOOLINFO ... st_type=digital50`. The base package itself was not modified; only its temporary extracted working copy was used for the attempted Sony rebuild.
- The Sony rebuild attempt stopped before producing a base PKG. The uploaded evidence artifact 14587718343 contains the report and original base SFO readback only. No patch GP4 or PKG was produced by this run.
- Engineering commit `27f7c1392ff087d5bf313ae17b6fcfb3a357b370` normalizes APP_VER to `01.00` only in the disposable SFO copy before the next Sony base rebuild, then reads it back and asserts the normalized value. This directly targets the publisher's explicit APP_VER diagnostic; it does not alter PKG 002 or establish that the Sony rebuild or patch build will pass.
- The run's `host-tests` passed (44644299610). Next candidate workflow is triggered by the patch-workflow change. Keep the base app, app data, saves, canonical PRX, and PS4 deployment untouched.


### Sony publisher VERSION validation follow-up (2026-11-20)
- Patch candidate run 44644594618 used the real validated PKG 002, confirmed its embedded PUBTOOLINFO contains `st_type=digital50`, and normalized APP_VER from `1.00` to `01.00` in the disposable extracted copy.
- Sony `img_create` then advanced past the prior APP_VER rejection but failed with the explicit next error: `sce_sys/param.sfo: Param VERSION: Invalid value.` No Sony-rebuilt base or patch PKG was produced.
- Engineering commit `8ef7e5daa2e391f35229f984c5fffb440e8ad34f` now normalizes both APP_VER and VERSION to `01.00` only in the temporary extracted base copy and asserts both readbacks before invoking Sony `img_create`. The original PKG 002 artifact remains unchanged.
- The push-trigger fallback now selects the latest validated base artifact from run `44644315395`, rather than stale run `44643472429`.
- This is a targeted response to the observed publisher error, not a proven fix. The next candidate must pass Sony base creation, independent base validation/readback, associated patch creation, independent patch validation, and QA before it can be called PKG 003.


### Sony publisher-managed directory collision (2026-11-20)
- Candidate run 44647038528 used the latest base artifact and confirmed the temporary base SFO readback had both APP_VER and VERSION normalized to 01.00. Sony img_create advanced beyond the earlier VERSION rejection.
- The next explicit publisher error was: `Could not create system directory. (sce_sys/about)`. The restructure log shows that OrbisPkgTool retained the original `Image0/sce_sys/about/right.sprx` and then carried that directory into the Sony GP4. This directly identifies a collision with the publisher-managed `sce_sys/about` directory.
- Engineering commit 470ea26c35c61f03a9e7cce26723d217e261d523 removes only `sce_sys/about` from the disposable extracted copy before creating the Sony base GP4, asserts the directory is absent, and records the exclusion. The original PKG 002 artifact and PS4 state are untouched.
- This is a targeted response to the exact publisher error, not a validated fix. Next gates remain Sony base creation, independent base validation and SFO readback, real patch association, patch build, independent validation, and QA.


### Sony publisher rejects extracted PlayGo metadata (2026-11-20, run 44647270045)
- The disposable base SFO now passes the prior APP_VER/VERSION checks, and the earlier `sce_sys/about` collision was removed from the Sony GP4 input.
- Sony `img_create` advanced to the next exact error: `The file name is not valid. (sce_sys/playgo-chunk.dat)`. OrbisPkgTool's restructure log confirms that it moved the original Sc0 PlayGo files into Image0/sce_sys and retained them as publisher-regeneration files.
- Engineering commit `573df16b5984a39bc5927718808e356e70ae91e8` excludes only `sce_sys/playgo-chunk.dat`, `sce_sys/playgo-chunk.sha`, and `sce_sys/playgo-manifest.xml` from the disposable Sony base-rebuild tree, records each exclusion, and leaves the original PKG 002 untouched.
- This addresses the exact filename rejection but is not yet a proven fix. Continue only through Sony base creation, independent validation/readback, associated patch creation, independent patch validation, and QA. No PS4 deployment or installation.


### Sony GP4 rejects Sc0 license metadata placed in Image0 (2026-11-20, run 44647394043)
- The base PKG 002 readback confirms `CATEGORY=gde`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`, and `PUBTOOLINFO ... st_type=digital50`. The Sony input copy normalizes only APP_VER/VERSION to `01.00`.
- The PlayGo exclusions worked: Sony `img_create` advanced past the prior `sce_sys/playgo-chunk.dat` rejection. It then failed on `The file name is not valid. (sce_sys/license.dat)`.
- The diagnostic log proves the current OrbisPkgTool restructure moved Sc0-only files into Image0/sce_sys. Its upstream GP4 source explicitly marks `sce_sys/license.dat`, `license.info`, `psreserved.dat`, and PlayGo metadata as Sony-managed system targets omitted by strict gengp4 generation. Reference: https://github.com/pearlxcore/OrbisPkgTool/blob/master/OrbisPkgTool/Gp4/Gp4Project.cs
- Engineering commit `7ed2f9d5267e22ed571fe9bb476e0a2e1b27ee15` expands the disposable Sony GP4 input exclusion list to include `sce_sys/license.dat`, `sce_sys/license.info`, and `sce_sys/psreserved.dat`, in addition to the already excluded PlayGo files. It does not alter the source PKG 002 or any console files.
- This is grounded in the upstream GP4 managed-target policy and the exact Sony rejection, but remains unverified until the next publisher run completes. The warning about missing `sce_sys/pronunciation.xml` is non-fatal in this run; do not invent that file.
- Artifact `14588638078` contains only diagnostic SFO/report text; no PKG 003 exists. Host tests for the preceding commit passed (run `44647394121`). Continue with Sony base creation, embedded SFO/storage readback, patch GP4 association, patch build, independent validation and QA. No PS4 installation or activation.


### Candidate 003 engineering gate: reject surrogate-base association (2026-11-20, run 44647569451)
- Build run `44647569451` completed and produced artifact `14588164308` (4,273,765-byte ZIP). Its candidate PKG is 4,915,200 bytes, SHA-256 `05CE78298DEC39D6B3E2D136EAEB17C9AE5B4F91F00F833001AA6C94EBF1829E`; the structural validator reported `Validation: PASS`, `CATEGORY=gpe`, `APP_VER=01.01`, and matching Content ID/TITLE_ID.
- Independent QA of the uploaded evidence found a release-blocking association defect: the GP4 `app_path` is `PS4-Neural-Engine-002-SONY-BASE.pkg`, not the exact base artifact `PS4-Neural-Engine-002.pkg` that the user owns/has installed. The workflow reconstructed this surrogate base, normalized its version strings, and removed Sony-managed files before building it (2,621,440 bytes; only 7 GP4 payload entries). A successful GP4 association to that surrogate does not prove compatibility with the actual PKG 002.
- The GP4 also logs redundant `eboot.bin` and `ps4_neural_engine.prx` entries and no referable blocks. The candidate is structurally valid but not approved as an update. It must not be installed or distributed as a completed patch.
- Engineering correction removes the surrogate Sony-base rebuild from the candidate workflow and points `gp4_proj_create --app_path` to the exact downloaded, validated PKG 002 artifact. The GP4 audit must assert the original base filename. This is a targeted correction, not a claimed successful build.
- Artifact `14588164308` is retained only as rejected QA evidence. No PS4 installation, app-data/save modification, PRX activation, or plugins.ini change occurred.


### Exact original base rejected by Sony patch publisher; ship paired Sony-built base and patch (2026-11-20)
- Build run `44647832781` tested the exact original PKG 002 as GP4 `app_path`. Its embedded base SFO readback contained `CATEGORY=gde` and `PUBTOOLINFO st_type=digital50`, and GP4 correctly named `PS4-Neural-Engine-002.pkg`; nevertheless Sony `img_create` rejected it with `Format of the param file is not valid. (param.sfo(in app pkg))`. No patch PKG was produced. This proves filename association alone is insufficient and that the original PkgTool-built base SFO/package representation is not accepted by this publisher as an application-package reference.
- The earlier successful publisher run `44647569451` showed that Sony can rebuild a normalized copy of the base (`APP_VER=01.00`, `VERSION=01.00`, `CATEGORY=gde`, `st_type=digital50`) and then create an associated patch against that Sony-built package. However, that rebuilt base had not been included in the artifact, so the patch association did not yet describe a complete deliverable pair.
- Engineering commit `2fb12af329ceaf0d3efd6b2654e639b5de722d60` restores the Sony-built base step, names the rebuilt app `PS4-Neural-Engine-002.pkg`, includes it in the same artifact as patch 003, and points the patch GP4 at that exact bundled base. The artifact must now be treated as a **paired replacement base 002 + patch 003**, not as a patch for the prior original PKG 002. Both files require independent validation and matching hashes before release.
- Independent QA workflow `.github/workflows/ps4-patch-qa-003.yml` downloads the paired artifact, verifies both hashes, validates and extracts both PKGs, checks the GP4 patch association, base `gde/digital50`, patch `gpe/digital25`, Content ID/TITLE_ID, APP_VER and required payload files. QA remains a separate gate; a successful engineering build alone does not approve the pair.
- The base publisher currently emits a warning that `sce_sys/pronunciation.xml` is missing and says the package will not be imported into GEMS. This warning must remain visible in QA evidence; do not fabricate the file. Neither package has been tested on the PS4. No installation or console changes are authorized by this work.


### Sony-compatible replacement-base pair and QA correction (2026-11-20)
- Run 44647038528 used the exact validated PKG 002 artifact and confirmed its embedded SFO contains `st_type=digital50`. The attempted temporary Sony base rebuild failed before patch generation with `Could not create system directory. (sce_sys/about)` because the extracted `sce_sys/about` directory collided with a publisher-managed directory. No package candidate was produced.
- Run 44647914715 then tried the exact original PKG 002 directly as the patch's `app_path`. The Sony publisher rejected the embedded base SFO with `Format of the param file is not valid. (param.sfo(in app pkg))`. The original base SFO has `APP_VER=1.00` and `VERSION=1.00`, while the PS4 PARAM.SFO reference specifies the `XX.YY` format. Do not claim a patch can be built directly against that legacy SFO using this publisher path.
- Run 44647979326 succeeded in building a **replacement base + patch pair** from the original PKG 002 on an isolated hosted Windows runner:
  - Sony-rebuilt base 002: 2,621,440 bytes; SHA-256 `83827C805F5A3F0D8DA4DC412450B5A3488DAB016BE843FB78A07BA31F600964`; SFO `CATEGORY=gde`, `APP_VER=01.00`, `VERSION=01.00`, `st_type=digital50`.
  - Patch 003 candidate: 4,915,200 bytes; SHA-256 `98967D67F7700833EB74427C7837E33AACFDE6AB5C787366C1D5F3D4BAAF535C`; GP4 `volume_type=pkg_ps4_patch`, `storage_type=digital25`, `app_path=PS4-Neural-Engine-002.pkg`; patch SFO `CATEGORY=gpe`, `APP_VER=01.01`, matching Content ID/TITLE_ID.
  - The Sony publisher emitted warnings about missing `sce_sys/pronunciation.xml` and redundant `eboot.bin`/`ps4_neural_engine.prx` entries/no referable blocks. It completed the build, and the independent structural validator reported `Validation: PASS` for both base and patch. These warnings are recorded, not silently ignored.
- This is **not the original PKG 002 byte-for-byte**. The pair uses a Sony-rebuilt replacement base with the same title/content identity, created from the original package's extracted payload. It is not approved for installation on the console; installing/replacing the base may affect the existing app and requires a separate explicit decision and hardware validation.
- Independent QA run 44648108020 validated both package structures and hashes, then failed only because the QA script looked for `changeinfo.xml` in `Image0/sce_sys`, while the publisher correctly stored it in `Sc0/changeinfo/changeinfo.xml`. Engineering commit 2f91cc63236375fb8fe63565181cba614067f640 corrects the QA path check and additionally checks the replacement-base `APP_VER/VERSION=01.00`. A fresh candidate build and independent QA are required; do not treat run 44647979326 as QA-approved.
- Engineering commit ea7c7e21d8e345a309c7a0cf8744d00a9ae50b73 labels the deliverable explicitly as a Sony-rebuilt replacement base. Candidate build run 44648296044 is the fresh run associated with the corrected QA workflow. Await its independent QA result before calling the pair PC-approved.
- No package was installed on the PS4. No saves, app data, PRX activation, canonical PRX, or `plugins.ini` were changed.


### Functional-delta gate: metadata-only candidate rejected (2026-11-20)
- Independent QA now compares the extracted base and patch `eboot.bin` / `ps4_neural_engine.prx` hashes and rejects a candidate when both are byte-identical. This preserves the original requirement that PKG 003 be a **real functional update**, not only a version/SFO bump.
- GitHub compare from the base-002 build commit `de9468c629161aa577d4e5c672cfaf5453957065` to the current engineering line showed no changes to `src/`, `installer/`, or `platform/ps4/` payload sources; intervening changes were package workflow/QA/docs only. The current patch workflow copies `eboot.bin` and `ps4_neural_engine.prx` from the base extraction, so the patch is necessarily metadata-only.
- The candidate pair from build run 44648366299 passed Sony image creation and the independent structural validator, but it must be rejected as a functional update until the payload has an intentional, user-approved behavior change. The current PRX hash `531e41aa3f4535e6004d47900f636d6e2a4b1ce2b2709f165db77ebef7e0a3f0` is the same source PRX already present in the base; the extracted patch copies it unchanged.
- Do not bypass or weaken the executable-delta QA gate. Do not create a metadata-only update, alter the installed app, or install the Sony-rebuilt replacement base as a workaround. Next engineering work requires a concrete functional change to the installer or PRX, followed by host tests, a new base/patch pair build, independent payload-hash/metadata/GP4 QA, and eventual console validation.
- The Sony-rebuilt 002 + 003 files previously emitted are unapproved diagnostic candidates only. No PS4 install, app-data/save change, PRX activation, canonical PRX change, or `plugins.ini` modification occurred.


### Independent executable-delta QA evidence (2026-11-20, run 44648742275)
- QA artifact `14589882524` independently validated both delivered PKGs structurally (`Validation: PASS` for base and patch), confirmed GP4 `pkg_ps4_patch` / `digital25` association to the bundled replacement base 002, and verified package SHA-256 values against the engineering report.
- QA then rejected the candidate at the functional-delta gate. Extracted base and patch executable hashes are identical:
  - `eboot.bin`: base = patch = `C466B001139B95404B52FA91FB74025A973707853398208ACB6755B3D6B5DBB5`.
  - `ps4_neural_engine.prx`: base = patch = `531E41AA3F4535E6004D47900F636D6E2A4B1CE2B2709F165DB77EBEF7E0A3F0`.
- Source comparison at base build commit `de9468c629161aa577d4e5c672cfaf5453957065` confirms `installer/main.c`, `tools/build_pkg.sh`, and `platform/ps4/goldhen/plugin/main.c` are unchanged on current main. The current installer already copies the PRX to `/data/GoldHEN/plugins/`, verifies copied bytes, updates `plugins.ini` idempotently, and writes `/data/GoldHEN/ps4_neural_engine_install.log`; these behaviors are already in base 002, not a new patch delta.
- Engineering build run `44648719738` succeeded and produced a structurally valid replacement base + metadata-versioned patch candidate, but QA run `44648742275` failed intentionally because the candidate contains no executable code change. Artifact `14589882524` is QA evidence only. Do not release or install the pair as a functional update.
- No further code edits should be fabricated merely to make hashes differ. A real PKG 003 requires an explicitly intended new behavior/code change, followed by host tests, rebuilt base/patch, independent QA and eventual controlled hardware validation. No PS4 install, PRX activation, saves/app data change, or `plugins.ini` modification occurred.


### Real PRX delta for PKG 003 — baseline/payload separation (2026-11-20)
- The selected functional change is an explicit GitHub disconnect controller chord in the GoldHEN PRX: `L3+L1+R3+Circle` cancels active device authorization, joins in-flight upload work, clears the in-memory token, and closes the HTTP/TLS session. The normal OAuth chord is latched while the disconnect chord is held to prevent accidental authorization.
- Engineering commit `1323f0f6bf28c9b696ca6620cd02559677c6c6cb` builds the updated PRX. Build run `44649084819` passed; its `ps4/ps4_neural_engine.prx` is 206,528 bytes, SHA-256 `7561da3d3b83766becddcb37c1257d83c61009b5e814825ee60694d19f9cf704`. The binary contains the new disconnect notification string.
- Candidate run `44649324529` exposed an engineering workflow mistake: it downloaded the new build's PKG 002 as the baseline and then copied the same PRX into the patch. Although Sony `img_create` and structural validation passed, this pair has no executable delta and must be rejected by independent QA.
- Engineering commit `cae6f8056b1e822db7f80cf3fe2d103a8b456ac8` corrects the separation:
  - Baseline is pinned to the previously validated original PKG 002 artifact from run `44644315395` (artifact `14588380285`), preserving its older PRX.
  - The updated PRX is downloaded separately from current build run `44649084819` and used only as the patch's `ps4_neural_engine.prx` payload.
  - The workflow compares the baseline PRX hash with the new PRX hash and stops if identical; the independent QA then compares the extracted Sony-rebuilt base with the patch payload.
- Corrected candidate build run `44649537623` is in progress. Do not call PKG 003 approved until its resulting pair passes the separate QA workflow. The bridge workflow only staged the validated PKG 002 in `/data/pkg`; no package was installed and no PRX was activated.
