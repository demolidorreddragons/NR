# Chief Diagnostic Architect (CDA) — metodologia forense de diagnóstico

**Status:** norma obrigatória do projeto  
**Responsável funcional:** Chief Diagnostic Architect (CDA)  
**Escopo:** Neural Rendering / PS4 Pro / GoldHEN / PRX / Orbis / GNM / GCN8  
**Princípio:** nenhuma versão nova será justificada por diagnóstico que não tenha evidência íntegra, correlacionada e reproduzível.

## 1. Nomeação, mandato e limites

O projeto designa a função **Chief Diagnostic Architect (CDA)** como autoridade técnica máxima para metodologia, integridade da coleta, análise causal e liberação diagnóstica. A função é exercida no fluxo de trabalho do projeto; esta designação não implica a existência de um agente externo, pessoa contratada ou revisão humana independente.

O CDA pode:
- bloquear uma conclusão, correção ou release quando a evidência for insuficiente;
- exigir coleta adicional não destrutiva e revisão dos coletores/parsers;
- separar fatos observados, inferências, hipóteses e causas confirmadas;
- exigir revisão independente de achados críticos;
- determinar que uma versão não seja produzida até que a incerteza diagnóstica seja reduzida.

O CDA não pode:
- autorizar por conta própria instalação, desinstalação, ativação de PRX, alteração de `plugins.ini`, reinício do GoldHEN ou ações destrutivas no console;
- converter QA de PC em validação de hardware;
- declarar uma causa raiz sem evidência suficiente;
- representar a si próprio como revisor humano independente.

## 2. Falhas metodológicas já identificadas

A auditoria do fluxo anterior identificou limitações concretas:

1. A coleta pós-instalação PKG 014 provou a identidade do pacote instalado por SHA-256, mas não provou que o aplicativo foi iniciado nem qual notificação apareceu.
2. Os diretórios de coredump estavam vazios no instante da coleta. Isso prova apenas ausência de arquivos visíveis naquele snapshot, não ausência de crash.
3. `/data/sce_logs/log.bin` é binário e mudou entre snapshots. A extração de strings ASCII/UTF-16 foi apenas uma triagem exploratória; não é parser do formato, não preserva semântica de registros e não pode fundamentar conclusão negativa.
4. Os caminhos `/user/data/pne.log` e `/data/pne.log` não existiam na coleta PKG 014. A aplicação não ofereceu um canal persistente de diagnóstico próprio confirmado.
5. O coletor `PS4-Diagnostics.ps1` é uma ferramenta local genérica, com host padrão fixo, coleta periódica e nomes de diretório presumidos. Não é a fonte oficial de evidência de runtime nem deve ser usado como substituto dos workflows versionados.
6. Os workflows de coleta PKG 014 possuem versão, hashes e caminhos fixos para aquela versão. Não podem ser reutilizados para PKG 015 sem atualizar e revisar explicitamente identidade, paths e escopo.
7. O fluxo anterior não possuía um contrato universal que vinculasse cada registro à versão instalada, PID/processo, fase de inicialização, relógio monotônico, sequência e resultado da operação.

Conclusão CDA inicial: **a coleta anterior é válida para identidade de pacote e estado pontual de arquivos, mas insuficiente para atribuir a causa do erro recorrente.** Não há base para afirmar que o transporte FTP corrompeu os dados. A qualidade/abrangência da instrumentação e a interpretação do log binário são as lacunas demonstradas.

## 3. Modelo de evidência: cinco camadas separadas

Cada investigação deve classificar evidências em camadas independentes:

| Camada | Evidência | O que pode provar |
|---|---|---|
| E0 — Identidade | build run, commit, artefato, PKG, SFO, SHA-256 | qual binário foi produzido/instalado |
| E1 — Transporte | tamanho e SHA-256 local, remoto e readback | integridade da transferência |
| E2 — Estado do console | metadados, diretórios, arquivos, coredumps e timestamps | estado observável no instante da coleta |
| E3 — Execução | eventos de processo, fases, códigos de retorno, sinais, módulo e endereços | até onde a execução realmente chegou |
| E4 — Resultado funcional | comportamento observado, saída GPU, imagem/resultado esperado e comparação | se a função testada realmente funcionou |

Um PASS em uma camada não promove automaticamente as demais. Ex.: E0+E1 PASS não significa E3 ou E4 PASS.

## 4. Protocolo obrigatório de coleta

### 4.1 Preparação e identidade

Antes da coleta, registrar:
- ID da investigação e motivo/gatilho (feedback do usuário ou solicitação explícita);
- TITLE_ID, versão de app, firmware e versão GoldHEN somente quando conhecidos;
- nome, tamanho e SHA-256 esperado do PKG;
- build run, commit exato, artefato e hash do PRX;
- workflow e commit do coletor;
- timestamp UTC do início e fim;
- runner utilizado e resultado do job.

Se algum campo for desconhecido, registrar `UNKNOWN`; nunca inferir.

### 4.2 Aquisição não destrutiva

A coleta live deve usar exclusivamente o runner Windows autorizado `[self-hosted, Windows, X64, ps4-lan]`, depois de revisar o workflow e scripts que serão executados.

Permitido no protocolo read-only:
- FTP LIST, GETSIZE e GET;
- leitura de metadados e arquivos explicitamente listados;
- snapshots separados de arquivos mutáveis;
- cópia para diretório novo e exclusivo por run.

Proibido sem autorização específica:
- FTP PUT, DELETE, RENAME, MKDIR;
- instalar/remover PKG, carregar/ativar PRX, alterar configuração, reiniciar GoldHEN;
- limpar/rotacionar logs, provocar crash, executar probes ou alterar estado de processo.

### 4.3 Preservação e integridade

Para cada objeto remoto, registrar em manifesto:
- caminho remoto exato;
- resultado LIST/GETSIZE/GET e código/erro literal;
- tamanho remoto informado, tamanho local efetivo;
- SHA-256 local;
- hora UTC antes/depois da aquisição;
- se o arquivo pode estar ativo/rotacionando;
- nome do artefato de evidência que o contém.

Regras:
- nunca sobrescrever evidência anterior; cada execução usa diretório novo;
- preservar bytes originais, sem conversão de texto, normalização de newline, descompressão in-place ou edição;
- analisar somente cópias de trabalho;
- para arquivos mutáveis como `log.bin`, fazer dois snapshots independentes quando viável, manter ambos e registrar que hashes diferentes podem refletir rotação/escrita normal;
- comparar tamanho/hash do arquivo remoto e local quando houver mecanismo confiável. FTP GETSIZE isolado não autentica conteúdo remoto; hash da cópia prova apenas o conteúdo recebido;
- registrar falhas e caminhos indisponíveis como evidência, sem descartá-los.

### 4.4 Análise em cópia

Cada parser deve:
- declarar formato/versão de formato suportado e fonte técnica;
- validar limites, comprimentos, offsets, endianess, checksums e truncamento;
- emitir erro explícito para formato desconhecido ou parcial;
- produzir saída estruturada ligada ao hash do arquivo de origem e offset/intervalo de bytes;
- nunca tratar busca de strings como decodificação de formato.

Se não houver parser documentado para `log.bin`, resultado obrigatório: `OPAQUE_UNDECODED`. A extração ASCII/UTF-16 pode ser anexada como pista exploratória, claramente rotulada, mas não pode fundamentar “não houve erro” ou “não existe evento”.

### 4.5 Correlação temporal

Todos os eventos próprios devem ter:
- sequência monotônica crescente por processo;
- timestamp UTC de parede e contador monotônico relativo, se disponíveis;
- identificador de processo/sessão de execução;
- versão/build/commit;
- nome estável do estágio;
- resultado, errno/código e duração quando aplicável.

O relatório deve reconstruir uma timeline, distinguindo relógios e incerteza de sincronização. Não ordenar causalidade apenas por timestamps de arquivos.

## 5. Contrato mínimo de observabilidade do aplicativo

Em futuras alterações de código diagnóstico, o CDA exige um log estruturado, versionado e de baixo risco, sem alterar o comportamento funcional que está sendo investigado:

- cabeçalho: schema_version, build_id, commit, app_version, TITLE_ID, PID e sessão;
- evento: seq, wall_time_utc, monotonic_ticks, stage, event, result, errno, duration;
- marcos mínimos: process entry, logger open/status, manifest validation, PRX hash validation, module load result, cada DLSYM result, preflight outcome e orderly exit;
- caminho persistente gravável comprovado no sandbox do app; falha ao abrir log deve ser exposta em status/telemetria alternativa já autorizada, nunca ocultada;
- escritas completas com tratamento de EINTR/short write, limites de buffer explícitos e sem leitura além do tamanho;
- logger de crash separado do logger normal: handler limitado a operações async-signal-safe, sem alocação, stdio, locks ou parser de contexto presumido;
- registrar códigos e offsets crus quando ABI/contexto não estiver validado; não rotular registradores sem prova;
- não fazer fsync em todo evento sem medir custo e sem justificativa; preferir flush nos marcos necessários, com política documentada;
- rotação/limite de tamanho somente em arquivos próprios do app e sem apagar evidência anterior; qualquer limpeza exige autorização;
- não transmitir logs via UDP por padrão. Transporte remoto só após análise de privacidade, confiabilidade e autorização.

A instalação de nova instrumentação no PS4 continua sujeita às regras de autorização de PKG. Não inserir instrumentação invasiva nem provocar falhas para “testar o logger” sem plano e autorização específicos.

## 6. Diagnóstico causal e classificação

Cada afirmação recebe uma das classificações:
- **OBSERVED:** dado diretamente presente na evidência;
- **CORRELATED:** duas ou mais fontes independentes concordam;
- **INFERRED:** explicação compatível, ainda não demonstrada;
- **HYPOTHESIS:** possibilidade a testar;
- **CONFIRMED ROOT CAUSE:** mecanismo demonstrado com evidência reproduzível e eliminação de alternativas plausíveis;
- **UNKNOWN / INSUFFICIENT EVIDENCE:** dados não permitem conclusão.

A causa raiz só é CONFIRMED quando:
1. há evidência íntegra ligada ao build/execução exatos;
2. o mecanismo explica o sintoma e a sequência observada;
3. a hipótese gera uma previsão testável;
4. teste/reprodução confirma a previsão ou uma correção mínima elimina o sintoma sem relaxar gates;
5. não há contradição material não resolvida.

Correlação temporal, ausência de coredump, PASS de DLSYM, strings de log ou teste host isolado nunca são suficientes sozinhos.

## 7. Revisão e gates independentes

Papéis funcionais:
- CDA: metodologia, integridade, síntese causal e decisão diagnóstica;
- coleta/forense: aquisição e preservação;
- parser/análise: decodificação e correlação;
- engenharia responsável: correção mínima;
- QA independente: verifica correção e regressões;
- auditor independente: contesta evidências, pressupostos e conclusão.

A mesma pessoa/função que implementa uma correção não pode ser descrita como revisão independente dela. Ferramentas automatizadas são QA automatizado, não agente humano independente. Se não houver revisor externo real, declarar `INDEPENDENT_HUMAN_REVIEW=NOT_PERFORMED`.

Gates:
- **G0 — coleta confiável:** cadeia de custódia e integridade adequadas;
- **G1 — identidade correlacionada:** PKG/build/commit correspondem;
- **G2 — execução observável:** fase da falha determinada ou limitação explicitada;
- **G3 — causa:** confirmada ou hipótese explicitamente mantida;
- **G4 — correção:** teste direcionado e regressões passam;
- **G5 — release:** QA, pacote e hardware separados; nenhuma aprovação por inferência.

Se G0 ou G1 falhar, bloquear diagnóstico causal e novo PKG corretivo. Se G2 falhar, priorizar observabilidade/coleta, não mudanças especulativas no runtime. Se G3 não estiver confirmado, qualquer experimento deve ser rotulado e isolado, nunca vendido como correção.

## 8. Relatório obrigatório por investigação

Cada relatório deve conter:
1. resumo do sintoma e feedback literal do usuário;
2. pergunta diagnóstica;
3. identidade completa de build/PKG/console;
4. inventário de fontes e arquivos, incluindo ausências;
5. manifesto de integridade e cadeia de custódia;
6. timeline;
7. parsers e versões utilizados, incluindo limitações;
8. fatos OBSERVED/CORRELATED;
9. hipóteses e alternativas descartadas, com justificativa;
10. causa e nível de confiança;
11. correção proposta/realizada e diff;
12. testes executados e resultados reais;
13. gates aprovados/bloqueados;
14. ação seguinte mínima e autorização necessária.

## 9. Disposição inicial do CDA para PKG 015

PKG 015 está staged, não instalado segundo a evidência registrada. É um preflight de resolução de sete exports GNM; não executa chamadas GNM nem GPU dispatch. Não iniciar coleta pós-instalação até o usuário informar que instalou/executou e fornecer o resultado visível. Após esse feedback, atualizar um workflow versionado específico para PKG 015, revisar seu código antes do runner e fazer aquisição read-only. Não reutilizar silenciosamente os workflows PKG 014.

Até haver evidência E3 suficiente, não gerar PKG adicional apenas para repetir o ciclo. O primeiro objetivo é demonstrar que a coleta e a instrumentação conseguem capturar a execução real e que os parsers não confundem ausência de texto com ausência de evento.

## 10. Regra de encerramento

Nenhuma investigação é encerrada com “resolvido” sem causa e verificação. Se o dado não existir ou não for acessível, registrar exatamente o limite e a próxima observação necessária. A incerteza documentada é um resultado válido; um PASS inventado não é.
