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

## Compromisso de entrega

A prioridade é fazer o projeto rodar de verdade no PS4 Pro. Cada etapa deve aproximar o repositório desse resultado mensurável; quando isso não for possível, o bloqueio deve ser descrito com precisão e acompanhado do próximo passo concreto.
