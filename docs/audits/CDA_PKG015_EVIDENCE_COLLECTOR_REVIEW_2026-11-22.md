# CDA — revisão do coletor PKG 015

**Data:** 2026-11-22  
**Escopo:** preparação e revisão estática de coleta forense pós-feedback para PKG 015  
**Estado:** workflow criado; não executado; nenhuma conexão FTP/PS4 realizada

## Identidade de referência

- Repositório: `demolidorreddragons/NR`
- Branch: `feature/gcn8-numeric-kernels`
- PKG: `PS4-Neural-Engine-015.pkg`
- Tamanho: 6.619.136 bytes
- SHA-256: `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`
- Build run: `45074310271`
- Commit de build: `2c9e3c12aed2791f2973496c31c8993259c64430`
- APP_VER: `01.10`
- TITLE_ID: `PNEU00001`

## Alterações

1. Criado `.github/workflows/cda-pkg015-readonly-evidence.yml`.
2. O workflow usa somente `workflow_dispatch`, com dois campos obrigatórios: resultado literal observado e referência do feedback. Não há gatilho `push`; portanto, commits no branch não iniciam coleta live.
3. O job exige o runner `[self-hosted, Windows, X64, ps4-lan]` e a variável `PS4_IP`; falha fechado se a variável não existir.
4. Operações remotas limitadas a FTP `LIST`, `GETSIZE` e `GET`. Não há PUT, DELETE, RENAME, MKDIR, instalação, ativação de PRX ou alteração de configuração.
5. Diretório local exclusivo por run; arquivos recebem identificador derivado do SHA-256 do caminho remoto para evitar colisões causadas por achatamento de diretórios.
6. Preserva snapshots binários separados de `/data/sce_logs/log.bin`, com GETSIZE antes/depois, tamanho local, SHA-256 local e horários UTC. Divergência de tamanho é marcada como instabilidade; hashes diferentes entre snapshots não são interpretados automaticamente como corrupção.
7. Não tenta decodificar `log.bin` nem extrair strings. O manifesto registra explicitamente `OPAQUE_UNDECODED`.
8. O manifesto associa a coleta ao PKG/build esperados e preserva erros/arquivos indisponíveis.

## Revisão estática e correção

- Revisados os limites do runner, variáveis, diretórios, operações FTP, nomes locais, preservação de falhas e saída de artefato.
- Corrigida a geração da chave SHA-256 de caminho para compatibilidade com Windows PowerShell 5.1 (`BitConverter`), evitando depender de `Convert.ToHexString` do .NET moderno.
- O workflow foi mantido manual para não executar coleta no PS4 apenas porque um commit foi enviado.
- A identidade de PKG 015 está fixa intencionalmente neste workflow versionado. Não usar para outro pacote sem nova revisão.

## Limitações e gates

- **G0:** preparação estática concluída; coleta real ainda não executada.
- **G1:** identidade de referência documentada; identidade atualmente instalada no console ainda precisa ser coletada e comparada.
- **G2:** bloqueado até feedback do usuário e aquisição read-only.
- **G3/G4/G5:** bloqueados; não há evidência nova de execução, causa raiz, correção do runtime ou validação de hardware.
- Nenhuma alegação de parser para `log.bin`, execução do app, sucesso funcional ou correção do PKG é feita.
- Nenhuma revisão humana independente foi realizada.

## Resultado

O próximo passo técnico ficou preparado sem tocar no console: depois que o usuário informar o resultado real da instalação/execução do PKG 015, revisar novamente este workflow e acioná-lo manualmente. Até lá, não gerar outro PKG corretivo nem tratar esta melhoria do coletor como correção do runtime.
