# Relatório de leitura dos logs PS4 — CE-34878-0
Data: 2026-11-21
Branch: `feature/gcn8-numeric-kernels`

## Coleta

A coleta foi feita pelo runner Windows self-hosted na LAN, usando FTP somente para leitura.

- Workflow: https://github.com/demolidorreddragons/NR/actions/runs/44874963364
- Artifact: `ps4-crash-logger-evidence`, ID `14695721468`
- SHA-256 do ZIP do artifact: `ad00bdd2ba2d363cee748dd06bc8126e64035dbe74941a51a5da9ecd87f28211`

Os caminhos `/user/data/pne.log` e `/data/pne.log` não existem no FTP (550). O log efetivamente encontrado na raiz de `/data` foi:

- `/data/pne_gcn8_compute_smoke_44873622760.log`
- 324 bytes
- SHA-256: `a6b3b08dcb100a7f9f4468022e14ecab7e20f4b5411b2682d0176ae74a686562`

## Conteúdo integral do log do probe

```text
PNE_GCN8_LOG_OPEN=1
LOG_PATH=/data/pne_gcn8_compute_smoke_44873622760.log
LOG_OPEN anchor=0x0000000000400450
STAGE=MAIN_ENTERED
STAGE=SIGNAL_INSTALL_FAILED
STAGE=BEFORE_COMPUTE_SMOKE
STAGE=GNM_IMPORT_ADDRESS_NULL
STAGE=GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS
STAGE=RESULT_FAIL_GNM_IMPORT_ADDRESS
STAGE=PNE_GCN8_SMOKE_EXIT=1
```

## Interpretação confirmada

1. O processo abriu o arquivo de log e chegou a `main`. Portanto, este teste não falhou antes da entrada no executável.
2. A instalação dos handlers de sinal falhou (`SIGNAL_INSTALL_FAILED`). Logo, este log não tinha captura de crash por sinal habilitada; a ausência de linha `CRASH` não prova ausência de falha.
3. O endereço de `sceGnmSubmitDone` foi observado como nulo em runtime (`GNM_IMPORT_ADDRESS_NULL`).
4. O probe não chamou nenhuma função GNM, não carregou shader e não fez dispatch. O próprio código registrou `GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS`.
5. Ao detectar o endereço nulo, o código escolheu retornar explicitamente código 1 (`PNE_GCN8_SMOKE_EXIT=1`). O log não contém uma exceção capturada nem identifica, por si só, se o CE-34878-0 foi emitido pelo encerramento não-zero ou por outra falha posterior.

## Cruzamento com o ELF produzido pelo CI

O `smoke.elf` do artifact de build foi inspecionado com `readelf`:

- `DT_NEEDED` inclui `libSceGnmDriver.so`.
- `sceGnmSubmitDone` consta como símbolo dinâmico indefinido.
- Existe uma relocação `R_X86_64_GLOB_DAT` para esse símbolo no endereço de slot `0xc000`.

Isso é coerente com o marcador de runtime: a dependência foi gravada no ELF, mas o endereço observado pelo programa ficou nulo. A causa exata ainda exige conferir o export/NID correspondente no import library OpenOrbis e a resolução feita pelo loader no firmware alvo.

## Outros arquivos lidos

- `/data/sce_coredumps/`: diretório vazio; nenhum core dump disponível.
- `/data/sce_logs/log.bin`: 16.777.216 bytes, SHA-256 `518b0bc57f02bb4d6c6e2935504c3be8e79bade22b10fe0f7e7a51a26800c3ec`. É binário sem strings legíveis; não foi decodificado como relatório de crash.
- `/data/Loader_Logs.txt`: registra verificação RSA bem-sucedida de `NPXS39041_000/app0/Media/rsa.pub`; não menciona Neural Rendering nem GNM.
- `/data/memory.log`: somente mensagens de criação de mspace e estatísticas de alocação da libc; não contém falha.

## Conclusão e próximos passos

**Achado principal:** o primeiro bloqueio concreto está na resolução do import `sceGnmSubmitDone`, não em shader, InputUsage, GPU VA ou dispatch. O processo alcançou `main`, observou endereço de import nulo e encerrou pelo ramo de falha do probe.

Isso ainda não prova que a resolução do import seja a causa única do CE-34878-0. O logger também revelou que os handlers de sinal não foram instalados, então precisamos corrigir essa instrumentação.

Próxima investigação segura, sem novo teste de GPU no console:
1. verificar no import library OpenOrbis o export/NID exato de `sceGnmSubmitDone` e a biblioteca a que pertence;
2. comparar o import do ELF com o padrão de imports de uma amostra OpenOrbis conhecida que realmente resolva GNM no firmware;
3. instrumentar `sigaltstack` e cada `sigaction` separadamente, registrando `errno`, para descobrir por que `SIGNAL_INSTALL_FAILED` ocorreu;
4. só então preparar um novo probe de inicialização, sem dispatch.

Nenhuma instalação, remoção ou alteração no PS4 foi realizada durante esta coleta.
