# Coleta PPG bruto versus filtrado

O teste standalone está configurado com `PPG_CSV_MODE=1` em `main/main.c`. O pipeline continua processando a 100 Hz e publica uma amostra alinhada a cada duas amostras, resultando em CSV a 50 Hz.

## Procedimento curto

1. Entre em `Codigos/Teste_de_componentes/MAX30102`.
2. Execute `idf.py build flash`.
3. Salve a serial durante aproximadamente 60 segundos:

   ```bash
   script -q -c "idf.py monitor" ppg_serial.log
   ```

4. Coloque o dedo no sensor, permaneça imóvel e aguarde pelo menos 10 segundos de estabilização antes do trecho principal.
5. Encerre o monitor com `Ctrl+]`.
6. Gere o CSV limpo e a visão geral da linha de base:

   ```bash
   python3 -m pip install matplotlib
   python3 plot_ppg_csv.py ppg_serial.log --output ppg_bruto_filtrado
   ```

7. Para as figuras da monografia, descarte o transiente inicial de contato, gere a visão geral e
   produza uma comparação ampliada sem normalização independente:

    ```bash
    python3 plot_ppg_csv.py ppg_serial.log --start 15 --end 73 \
       --plot-mode summary --output ppg_bruto_filtrado_estavel
    python3 plot_ppg_csv.py ppg_serial.log --start 30 --end 40 \
       --plot-mode comparison --output ppg_bruto_filtrado_zoom_30_40s
    ```

## Saídas

- `ppg_bruto_filtrado.csv`: amostras alinhadas de IR/vermelho brutos, estimativa DC e AC filtrado;
- no modo `summary`, PNG e SVG com IR bruto/estimativa DC entre 15 e 73 s e espectro antes/depois
  no recorte de 30 a 40 s;
- no modo `comparison`, PNG e SVG com sobreposição temporal em escala comum e espectro antes/depois.

## Cuidados de interpretação

- Os primeiros segundos incluem convergência do estimador DC; não os use para avaliar regime estacionário.
- A curva `raw - DC` representa a remoção de linha de base antes do passa-baixa Butterworth.
- As curvas comparadas usam as mesmas unidades e o mesmo eixo; não há normalização independente.
- A coleta confirma remoção de linha de base e limitação de banda. Sem executar o detector com e
  sem o passa-baixa, ela não demonstra ganho de exatidão em frequência cardíaca ou SpO₂.
