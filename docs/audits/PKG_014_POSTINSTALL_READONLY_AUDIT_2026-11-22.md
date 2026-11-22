# PKG 014 — post-install read-only evidence audit

Date: 2026-11-22  
Target: PS4, TITLE_ID PNEU00001  
Expected package: PS4-Neural-Engine-014.pkg  
Build run: 45072353238  
Build commit: 8aacfe426fac164eca681aa87829a509045cbed1

## Collection provenance

- Read-only current-state workflow: 45073902620, job 137366750687, SUCCESS.
- Evidence artifact: 14774122951, 13,265,687 bytes, ZIP SHA-256 723b79275d71a0b62bb1add2b9873b5a7c6136f27f8f20723a3c5670bb050988.
- Collection timestamp: 2026-11-22T21:46:06Z.
- Log analysis workflow: 45074043686, job 137367175648, SUCCESS.
- Log-analysis artifact: 14774566102, 970,739 bytes, ZIP SHA-256 a85daa8d0e89a90f59bacc3d6372766ec066564ba871300d017870b37c0b0dad.
- All remote operations were FTP LIST, GETSIZE and GET only. No remote files were changed/deleted; no install, PRX activation, plugins.ini edit or GoldHEN restart occurred.

## Installed package identity

- Remote `/user/app/PNEU00001/app.pkg`: 6,619,136 bytes.
- SHA-256: `96c15b46e7579c5f728a131279e986af04a9e4683c9903221e418ca52a3842db`.
- This exactly matches the approved PKG 014 build artifact and the staged remote PKG digest.
- `app.json` references `/user/data/pkg/PS4-Neural-Engine-014.pkg` and reports packageDigest `BCE0E519BDBE271AEB92CA4458D253DF49C49A26E6F5A2128C6DB15482E29047`. This field is not the raw-file SHA-256 and must not be compared as though it were. The installed `app.pkg` raw-file digest is the verified identity.
- app.xml/app.json were retrievable; installed TITLE_ID directory exists.
- This proves PKG 014 is installed. It does not by itself prove that the app was launched or that the notification displayed PASS.

## Logs and crash evidence

- `/data/sce_coredumps/` listing contained only `.` and `..`: no coredump was present at collection time.
- `/data/sce_logs/log.bin`: 16,777,216 bytes. Its SHA-256 changed between two read-only snapshots (21:46 and 21:47 UTC), so it is a live/rotating system log and each captured digest must be preserved separately.
- Binary printable-string scan found only three low-quality false positives containing fragments such as `GNm`/ `GCn`; no readable PNE, GNM, GFX, shader, crash, fault or exception event was recovered. The file format is not decoded by this scan.
- `/user/data/pne.log` and `/data/pne.log` were unavailable (FTP 550); no app-specific persistent preflight log is currently present at those paths.
- `/data/Loader_Logs.txt` contains RSA signature verification for a different app path (`NPXS39041_000`), not Neural Rendering runtime evidence.
- `/data/memory.log` contains successful libc mspace initialization/statistics calls; no Neural Rendering-specific error is recorded.
- Historical GNM diagnostic logs remain historical, not PKG 014 launch logs:
  - 44873622760: static GNM import address null; no GNM calls.
  - 44876293567: dynamic module load and one DLSYM succeeded; no GNM calls.
  - 45040217180: five GNM exports resolved; signal alternate-stack setup returned EINVAL and fell back without SA_ONSTACK; no GNM calls or compute dispatch.
- No current PKG 014-specific crash or runtime error was found in the collected readable evidence.

## Findings and disposition

1. **PASS — package identity:** installed `app.pkg` hash exactly matches PKG 014.
2. **PASS — no crash dump observed:** the coredump directory was empty during collection.
3. **LIMITATION — no app-specific runtime log:** the preflight writes only a user notification; the existing `pne.log` paths do not exist.
4. **LIMITATION — system log opaque:** current `log.bin` is binary and not decoded by the simple printable-string scan.
5. **NOT PROVEN — app launch/preflight result:** installed package identity does not prove execution. No new user-visible PASS/FAIL notification was provided in this collection.
6. **NOT PROVEN — neural rendering runtime:** PKG 014 only checks six asset hashes and resolves five GNM exports. It does not call GNM, submit command buffers, dispatch shaders or run inference.

No code fix is justified by the current console snapshot: it contains no reproducible PKG 014 runtime failure. Do not invent a fix or promote an experimental GNM call. The next engineering gate is to improve diagnostic observability and independently validate the exact GNM ABI/command-buffer contract before enabling any GPU submission. Hardware execution remains unvalidated.
