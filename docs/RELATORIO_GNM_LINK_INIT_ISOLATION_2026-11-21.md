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

## Integração do logger no PRX existente
O build host do PRX revelou referências indefinidas a `stage_log` em `compute_smoke.cpp`, porque o Makefile do plugin ainda não compilava nem ligava `stage_log.c`. A regra `stage_log.o` foi adicionada à lista de objetos e ao link do PRX. A correção está no commit `5249a9f59f23c70809ee3aa8ca4beccd65237a4d`; uma verificação estática adicional protege essa dependência.

## GNM-Init-Only: bloqueado por ABI/API não fechada
A documentação Getting Started do umbrella OpenGNM contém um exemplo com `sceGnmInitialize()`. Porém, no commit OpenGNM fixado pelo projeto (`4b295ca54c82c83acf308d1c646a2dfa9ae57350`), o header público `include/gnmdriver.h` não declara `sceGnmInitialize`, `sceGnmInit` ou `sceGnmTerminate`. O header OpenOrbis `include/orbis/GnmDriver.h` inspecionado também não declara `sceGnmInitialize`.

Portanto, não foi adicionada uma declaração manual, NID presumido ou chamada de inicialização. O exemplo da documentação não basta para fechar assinatura, biblioteca/import NID e semântica da função na combinação de toolchain/driver usada pelo NR.

## Sequência de diagnóstico
1. Startup-Only — sem import GNM.
2. GNM-Link-Only — import de símbolo resolvido, nenhuma chamada GNM.
3. GNM-Init-Only — aguardando confirmação da API/ABI e disponibilidade no import library OpenOrbis.
4. Shader/Dispatch — permanece bloqueado pelas pendências de GPU VA/V# e InputUsage.

## Validação
A validação host verifica isolamento estático dos modos, presença do objeto de logger no PRX e sintaxe Bash. O build PRX anterior ao ajuste de Makefile compilou/linkou o teste de ABI GNM, mas falhou no PRX principal por ausência de `stage_log.o`; isso motivou a correção citada. A primeira compilação real do GNM-Link-Only identificou que o sysroot OpenOrbis expõe uma declaração `ucontext_t` musl incompatível com o frame FreeBSD fornecido pelo kernel, e também tem um alias incorreto para `sa_sigaction`. O handler foi alterado para usar o membro de união real do header OpenOrbis e preservar 48 palavras brutas do contexto, incluindo candidatos em offsets 22/25/28/31, sem afirmar RIP/RSP até a disposição do frame estar confirmada. A execução OpenOrbis `44873396730` compilou e empacotou o modo GNM-Link-Only com sucesso, além de concluir o build do PRX principal. O PKG diagnóstico passou pela verificação de payload interna (`GNM_LINK_ONLY_PKG_PAYLOAD=PASS; COMPUTE_SHADER=ABSENT`), SHA-256 do PKG `2f9b3ca80b2f25a5125f07a38271c8fc873c633f11a47d06e44f6f360e29ad88`, artifact `14694443814`. Essa execução precede a correção do literal de newline no dump de contexto; a suíte host da revisão posterior passou e uma nova execução OpenOrbis com essa correção está na fila. QA independente do artefato principal ainda estava em andamento no momento deste relatório. Nenhuma validação no PS4 foi feita. Nenhum PKG foi instalado ou executado no console.
