# Diagnóstico CE-34878-0 — isolamento de startup GCN8

Data: 2026-11-21  
Branch: `feature/gcn8-numeric-kernels`  
Incidente: [Issue #13](https://github.com/demolidorreddragons/NR/issues/13)  
Classificação: revisão estática interna; hardware ainda NOT_RUN para o probe isolado.

## 1. Fato de hardware registrado

O usuário informou que o pacote `PS4-Neural-Rendering-GCN8-Smoke-44778603716.pkg` foi instalado no PS4 Pro (FW 14.02 / GoldHEN 2.4b18.14), mas abrir o app resultou em `CE-34878-0`.

Build reportado:
- Commit: `a3c64c9a777f95ee966d0f9a6cd82a75e5f42de8`
- Workflow: `44778603716`
- PKG SHA-256: `5813d55c48dd43d6700867621a4b48a9427c2871c22a460b8d966b2500c83ae8`

O erro de lançamento não prova que `main` foi alcançado, que o shader foi carregado, que GNM foi inicializado ou que houve dispatch.

## 2. Probe de isolamento — evidência de código

O script `tools/build_compute_smoke_pkg.sh` tem dois modos:
- `compute`: compila e liga `smoke_main.cpp`, `compute_smoke.cpp`, binding/resource-table e shader-binary contract; inclui `-lSceGnmDriver`.
- `startup`: compila `smoke_main.cpp` com `PNE_GCN8_STARTUP_ONLY=1`; omite todos os objetos de compute/binding e liga somente `-lc -lc++ -lkernel`.

No modo startup, `smoke_main.cpp` registra:
- `PNE_GCN8_LOG_OPEN`
- `LOG_PATH` quando um arquivo de log é aberto
- `PNE_GCN8_APP_MAIN_ENTERED`
- `STAGE=BEFORE_COMPUTE_SMOKE`
- `RESULT=PASS; MODE=STARTUP_ONLY; GNM_NOT_LINKED`

O probe tenta abrir log em `/user/data` e `/data`; se ambos falharem, usa stdout e continua. Ele retorna antes de chamar `pne_gcn8_compute_smoke_run`.

Limitação de empacotamento identificada: o script ainda exige e inclui `compute_smoke_add.sb` no PKG startup, embora o executável startup não o abra. Isso é payload supérfluo, não dependência ELF de GNM; deve ser removido para tornar o artefato diagnóstico mais limpo.

## 3. Evidência CI do build isolado

Workflow [44863812886](https://github.com/demolidorreddragons/NR/actions/runs/44863812886) terminou `success`. Job `136723853934` compilou e validou ambos os pacotes.

| Artefato | SHA-256 | Resultado |
|---|---|---|
| `PS4-Neural-Rendering-GCN8-compute-44863812886.pkg` | `fa9efc2cc7457ff005a05bc697fb437ec930ef33fa86de1879841e86181f04fb` | Payload self-validation PASS |
| `PS4-Neural-Rendering-GCN8-startup-44863812886.pkg` | `0446077f7751965f482287498a9f0eb56793940c249e00302a138d3ff32969ed` | Payload self-validation PASS |

Artifact ZIP: `nr-gcn8-compute-smoke-ps4-pkg`, ID `14691072641`.

A CI comprova compilação/link e integridade estrutural do pacote. Não comprova instalação ou execução no PS4.

## 4. Árvore de diagnóstico

| Resultado observado no PS4 | Interpretação permitida |
|---|---|
| Startup-only também falha antes de `PNE_GCN8_APP_MAIN_ENTERED` | Investigar loader/ELF/FSELF/manifesto/runtime inicial; não atribuir ao GNM compute |
| Startup-only chega a `RESULT=PASS`, compute falha | Restringe a falha ao caminho adicional do compute package, incluindo import GNM, leitura do shader, direct memory, setup, submit ou GPU; ainda não identifica qual |
| Compute grava `PNE_GCN8_COMPUTE_SMOKE_BEGIN` e depois falha | `main` e chamada do smoke foram alcançados; usar último marcador para localizar estágio |
| Compute falha sem log | Não prova falha antes de main; confirmar stdout/crash report/core dump e caminho de log |

## 5. Dados ainda necessários para fechar a causa

1. Resultado de execução do PKG startup-only no mesmo console/firmware.
2. Se houver crash, crash report/core correspondente, com módulo, faulting PC e backtrace.
3. Se o compute iniciar, arquivo de log `/user/data/pne_gcn8_compute_smoke_<run>.log` ou `/data/...`, mais stdout/último estágio.
4. Hash do PKG efetivamente testado para correlacionar com o artefato.

Sem esses dados, não escolher uma causa nem fazer patch especulativo.

## 6. Segurança e estado

- Nenhuma instalação ou execução foi realizada por esta análise.
- Nenhum PRX foi carregado; `plugins.ini` não foi alterado.
- Nenhum dispatch GPU foi presumido.
- Nenhuma alteração de runtime foi feita.
- O teste de hardware permanece bloqueado até plano reversível aprovado e autorização explícita.

**Conclusão:** o probe startup-only fornece um discriminador válido em nível de ELF/imports, e a CI confirmou que ambos os pacotes foram construídos. Como ainda não há resultado de execução do probe, a causa do `CE-34878-0` permanece **INDETERMINADA / BLOCKED**.
