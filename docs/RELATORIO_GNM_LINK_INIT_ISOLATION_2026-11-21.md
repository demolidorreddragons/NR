# Relatório — isolamento GNM Link-Only e bloqueio de Init-Only
Data: 2026-11-21

## Objetivo
Separar o carregamento/resolução das dependências GNM da execução de comandos GNM, sem iniciar shader, alocar memória GPU ou submeter trabalho.

## GNM-Link-Only implementado
O modo `GCN8_SMOKE_MODE=gnm-link` produz um PKG diagnóstico próprio (TITLE_ID `PNEU00004`). Ele:
- compila apenas `smoke_main.cpp` e `stage_log.c`;
- liga `-lSceGnmDriver`;
- toma o endereço de `sceGnmSubmitDone` para forçar uma relocação de import, sem chamar a função;
- registra `GNM_IMPORT_RESOLVED` e `GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS` somente depois de `main` iniciar;
- não inclui `compute_smoke.cpp`, shader, alocação de memória GPU, dispatch ou submit.

Interpretação: se o processo alcança `GNM_IMPORT_RESOLVED`, a relocação do símbolo de import foi resolvida e o processo chegou a `main`. Isso é evidência do caminho de dependência/import, mas não é uma prova independente de que um PRX específico foi efetivamente carregado em todas as configurações de loader. A validação definitiva exige o ELF produzido e log no PS4.

## GNM-Init-Only: bloqueado por ABI/API não fechada
A documentação Getting Started do umbrella OpenGNM contém um exemplo com `sceGnmInitialize()`. Porém, no commit OpenGNM fixado pelo projeto (`4b295ca54c82c83acf308d1c646a2dfa9ae57350`), o header público `include/gnmdriver.h` não declara `sceGnmInitialize`, `sceGnmInit` ou `sceGnmTerminate`. O header OpenOrbis `include/orbis/GnmDriver.h` inspecionado também não declara `sceGnmInitialize`.

Portanto, não foi adicionada uma declaração manual, NID presumido ou chamada de inicialização. O exemplo da documentação não basta para fechar assinatura, biblioteca/import NID e semântica da função na combinação de toolchain/driver usada pelo NR.

## Sequência de diagnóstico
1. Startup-Only — sem import GNM.
2. GNM-Link-Only — import de símbolo resolvido, nenhuma chamada GNM.
3. GNM-Init-Only — aguardando confirmação da API/ABI e disponibilidade no import library OpenOrbis.
4. Shader/Dispatch — permanece bloqueado pelas pendências de GPU VA/V# e InputUsage.

## Validação
A validação host verifica isolamento estático dos modos e sintaxe Bash. Não substitui compilação OpenOrbis nem execução em PS4. Nenhum PKG foi instalado ou executado no console.
