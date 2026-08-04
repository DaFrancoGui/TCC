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
6. Gere o CSV limpo e a figura:

   ```bash
   python3 -m pip install matplotlib
   python3 plot_ppg_csv.py ppg_serial.log --output ppg_bruto_filtrado
   ```

7. Para a figura da monografia, descarte o transiente inicial de contato e limite o trecho anterior
    à retirada do dedo:

    ```bash
    python3 plot_ppg_csv.py ppg_serial.log --start 15 --end 73 \
       --output ppg_bruto_filtrado_estavel
    ```

## Saídas

- `ppg_bruto_filtrado.csv`: amostras alinhadas de IR/vermelho brutos, estimativa DC e AC filtrado;
- `ppg_bruto_filtrado.png`: quatro painéis com sinais brutos, `raw - DC`, sinais filtrados e comparação normalizada do IR.

## Cuidados de interpretação

- Os primeiros segundos incluem convergência do estimador DC; não os use para avaliar regime estacionário.
- A curva `raw - DC` representa a remoção de linha de base antes do passa-baixa Butterworth.
- A normalização do último painel serve apenas à comparação de forma; ela não preserva amplitudes absolutas.
- Esta coleta demonstra o efeito do processamento no protótipo. Não constitui validação clínica das estimativas de frequência cardíaca ou SpO2.
