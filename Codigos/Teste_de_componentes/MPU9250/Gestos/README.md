# Leitor de Gestos - MPU-9250/6500

Implementação de reconhecimento de gestos simples usando acelerômetro e giroscópio.

## Funcionalidades Planejadas

- ⏸️ Detecção de movimento básico
- ⏸️ Identificação de gestos:
  - Shake (sacudir)
  - Tap (toque duplo)
  - Tilt (inclinação)
  - Rotation (rotação)
  - Free fall (queda livre)
- ⏸️ Saída via serial
- ⏸️ Integração com Round Display (futuro)

## Gestos a Implementar

### 1. Shake (Sacudir)
Detecta movimento rápido de vai-e-vem.
- **Sensor**: Acelerômetro
- **Threshold**: ΔAccel > 2g em curto período

### 2. Double Tap (Toque Duplo)
Detecta dois toques rápidos consecutivos.
- **Sensor**: Acelerômetro
- **Threshold**: Dois picos de aceleração com <500ms entre eles

### 3. Tilt (Inclinação)
Detecta mudança de orientação (para cima/baixo/esquerda/direita).
- **Sensor**: Acelerômetro + Giroscópio
- **Threshold**: Ângulo > 30° em relação à posição inicial

### 4. Rotation (Rotação)
Detecta rotação em torno de um eixo.
- **Sensor**: Giroscópio
- **Threshold**: ΔGyro > 250°/s em qualquer eixo

### 5. Free Fall (Queda Livre)
Detecta quando o dispositivo está caindo.
- **Sensor**: Acelerômetro
- **Threshold**: |Accel_total| < 0.5g por >100ms

## Formato de Saída (Serial)

```
╔═══════════════════════════════════════╗
║     DETECTOR DE GESTOS MPU-9250       ║
╠═══════════════════════════════════════╣
║ Acelerômetro (g):                     ║
║   X:  0.12  Y: -0.05  Z:  1.02        ║
║                                       ║
║ Giroscópio (°/s):                     ║
║   X:  2.5   Y:  -1.2  Z:  0.8         ║
║                                       ║
║ Gesto Detectado: SHAKE                ║
║ Confiança: 95%                        ║
╚═══════════════════════════════════════╝
```

## Algoritmo de Detecção

```
1. Ler acelerômetro e giroscópio (100Hz)
2. Calcular magnitude vetorial
3. Comparar com thresholds de cada gesto
4. Aplicar filtro de debounce (evitar múltiplas detecções)
5. Emitir evento de gesto reconhecido
```

## Status do Projeto

**Última atualização**: 16/02/2026  
**Status**: ⏸️ Pasta criada, sem código implementado ainda (focando primeiro na bússola)

## Próximos Passos

1. ⏸️ Aguardar finalização da bússola
2. ⏸️ Implementar driver base MPU-9250/6500
3. ⏸️ Criar máquina de estados para detecção de gestos
4. ⏸️ Testar cada gesto individualmente
5. ⏸️ Integrar com interface visual (Round Display)
