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
