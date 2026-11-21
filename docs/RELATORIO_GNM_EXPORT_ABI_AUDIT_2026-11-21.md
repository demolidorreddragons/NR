# Auditoria de exports GNM e contratos ABI — 2026-11-21

Branch: `feature/gcn8-numeric-kernels`

## Resultado dos workflows

- Build PS4/PKG: [run 44920040199](https://github.com/demolidorreddragons/NR/actions/runs/44920040199) — sucesso.
- QA independente do PKG: mesmo run, job `independent-pkg-qa` — sucesso; validou GP4, PKG, SFO, manifest e payload, inclusive os modos Startup-Only e GNM-Load-Only.
- Compilação GCN8/PSBC e geração de shaders: [run 44920040227](https://github.com/demolidorreddragons/NR/actions/runs/44920040227) — sucesso.
- Testes host: [run 44920048682](https://github.com/demolidorreddragons/NR/actions/runs/44920048682) — sucesso, 7/7 CTest.

Esses resultados são de CI/host. O novo pacote de auditoria de exports ainda não foi enviado nem executado no PS4.

## Descoberta ABI crítica

Foi comparado o cabeçalho de stubs OpenOrbis `include/orbis/GnmDriver.h` com o cabeçalho e backend Orbis do OpenGNM fixado em:

- Repositório: https://github.com/PS4-OpenGNM/opengnm
- Commit: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`
- Arquivos: `include/gnmdriver.h`, `src/driver_orbis.c`

O cabeçalho OpenOrbis declara vários símbolos como `void sceGnmFunction()` sem parâmetros. Essas declarações são stubs incompletos e **não podem ser usadas para definir function pointers de runtime**.

Contratos confirmados pelo OpenGNM fixado:

| Símbolo | ABI documentada pelo OpenGNM | Uso pretendido |
|---|---|---|
| `sceGnmDispatchInitDefaultHardwareState` | `uint32_t PS4_SYSV_ABI(uint32_t* cmdbuf, uint32_t size)` | Emite estado compute no command buffer; retorna dwords escritos |
| `sceGnmFlushGarlic` | `void PS4_SYSV_ABI(void)` | Flush Garlic |
| `sceGnmSubmitCommandBuffers` | `int32_t PS4_SYSV_ABI(uint32_t count, void* const dcb_addrs[], uint32_t* dcb_sizes, void* const ccb_addrs[], uint32_t* ccb_sizes)` | Submete DCB/CCB |
| `sceGnmSubmitDone` | `int32_t PS4_SYSV_ABI(void)` | Sinaliza fim da submissão |

O mesmo backend declara `PS4_SYSV_ABI` nas funções exportadas. Qualquer ponteiro obtido por `sceKernelDlsym` deve usar a convenção e a assinatura completa, não o protótipo vazio de OpenOrbis. O endereço resolvido, isoladamente, não valida ABI nem torna seguro chamar a função.

## Decisão técnica

1. O modo GNM-Load atual prova no hardware que o módulo pode ser carregado e que `sceGnmSubmitDone` resolve por DLSYM. Ainda não há evidência de que os outros três exports resolvam no firmware 14.02; o novo export audit precisa ser executado para obter esse dado.
2. A auditoria de endereços é apenas resolução de símbolos; não chama nenhuma função GNM.
3. Não foi implementado wrapper dinâmico de compute nesta etapa. Antes de fazê-lo, é necessário obter o resultado real do export audit no PS4 e revisar as assinaturas completas de cada função que o caminho de compute usa.
4. O harness de compute atual ainda contém imports diretos de `gnmdriver.h`; portanto, não está pronto para ser considerado um caminho runtime exclusivamente dinâmico.
5. Não staging, instalação ou execução do novo pacote foram feitos. A autorização anterior cobria somente o staging do pacote GNM-Load anterior, não este build novo.

## Gate de compute continua bloqueado

Mesmo com os workflows verdes, não há autorização técnica para dispatch. Permanecem independentes e não resolvidos:

- GPU VA de V# / tabela de recursos no firmware 14.02;
- semântica PSBC InputUsage: as duas hipóteses de localização de slots continuam divergentes no relatório de shader;
- ABI completa e disponibilidade real dos exports no firmware alvo;
- validação independente e execução controlada no hardware.

Não converter sucesso de build, QA de PKG ou DLSYM em alegação de compute funcional.

## Próxima sequência

1. Obter o log de hardware do build novo e registrar quais dos quatro símbolos resolvem.
2. Se todos resolverem, construir uma camada pequena de resolução dinâmica com typedefs `PS4_SYSV_ABI` explícitos e testes host para as assinaturas/contratos; remover os imports GNM estáticos do caminho de runtime.
3. Fazer revisão independente da tabela de funções chamada pelo harness.
4. Manter dispatch bloqueado até fechar GPU VA e InputUsage; somente então preparar um teste de hardware separado, sujeito à autorização específica do usuário.
