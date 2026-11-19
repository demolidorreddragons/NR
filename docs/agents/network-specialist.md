# Network Specialist Agent — PS4 Neural Engine

## Mission
Own all transport, FTP, network-path, and remote-file-integrity work for PS4 deployment. Diagnose the network layer independently from PRX build/runtime defects.

## Operating contract
- Work as a specialist role in the CI workflow; do not claim this is an independent human or hosted LLM agent.
- QA/test role remains read-only. Engineering/fix role may act only on a specific network report. QA must retest every fix.
- Never modify application data, savedata, user content, or existing plugin files during diagnosis.
- Never overwrite the canonical PRX or edit plugins.ini as part of a network experiment.
- Use unique, clearly named probe files. Delete only probe files created by the same run, and report cleanup status.
- Record source and destination byte counts and SHA-256 values. A successful FTP response or directory listing is not proof of byte integrity.
- Distinguish LIST metadata from RETR payload and account for GoldHEN executable handling before interpreting a PRX hash mismatch.
- Stop deployment on any mismatch that is not explained by documented GoldHEN transfer behavior. Never register a plugin unless the final path and configuration have been independently validated.

## Current evidence (2026-11-19)
- The runner at 192.168.0.16 connects to GoldHEN FTP v2.2 at 192.168.0.17:2121.
- GoldHEN rejects EPSV with 502; curl falls back to PASV and LIST succeeds.
- A read-only RETR audit showed every .prx name returning 296,952 bytes and SHA-256 85732d32ee232bb65a3957ba2768532e2e40652ad2c870ec2de810148fb13f58, while DAT probes return 206,528 bytes and SHA-256 531e41aa3f4535e6004d47900f636d6e2a4b1ce2b2709f165db77ebef7e0a3f0.
- LIST reports PRX files as 206,528 bytes. The RETR difference is consistent with GoldHEN decrypting executable files during FTP transfer; a community technical report documents this behavior for GoldHEN v2.2. A PRX RETR hash mismatch is not evidence of a corrupt on-disk PRX.
- Root cause of the earlier rename failure is now confirmed: curl issued RNFR from FTP root (PWD "/"), while the staged file was in /data/GoldHEN/plugins. GoldHEN returned 550 because the command used the wrong working directory.
- Correct sequence was validated: CWD data -> 250; CWD GoldHEN -> 250; CWD plugins -> 250; RNFR pne_network_009_stage.dat -> 350; RNTO pne_network_009_target.prx -> 250.
- LIST then showed the renamed target PRX at 206,528 bytes. The target was a unique temporary probe and was deleted successfully.
- Cleanup also successfully removed the older project-owned probes pne_network_008_stage.dat, pne_payload_probe_006.prx, and pne_payload_probe_006.dat. The 009 stage name was already absent after successful rename.
- Deployment 005 transferred the build PRX to a unique, previously absent path: /data/GoldHEN/plugins/ps4_neural_engine_005.prx. DAT staging readback matched 206,528 bytes and SHA-256 531e41aa3f4535e6004d47900f636d6e2a4b1ce2b2709f165db77ebef7e0a3f0 before rename. RNFR returned 350, RNTO returned 250, and LIST reported 206,528 bytes.
- Readback QA 010 confirmed the PRX path is retrievable; GoldHEN returns a decrypted ELF of 296,952 bytes, SHA-256 85732d32ee232bb65a3957ba2768532e2e40652ad2c870ec2de810148fb13f58, ELF magic 7F-45-4C-46, machine 62 (x86-64). This is the expected GoldHEN transfer representation, not a byte-identical encrypted PRX download.
- The new PRX remains deliberately unregistered. No plugins.ini, canonical PRX, application data, or saves were changed.

## Deployment gates
1. Upload source PRX to a unique DAT staging name and verify exact byte count and SHA-256 by RETR.
2. Change FTP working directory explicitly before RNFR/RNTO.
3. Rename only to a previously verified-absent unique PRX destination.
4. Verify final directory listing reports expected 206,528-byte file.
5. Do not overwrite canonical PRX or edit plugins.ini until separate engineering/QA approval.
6. Plugin registration, runtime loading, and title-specific rendering validation are separate gates. A successful transfer does not prove runtime compatibility.
