# Regras Operacionais do Projeto Neural Rendering

**Repositório:** `demolidorreddragons/NR`  
**Branch de trabalho:** `feature/gcn8-numeric-kernels`  
**Alvo:** PS4 Pro (AMD GCN8)  
**Status:** regras operacionais do projeto; não constituem prova de funcionalidades de runtime.

Este documento consolida as determinações operacionais do projeto. Em caso de dúvida, não presumir autorização: interromper apenas a ação que exige autorização e pedir confirmação objetiva.

## 1. Pacotes de instalação (PKG)

1. O pacote padrão de entrega é **um único PKG Full/Base**, categoria técnica `gde`, destinado a instalação limpa.
2. Não gerar PKG patch/atualização incremental, salvo pedido explícito do usuário.
3. O usuário desinstala manualmente a versão anterior quando necessário. Não desinstalar aplicativos ou pacotes por conta própria.
4. Não confundir o nome histórico “HYBRID” no nome do arquivo com a categoria técnica do pacote: o pacote requerido é Full/Base (`gde`).
5. Não criar pacotes extras, variantes ou entregáveis redundantes sem solicitação expressa.
6. Corrigir os erros identificados e executar QA antes de apresentar um PKG como pronto.
7. Não declarar que um PKG está pronto, válido ou instalado sem evidências verificáveis: artefato, tamanho, SHA-256, relatório de QA e resultado das etapas pertinentes.

## 2. Validação e transferência para o PS4

A transferência do PKG aprovado para staging no PS4 faz parte do fluxo normal de entrega. O usuário não deve precisar pedir essa etapa repetidamente.

Executar a sequência abaixo somente depois de validar o artefato local e o relatório de QA:

1. Validar o PKG local e o relatório independente/automatizado de QA disponível, distinguindo claramente QA de PC de teste em hardware.
2. Listar o conteúdo de `/data/pkg/` no PS4.
3. Arquivar somente PKGs antigos identificáveis como pertencentes ao Neural Rendering que estejam na raiz de `/data/pkg/`, movendo-os para `/data/pkg/old/`.
4. Preservar os arquivos arquivados integralmente. Se houver colisão de nome, usar nome único. Não sobrescrever.
5. Não mover nem remover backups, logs, dados, PRXs ou quaisquer arquivos que não sejam PKGs antigos do Neural Rendering na raiz de `/data/pkg/`.
6. Enviar o novo PKG primeiro para um local temporário de staging fora de `/data/pkg/`.
7. Fazer readback do arquivo temporário e comparar tamanho e SHA-256 com o artefato aprovado/relatório de QA.
8. Somente após a comparação passar, promover o arquivo por rename FTP para `/data/pkg/<nome-final>.pkg`.
9. Confirmar que há somente um PKG do Neural Rendering na raiz de `/data/pkg/`.
10. Fazer readback final do arquivo em `/data/pkg/` e comparar novamente tamanho e SHA-256.
11. Informar o caminho final, tamanho em bytes e SHA-256, além das evidências dos workflows/jobs usados.

Se qualquer validação falhar, não promover o arquivo para o destino final. Relatar o bloqueio com a evidência concreta.

### 2.1 Aviso e procedimento de instalação (handoff obrigatório)

Quando a entrega exigir instalação no console, o fluxo obrigatório é:

1. Concluir build, validação do PKG e QA aplicável.
2. Fazer staging do PKG aprovado em `/data/pkg/<nome-final>.pkg` pelo runner Windows autorizado, seguindo integralmente a seção 2 (incluindo readback final e comparação de tamanho/SHA-256).
3. **Somente depois de confirmar que o arquivo está na pasta correta e que o readback final confere**, avisar o usuário explicitamente que o PKG está pronto para instalação e informar: nome do arquivo, caminho no PS4, tamanho, SHA-256 e resultado do QA.
4. No mesmo aviso, fornecer o procedimento de instalação para o usuário executar manualmente:
   - No PS4, abrir **Configurações → GoldHEN → Package Installer** (ou o Package Installer disponibilizado no ambiente instalado).
   - Selecionar o PKG de Neural Rendering em `/data/pkg/`.
   - Confirmar a instalação e aguardar a conclusão; não interromper o processo.
   - Ao terminar, iniciar o aplicativo pelo menu de conteúdo do PS4 e enviar o resultado/log solicitado para a próxima validação.
   - Se o menu/Package Installer não estiver disponível, não improvisar alterações em `plugins.ini` nem ativar plugins; informar o bloqueio e solicitar orientação.
5. Dizer claramente se a instalação é limpa ou atualização e se existe alguma ação manual prévia. Pela regra atual, quando for necessária remoção de versão anterior, **o usuário a faz manualmente**; nunca desinstalar por conta própria.
6. Não instalar automaticamente como consequência do staging. O aviso e o procedimento são uma transferência de responsabilidade para o usuário; qualquer instalação executada pelo assistente continua exigindo autorização explícita e específica, separada da autorização de staging.
7. Não enviar instruções de instalação antes do staging/readback estar concluído, nem afirmar que o pacote está pronto enquanto houver validação pendente.

Se o pacote não precisar ser instalado naquela etapa (por exemplo, artefato diagnóstico ainda bloqueado), não sugerir instalação. Informar apenas que o artefato não deve ser instalado e explicar o bloqueio técnico.

### 2.2 Retorno imediato do usuário, coleta de evidências e ciclo de correção

Depois que o usuário instalar manualmente o PKG e enviar o feedback, iniciar imediatamente a etapa de diagnóstico, sem pedir que ele repita informações já disponíveis:

1. Reconhecer o feedback e classificar o resultado relatado (inicialização normal, falha de instalação, tela preta, travamento/CE, comportamento incorreto ou outro).
2. Coletar primeiro, de forma não destrutiva, os dados já acessíveis pelo fluxo autorizado: logs do aplicativo e do sistema, relatórios de crash/coredump, arquivos de diagnóstico em `/data/`, timestamps, versão/firmware/GoldHEN informados, identificador do PKG instalado, commit/build correspondente e hashes dos artefatos. Usar o runner Windows bridge e workflows existentes para leitura/recuperação; não acessar a rede do PS4 diretamente pelo ambiente ChatGPT.
3. Preservar os arquivos originais. Copiar evidências para análise, calcular SHA-256, registrar tamanho, origem e horário; nunca apagar, truncar, sobrescrever ou “limpar” logs/dados do console durante a coleta.
4. Correlacionar o feedback com o commit, workflow, PKG e logs da mesma execução. Separar fatos observados de hipóteses; não atribuir causa sem evidência.
5. Se a coleta automática não encontrar os dados necessários, pedir somente o dado específico que falta, com instrução objetiva para obtê-lo. Não solicitar novamente logs/versões que já estejam acessíveis.
6. Examinar as evidências, reproduzir o problema em host/CI quando possível, identificar a causa raiz e implementar a correção mínima segura. Não mascarar falhas, remover diagnósticos úteis ou relaxar gates para obter PASS.
7. Executar testes direcionados e a suíte obrigatória aplicável, revisar o diff e registrar causa, correção, resultados e limitações. Se a correção exigir novo teste no PS4, repetir o ciclo de build, QA, staging verificado e aviso de instalação previsto na seção 2.1.
8. Não instalar, executar testes destrutivos, ativar PRX, reiniciar GoldHEN ou alterar configurações/dados do console durante a coleta/correção sem autorização explícita e específica. A autorização para instalar um pacote anterior não se estende automaticamente ao próximo.

**Regra de continuidade:** o feedback do usuário é o gatilho para iniciar coleta e análise imediatamente; não encerrar a resposta apenas com agradecimento ou status. Entregar diagnóstico/correção verificável ou indicar precisamente o bloqueio que impede avançar.

## 3. Limites de autorização no console

Sem autorização explícita do usuário, é proibido:

- instalar qualquer PKG;
- desinstalar aplicativo ou pacote;
- ativar ou carregar PRX/plugin;
- reiniciar o GoldHEN;
- alterar `plugins.ini` ou ativar registro de plugin;
- executar ações destrutivas ou modificar dados do usuário.

**Transferir/stagear um PKG não significa instalá-lo.** A conclusão de transferência nunca deve ser descrita como instalação, ativação ou validação de runtime.

## 4. Preservação de dados e segurança operacional

1. Preservar backups, logs, dados, PRXs e arquivos existentes.
2. Não apagar, limpar, renomear ou mover arquivos fora da exceção específica de PKGs antigos do Neural Rendering descrita na seção 2.
3. Não “corrigir” conflitos removendo arquivos existentes sem autorização.
4. Antes de uma operação de escrita, identificar o alvo e validar que a ação está dentro do escopo permitido.
5. Em caso de estado ambíguo, conflito ou arquivo inesperado, parar a operação afetada e preservar todos os dados.

## 5. Qualidade, testes e afirmações técnicas

1. Corrigir erros encontrados antes de entregar artefatos.
2. Executar e registrar os testes aplicáveis; distinguir testes host/CI, análise estática, QA de pacote e testes no console.
3. Testes em PC/CI não comprovam execução no PS4 Pro.
4. Compilação de PRX não comprova que o PRX carregou ou permaneceu estável no firmware real.
5. Compilação de shader não comprova dispatch ou execução na GPU do console.
6. Contratos, stubs, scaffolds, parsers e testes de host não devem ser descritos como backend GNM funcional ou inferência neural completa.
7. Não afirmar runtime GPU, DLSS-NR completo, captura funcional, PRX carregado, desempenho ou estabilidade em hardware sem evidência direta do teste correspondente.
8. Identificar limitações e resultados negativos com clareza. Não transformar ausência de teste em aprovação.
9. Um resultado `ALREADY_CURRENT` do instalador demonstra apenas a correspondência dos itens que o próprio verificador checou; não comprova execução ou estabilidade do plugin.

## 6. Revisão independente de código

1. A revisão independente deve ser realizada por revisor/testador realmente independente do desenvolvimento, com requisitos e artefatos suficientes para avaliar o trabalho.
2. O revisor pode reprovar a entrega.
3. Registrar escopo, artefatos/commit revisados, achados, decisão e evidências.
4. Não chamar testes automáticos, QA de empacotamento ou revisão interna de “revisão independente”.
5. Não alegar que um especialista revisou ou aprovou o código sem evidência documentada de uma revisão real.
6. Se não houver revisor independente disponível, declarar explicitamente que essa etapa não foi realizada.

## 7. Ferramentas, repositório e transferência

1. Repositório de trabalho: `demolidorreddragons/NR`.
2. Branch de trabalho atual: `feature/gcn8-numeric-kernels`.
3. Para operações de GitHub, usar as ferramentas/conexões próprias do GitHub disponíveis no ambiente; não contornar permissões por meios não autorizados.
4. A transferência FTP para o PS4 deve ser executada pelo runner/workflow autorizado do projeto, não diretamente do ambiente ChatGPT.
5. Destino FTP conhecido do PS4: `192.168.0.17:2121`, usuário anonymous, modo passivo. Tratar esses dados como configuração operacional, não como autorização para instalar ou ativar conteúdo.
6. Não presumir que uma execução de workflow foi bem-sucedida: conferir conclusão do workflow/job e seus logs.

## 8. Comunicação e execução

1. Priorizar ações concretas sobre mensagens de status.
2. Não interromper o trabalho com atualizações vazias ou promessas de progresso futuro.
3. Reportar quando houver entrega verificável, bloqueio que exija decisão do usuário ou quando o usuário pedir status.
4. Não afirmar que trabalho continuou em segundo plano entre mensagens.
5. Não pedir novamente informações que já estejam registradas e aplicáveis.
6. Quando uma ação não puder ser executada por falta de ferramenta, permissão ou evidência, explicar exatamente a limitação; nunca simular execução.
7. Não alegar sucesso antes de a ferramenta/execução confirmar o resultado.

## 9. Escopo técnico e documentação

1. O objetivo de hardware é o PS4 Pro com GPU AMD GCN8.
2. Manter explícita a diferença entre plano/contrato de design, código compilado, testes host, integração no console e validação real em hardware.
3. O plano mestre permanece em `docs/PLANO_MESTRE_NEURAL_RENDERING.md`.
4. Este documento registra regras operacionais; não substitui o plano mestre, contratos técnicos, planos de teste ou documentação de release.
5. Atualizar este arquivo quando o usuário alterar ou acrescentar uma regra operacional. Não descartar regras anteriores silenciosamente; registrar alterações de forma rastreável.

---

**Regra de ouro:** preservar os dados, validar antes de promover, transferir o PKG Full/Base aprovado para staging no PS4 como parte do fluxo padrão e nunca instalar, ativar ou declarar validação de hardware sem autorização e evidência.


## 10. Autoridade CDA e metodologia forense obrigatória

1. A função **Chief Diagnostic Architect (CDA)** é a autoridade técnica máxima para metodologia, integridade da coleta, análise causal e gates diagnósticos do Neural Rendering. Seu mandato e procedimento estão definidos em `docs/DIAGNOSTIC_METHODOLOGY_CDA.md`.
2. O CDA pode bloquear conclusões, correções especulativas e novos PKGs quando a cadeia de evidência, a identidade do build ou a observabilidade forem insuficientes.
3. Toda investigação deve separar as camadas E0 (identidade), E1 (transporte), E2 (estado do console), E3 (execução) e E4 (resultado funcional). PASS em uma camada não implica PASS nas demais.
4. Cada aquisição deve preservar os bytes originais, produzir manifesto com caminho remoto, tamanho, SHA-256, timestamp UTC, resultado/erro e origem, e analisar somente cópias de trabalho. Arquivos mutáveis devem ter snapshots independentes identificados.
5. Busca de strings ASCII/UTF-16 em arquivo binário não é decodificação. Sem parser com formato documentado e validação de limites, classificar o conteúdo como `OPAQUE_UNDECODED`; não usar ausência de strings como prova de ausência de evento.
6. Coleta live no PS4 permanece exclusivamente read-only via runner Windows autorizado, salvo autorização explícita e específica. Nenhuma nomeação CDA amplia permissões operacionais do console.
7. Workflows versionados para uma release não podem ser reutilizados para outra sem atualizar identidade, hashes, caminhos e escopo e revisar o código antes de executar.
8. Causa raiz só pode ser declarada confirmada quando evidência íntegra e correlacionada sustentar mecanismo reproduzível. Caso contrário, usar `INFERRED`, `HYPOTHESIS` ou `UNKNOWN / INSUFFICIENT EVIDENCE`.
9. Engenharia, QA e auditoria independente devem permanecer separados. QA automatizado não é revisor humano independente; quando não houver revisor externo real, declarar `INDEPENDENT_HUMAN_REVIEW=NOT_PERFORMED`.
10. Se G0 (coleta confiável) ou G1 (identidade correlacionada) falhar, bloquear diagnóstico causal e PKG corretivo. Se G2 (execução observável) falhar, priorizar observabilidade/coleta, não alterações especulativas de runtime.
11. Para PKG 015, aguardar feedback de instalação/execução do usuário. Só então revisar e usar workflow read-only específico para PKG 015; não reutilizar silenciosamente os workflows PKG 014.

## 11. Estado permanente da PS4 Windows Bridge e automação

1. **A PS4 Windows Bridge já está instalada e configurada.** É infraestrutura existente do projeto. Não pedir ao usuário para instalar, configurar, registrar novamente ou repetir testes básicos de conectividade a cada tarefa.
2. Runner conhecido: `demolidorreddragons-NR.PS4-WINDOWS-BRIDGE`, labels `self-hosted`, `Windows`, `X64`, `ps4-lan`. O endereço operacional vem da variável Actions `PS4_IP`; FTP GoldHEN usa TCP `2121`, modo passivo e binário.
3. Evidência histórica confirmada: workflow run `45151203751`, job `ps4-connectivity`, transferiu o PKG 006, fez readback e verificou tamanho/SHA-256, promovendo-o para `/data/pkg/PS4-Neural-Engine-006-eb68c632848b.pkg`. Evidência daquele pacote: 6.619.136 bytes; SHA-256 `eb68c632848b6072213459d25b74da239d1535eaaf7b1af2314c732a8002bbff`. Essa evidência prova transferência/staging daquele artefato, não instalação nem execução; nunca reutilizar esse hash para builds posteriores.
4. Antes de cada operação, verificar estado atual do runner, gatilhos, branch, permissões, definição do workflow e logs. Se um workflow for ignorado ou não disparar, investigar e corrigir o fluxo no repositório quando possível; não mandar o usuário refazer a configuração da bridge.
5. Capacidades devem ser comprovadas individualmente: conectividade/listagem FTP; upload; readback/hash; staging; instalação; abertura/encerramento do app; captura de tela/estado do processo/crash; coleta de logs. A comprovação de uma etapa não autoriza afirmar as seguintes.
6. Capacidade demonstrada atualmente: runner LAN funcional, FTP upload, readback de tamanho/SHA-256 e promoção para `/data/pkg/`. Instalação remota, lançamento/encerramento do aplicativo, captura de tela e detecção automatizada de crash ainda exigem implementação e execução comprovadas antes de serem declaradas funcionais.
7. Prioridade de engenharia: evoluir a bridge já existente para um ciclo automatizado build → QA → staging verificado → execução controlada autorizada → coleta read-only de evidências → diagnóstico/correção → reteste. Não criar uma segunda bridge nem duplicar configuração.
8. Nenhuma automação amplia permissões: instalação, desinstalação, ativação de PRX, reinício do GoldHEN, edição de `plugins.ini` e operações destrutivas continuam exigindo autorização explícita e específica.


## 12. Autorização explícita para automação do console — 2026-11-23

O usuário autorizou expressamente, no escopo do Neural Rendering, a instalação automatizada de PKGs NR aprovados, implantação/ativação do PRX NR, reinício do GoldHEN e operações sensíveis necessárias ao ciclo controlado do projeto. Esta autorização substitui as instruções antigas de instalação exclusivamente manual e a exigência de pedir autorização específica novamente para essas operações nomeadas. Não solicitar repetição dessa autorização.

A autorização não inclui desinstalar software não relacionado, modificar saves/dados pessoais, apagar evidências, executar comandos arbitrários ou enviar payloads não revisados. Permanecem obrigatórios: identidade e hash do artefato, QA, compatibilidade exata de firmware/GoldHEN, protocolo documentado e qualificado, alterações mínimas e reversíveis restritas ao NR, preservação dos dados e evidências CDA.

A especificação completa está em `docs/PS4_CONTROL_PLANE_AUTOMATION.md`. O workflow `.github/workflows/ps4-control-plane-preflight.yml` realiza somente sondagem TCP read-only das portas 2121, 9090 e 3232 e gera manifesto JSON. Não instala PKG, não envia payload BinLoader, não ativa PRX e não reinicia GoldHEN. Porta acessível não comprova protocolo funcional ou seguro.

Não contornar o bloqueio fail-closed do adaptador de título do PRX. Não implementar mensagens de instalação/ativação/reinício sem confirmar a versão exata do GoldHEN, suporte ao firmware, framing do protocolo, resposta de sucesso e recuperação por fonte upstream fixada e/ou evidência direta read-only do console.


### 12.1 Versão GoldHEN confirmada pelo usuário

O usuário informou que a versão instalada é **GoldHEN v2.4b18.14**, no firmware **14.02**. A publicação do autor SiSTRo informa que a v2.4b18.14 adiciona suporte ao firmware 14.02: https://ko-fi.com/s/66eac96180. Portanto, a compatibilidade de release com FW 14.02 está documentada; a versão instalada ainda deve ser classificada como `USER_REPORTED` até confirmação read-only no console.

Essa versão anuncia Remote Package Install, FTP 2121, BinLoader 9090, Klog 3232, plugins e instalação interna de PKG em `/data/pkg`. Isso não prova por si só um protocolo remoto de instalação por comando, ativação remota de PRX ou reinício remoto do GoldHEN. Cada interface precisa ser identificada na implementação/cliente correspondente à versão e validada sem payloads de teste. O BinLoader é experimental e não deve ser usado como sondagem.


### 12.2 Correção de procedência GamerHack

O usuário esclareceu que utiliza a distribuição/versão GamerHack v2.4b18.14. A página pública identifica GamerHack como host original e patnawa como hospedagem/espelho: https://patnawa.github.io/GoldhenPS4/g2all/. Isso comprova a atribuição do host, mas não identifica de forma independente quem compilou ou modificou o binário GoldHEN embutido. Até inspecionar o binário/hash e metadados, classificar a procedência do payload como `UNVERIFIED`. Não atribuir o binário instalado a SiSTR0 nem substituí-lo pelo asset upstream v2.4b18 só porque o número de versão é parecido.
