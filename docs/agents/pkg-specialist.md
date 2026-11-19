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
