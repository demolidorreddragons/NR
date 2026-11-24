# Objetivo da colaboração — Neural Rendering

## Objetivo principal

Meu objetivo nesta colaboração é conduzir o projeto **Neural Rendering** até uma implementação funcional e verificável no **PS4 Pro com firmware 14.02** — não apenas produzir documentação, compilar exemplos ou manter uma sequência de auditorias.

O resultado que buscamos é executar trabalho de compute real na GPU do console, comprovar a correção por meio de readback determinístico e, a partir dessa base, integrar o pipeline de reconstrução/renderização neural ao runtime do projeto.

## Critério de sucesso final

O objetivo só será considerado alcançado quando houver evidência reproduzível, obtida no hardware-alvo, de que:

1. O runtime inicializa corretamente no PS4 Pro / FW 14.02.
2. O backend GNM utiliza contratos de firmware comprovados para memória, endereçamento GPU, descritores, submissão e sincronização.
3. Um compute shader é efetivamente submetido e executado pela GPU.
4. O resultado escrito pela GPU é lido de volta e comparado com uma saída esperada, de forma determinística.
5. O pipeline neural é integrado e validado no caminho de renderização pretendido.

Compilação, testes de host, análise estática, artefatos de CI e comportamento de emulador são evidências úteis, mas **não substituem a execução no PS4 Pro**.

## Caminho de entrega

- **Gate A — compilação e artefatos GCN8:** validar shader, formato, parser, compilador e artefatos.
- **Gate B — prontidão do backend GNM:** comprovar ABI do FW 14.02, GPU VA/residência, descritores, submissão, sincronização e readback.
- **Gate C — execução em hardware:** executar um compute mínimo, determinístico e reproduzível no PS4 Pro.
- **Gate D — integração neural:** conectar os kernels e o grafo neural ao pipeline de reconstrução/renderização e validar qualidade e desempenho.
- **Entrega:** consolidar testes, documentação operacional e pacote instalável somente depois das validações correspondentes.

## Regras de execução

- Priorizar mudanças concretas que removam bloqueios: código, testes, correções, evidências e integração.
- Não repetir pesquisas ou auditorias já concluídas sem uma nova hipótese ou evidência.
- Não criar commits cosméticos nem documentação que apenas dê aparência de progresso.
- Não declarar um gate concluído sem cumprir seus critérios e registrar as evidências.
- Não apresentar CI de host, emulador ou análise estática como execução em hardware.
- Não inventar contratos de ABI, endereços GPU, resultados, logs ou execuções.
- Respeitar os limites de autorização do usuário: nenhuma carga de módulo, chamada GNM, alocação/submissão GPU, ativação de PRX, instalação de PKG ou alteração do console sem a validação e autorização aplicáveis.
- Quando uma dependência externa impedir a execução, identificar precisamente o artefato, acesso ou resultado necessário, sem alegar progresso em background.

## Estado atual

O Gate A possui aprovação baseada em artefatos. O Gate B permanece **aberto**: ainda faltam provas qualificadas para FW 14.02 e evidências de execução/readback no hardware-alvo. O projeto não deve ser descrito como um Neural Renderer funcional enquanto esses critérios e os gates posteriores não forem satisfeitos.



## Plano de execução otimizado

A partir desta revisão, o trabalho será organizado por **entregáveis funcionais**, e não por quantidade de relatórios, auditorias ou commits. Uma investigação só será aberta quando responder a uma hipótese técnica que bloqueie a próxima implementação. Resultados já estabelecidos não serão pesquisados novamente sem evidência nova.

### Ordem de execução

1. **Fechar a descoberta segura dos provedores GNM.** O smoke test deve apenas enumerar módulos já carregados e identificar separadamente o provedor normal e o Neo. Não pode carregar módulos como fallback. Uma enumeração incompleta ou ABI não qualificada deve interromper o caminho de forma explícita.
2. **Qualificar o contrato mínimo do FW 14.02.** Obter evidência específica do firmware para enumeração de módulos, informações dos módulos e resolução de símbolos, incluindo estrutura, tamanhos, convenção de chamada e provedores. Não usar contratos de FW 1.76/9.00, NIDs de títulos ou HLE como substitutos.
3. **Provar endereçamento e memória GPU.** Determinar, com evidência específica do alvo, GPU VA, mapeamento, tipo de memória, residência, alinhamento, visibilidade de cache e vida útil dos recursos. Endereço virtual de CPU e offset de direct memory não serão tratados como GPU VA por suposição.
4. **Estabelecer o caminho mínimo de submissão.** Confirmar estado compute inicial, descritores, user data, formato/tamanho dos command buffers, fila/submissão e sincronização. Corrigir o backend e adicionar testes focados somente quando o contrato estiver sustentado.
5. **Executar um único teste vertical de ponta a ponta.** Usar o kernel determinístico `buffer_add_one`: 64 entradas, dispatch real, fence/espera válida e comparação de todos os 64 resultados. O critério é readback correto no PS4 Pro, não apenas pacote compilado ou log de submissão.
6. **Substituir o smoke por trabalho neural real.** Integrar primeiro um kernel pequeno e mensurável ao grafo de reconstrução/renderização; depois ampliar o pipeline e medir qualidade, latência e estabilidade.
7. **Empacotar e entregar.** QA e PKG final somente após execução e integração comprovadas.

### Política de foco e decisão

- Prioridade máxima: remover o bloqueio que impede o primeiro dispatch real e o readback.
- Cada ciclo deve terminar em uma destas saídas: código funcional com teste, evidência nova que fecha um requisito, ou bloqueio externo preciso com o artefato mínimo necessário.
- Não abrir novas frentes de documentação, inventários ou auditorias paralelas quando não alterarem a próxima decisão de engenharia.
- Não repetir Gate A: ele já passou em nível de artefatos. Só reabrir se houver mudança real no compilador, shader, parser ou configuração de build.
- Não habilitar compute experimental, carregar módulos, chamar GNM, alocar/submeter GPU ou alterar o console enquanto os contratos e autorizações aplicáveis não estiverem satisfeitos.
- Não considerar uma PR, CI de host, análise estática, emulador ou geração de PKG como avanço equivalente à execução GPU no hardware.
- Medida principal de progresso: distância reduzida até o teste vertical de dispatch + readback; depois, distância até o primeiro kernel neural integrado.

### Primeiro incremento concreto já iniciado

A PR [#67 — Make compute smoke GNM discovery passive-only](https://github.com/demolidorreddragons/NR/pull/67) remove o fallback que carregava o módulo GNM automaticamente, exige descoberta passiva de um provedor normal já carregado, registra a presença do provedor Neo separadamente e falha quando a lista de módulos está incompleta. É uma correção de segurança no código-fonte; **não** comprova ABI FW 14.02 nem execução no console.

### Dependência externa que realmente destrava o hardware

O próximo avanço de hardware depende de um canal aprovado para obter a evidência passiva do processo no PS4 Pro e qualificar as APIs de enumeração/resolução no FW 14.02. Sem isso, não há base segura para escolher o provedor ou chamar suas funções. Quando esse insumo estiver disponível, a sequência é direta: censo passivo normal/Neo → qualificação de ABI → memória GPU → dispatch mínimo → readback. Não será criado outro relatório intermediário sem produzir ou consumir esse insumo.

## Compromisso de entrega

A prioridade é fazer o projeto rodar de verdade no PS4 Pro. Cada etapa deve aproximar o repositório desse resultado mensurável; quando isso não for possível, o bloqueio deve ser descrito com precisão e acompanhado do próximo passo concreto.
