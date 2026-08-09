# Capítulo 7 — Referências Pendentes

> Pontos que **ainda precisam de fundamentação bibliográfica**. Não inventar referências: indicar
> assunto, criticidade e tipo de fonte. As referências **já existentes e válidas** (datasheets,
> papers, normas dos cap. 03–08) estão consolidadas em `07_referencias.md`.

| # | Assunto | Criticidade | Tipo de fonte recomendada |
|---|---|---|---|
| 1 | Arquitetura modular / engenharia de software (acoplamento, coesão, extensibilidade) | **Alta** | Livros de eng. de software; padrões de arquitetura de firmware/HAL; artigos |
| 2 | Consumo e autonomia (substituir números especulativos do cap. 02) | **Alta** | App notes de baixo consumo; documentação ESP-IDF (sleep modes); medições próprias |
| 3 | Arduino × ESP-IDF (hoje opinião como fato no cap. 01) | **Alta** | Documentação oficial Arduino e ESP-IDF; artigos comparativos |
| 4 | Mercado/evolução de wearables (cap. 01 §6.1) | **Alta** | Relatórios de mercado (IDC/Statista/Gartner); artigos de revisão |
| 5 | FreeRTOS e modelo de concorrência (mutex, inversão de prioridade, idle hook) | **Alta** | Documentação oficial FreeRTOS; livros de RTOS; artigo clássico de inversão de prioridade |
| 6 | Protocolo I²C e integridade de sinal/pull-ups (cap. 02 §5) | **Alta** | Especificação I²C (NXP UM10204); app notes NXP |
| 7 | Trabalhos relacionados / estado da arte | **Alta** | TCCs/dissertações/teses; wearables open-source; plataformas modulares IoT |
| 8 | ESP32-C6 / RISC-V | Média-Alta | TRM e datasheet Espressif; documentação RISC-V |
| 9 | Protocolos 1-Wire e SPI | Média | Documentação Maxim (1-Wire); datasheet do controlador (SPI) |
| 10 | Definição de "wearable" / vestível | Média | Artigos de revisão; livros de computação vestível |
| 11 | Estética "tática" como justificativa de design (cap. 02 §2.1) | Média | Remover ou citar como decisão de design (não como fato técnico) |
| 12 | Fórmulas físicas usadas sem citação (lei de Newton, cap. 05 §7.3; fotometria/UVI) | Média | Livros-texto de física/instrumentação; CIE/ISO 17166; OMS |
| 13 | LVGL / GUI embarcada (complementar) | Baixa | Documentação LVGL; SquareLine |

> Plano de ação e sequência de busca em `plano_de_referencias.md` (raiz do projeto).
