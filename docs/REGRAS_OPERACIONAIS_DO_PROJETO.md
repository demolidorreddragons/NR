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
