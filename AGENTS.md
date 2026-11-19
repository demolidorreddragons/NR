# Mandatory operating rules — Neural Engine

## Read and understand before every action

**Before taking any action in this repository or project, read this file from beginning to end and understand it.** Then read the applicable project instructions and workflow documentation before editing files, triggering workflows, deploying artifacts, or touching the PS4. Do not act first and consult the rules afterward.

If an instruction is unclear, resolve the ambiguity from the repository documentation and current project state before acting. If it cannot be resolved safely, stop only at that genuine blocker and ask one precise question.

## Execution discipline

- Do the requested work; do not replace execution with progress announcements, apologies, promises, or repeated requests for authorization already given.
- Do not send unsolicited status updates. Report only a concrete result, a genuine blocker requiring user input, or a status explicitly requested by the user.
- Never claim background work continues between messages. Never claim a build, test, deployment, validation, or hardware check succeeded unless its result was actually inspected.
- When the user says **Go** or otherwise authorizes continuation, continue the existing task from the actual repository and workflow state. Do not ask for another Go.
- Keep work moving through the next safe, deterministic steps. Do not stop merely to narrate that work is continuing.

## Engineering and QA integrity

- Separate engineering changes from QA review. QA audits and reports; Engineering addresses the findings; network/deployment work handles transfer and integrity.
- Do not turn an unverified hypothesis into a code change presented as a fix. Inspect the relevant source, logs, metadata, and authoritative documentation first.
- After a change, run the relevant tests and inspect their actual results. For package work, independently inspect package metadata, GP4 association, contents, hashes, and validation output.
- A base PKG is not a patch. Do not describe a candidate as installable or ready until it is a genuine associated patch and has passed package validation and QA. Hardware readiness requires actual console validation.
- Never invent workflow runs, artifacts, hashes, test results, or successful outcomes.

## PS4 safety boundaries

- Do not uninstall the app, alter saves or app data, activate a PRX, or edit `plugins.ini` without a clear technical reason and the user's approval.
- Do not deploy or execute uninspected third-party binaries on the privileged Windows runner that has access to the PS4 network.
- Preserve existing verified artifacts and deployment state unless a change is necessary, understood, and authorized.
- Keep PRX staging, activation, and package installation as separate operations. A file being present on the console does not mean it is activated or safe to load.

## Final reporting

Be concise and factual. State exactly what changed and cite the commit or workflow evidence when available. Clearly distinguish completed work, failed attempts, unverified candidates, and remaining hardware checks. Do not pad the response with status narration.
