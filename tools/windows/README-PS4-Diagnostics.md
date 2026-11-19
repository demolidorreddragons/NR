# Coletor de diagnóstico PS4 para Windows

Script portátil em PowerShell; não instala programas nem altera arquivos no PS4.

## Uso
1. Baixe o arquivo PS4-Diagnostics.ps1.
2. Abra PowerShell na pasta onde salvou.
3. Execute: powershell -NoProfile -ExecutionPolicy Bypass -File .\PS4-Diagnostics.ps1
4. Teste único: acrescente -Once ao comando.

Padrões: PS4 192.168.0.17, FTP 2121. Arquivos em %USERPROFILE%\PS4NeuralEngine-Diagnostics.

O coletor tenta ler /data/PS4NeuralEngine, /data/PS4NeuralEngine/logs e /data/PS4NeuralEngine/captures. Não obtém logs internos do sistema de instalação se o FTP GoldHEN não os expuser. A listagem FTP espera nomes sem espaços.

Não faz upload ao GitHub e não contém tokens. Não exponha a porta FTP do GoldHEN à internet; use somente na rede local confiável.
