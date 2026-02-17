# Teste de Componentes - ESP32

Este diretório contém os testes individuais de cada componente utilizado no projeto de TCC, implementados em **ESP-IDF**.

## Componentes Disponíveis

| Componente        | Interface | Descrição                                          | Documentação                      |
| ----------------- | --------- | ---------------------------------------------------| --------------------------------- |
| **DS18B20**       | 1-Wire    | Sensor de temperatura à prova d'água               | [README](DS18B20/README.md)       |
| **ADXL345**       | I²C       | Acelerômetro triaxial                              | [README](ADXL345/README.md)       |
| **MPU9250**       | I²C       | IMU 9-DOF (acelerômetro, giroscópio, magnetômetro) | [README](MPU9250/README.md)       |
| **MAX30102**      | I²C       | Sensor de frequência cardíaca e SpO2               | [README](MAX30100/README.md)      |
| **Round Display** | SPI + I²C | Display circular Seeed (GC9A01 + CST816S)          | [README](Round_Display/README.md) |

Cada componente possui seu próprio README com:

- Especificações técnicas
- Pinout e conexões
- Instruções de compilação
- Troubleshooting específico
- Exemplos de código

## Índice

- [Preparação do Ambiente](#preparação-do-ambiente)
- [Estrutura dos Projetos](#estrutura-dos-projetos)
- [Comandos Básicos](#comandos-básicos)
- [Troubleshooting Geral](#troubleshooting-geral)

---

## Preparação do Ambiente

**IMPORTANTE:** É necessário configurar o ESP-IDF com o framework versão 5.5.1 ou mais recente.

### 1. Pré-requisitos Windows

Instalar na seguinte ordem:

1. **VS Code** - [Download](https://code.visualstudio.com/)
2. **Git for Windows** - [Download](https://git-scm.com/download/win)
3. **Python 3.8–3.11** - [Download](https://www.python.org/)
   - **IMPORTANTE**: Marcar "Add Python to PATH" durante instalação

**Reinicie o Windows após instalar tudo.**

### 2. Instalar ESP-IDF no VS Code

1. Abra o VS Code
2. Instale a extensão **Espressif IDF**
3. Pressione `Ctrl + Shift + P`
4. Digite: `ESP-IDF: Configure ESP-IDF Extension`
5. Escolha:
   - **Express**
   - Download do ESP-IDF automático
   - Toolchain automática
6. Aguarde (leva alguns minutos)

### 3. Validar Instalação

Abra um terminal **ESP-IDF** (não PowerShell comum) e rode:

```bash
idf.py --version
```

Se retornar a versão v5.5.1 ou superior, o ambiente está correto.

### 4. Driver USB (Windows)

Verifique qual chip USB seu ESP32 usa:

- **CP2102** → [Driver Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **CH340** → [Driver WCH](http://www.wch-ic.com/downloads/CH341SER_EXE.html)

**Verificação:**

1. Conecte o ESP32
2. Abra o **Gerenciador de Dispositivos**
3. Procure em **Portas (COM e LPT)**
4. Deve aparecer como `COMx` (ex: COM5)
5. Anote o número da porta

---

## Estrutura dos Projetos

Cada componente tem seu próprio projeto ESP-IDF:

```
Teste_de_componentes/
├── DS18B20/          # Sensor de temperatura (1-Wire)
├── ADXL345/          # Acelerômetro I²C
├── MPU9250/          # IMU 9-DOF (Bússola, Gestos)
│   ├── Bussola/      # Bússola digital (magnetômetro AK8963)
│   └── Gestos/       # Reconhecimento de gestos (acelerômetro + giroscópio)
├── MAX30100/         # Monitor cardíaco MAX30102 (HR + SpO2)
└── Round_Display/    # Display circular Seeed
```

Cada projeto segue a estrutura padrão do ESP-IDF:

```
componente/
├── main/
│   ├── CMakeLists.txt
│   └── componente.c
├── CMakeLists.txt
├── sdkconfig
└── build/            # Gerado automaticamente (ignorado pelo git)
```

---

## Comandos Básicos

### Carregar Ambiente ESP-IDF (Linux/macOS)

Antes de usar qualquer comando `idf.py` no terminal, é necessário carregar o ambiente do ESP-IDF:

```bash
source $HOME/esp/esp-idf/export.sh
```

**Dica:** Para não precisar digitar isso toda vez, adicione um alias ao seu `~/.bashrc` ou `~/.zshrc`:

```bash
alias esp="source $HOME/esp/esp-idf/export.sh"
```

Depois basta rodar `esp` antes de começar a trabalhar.

> **Nota:** No Windows, use o terminal **ESP-IDF Terminal** do VS Code, que já carrega o ambiente automaticamente.

### Criar Novo Projeto

```bash
cd ~/Documents/TCC/Codigos/Teste_de_componentes
idf.py create-project NOME_DO_COMPONENTE
cd NOME_DO_COMPONENTE
```

### Definir Target

```bash
idf.py set-target esp32c6 #ou qualquer outro microcontrolador compatível que esteja em uso
```

**Sempre execute isso antes de buildar pela primeira vez.**

### Build

```bash
idf.py build
```

### Flash (gravar no ESP32)

```bash
idf.py flash
```

Ou especificando a porta:

```bash
idf.py -p COM5 flash
```

### Monitor (ver saída serial)

```bash
idf.py monitor
```

Sair do monitor: `Ctrl + ]`

### Flash + Monitor (combo)

```bash
idf.py flash monitor
```

Ou com porta específica:

```bash
idf.py -p COM5 flash monitor
```

### Limpar Build

```bash
idf.py fullclean
```

---

## Início Rápido

### Exemplo: Testar DS18B20

```bash
cd DS18B20
idf.py set-target esp32
idf.py build
idf.py -p COM5 flash monitor
```

Para instruções detalhadas de cada componente, consulte o README específico na tabela acima.

---

## Troubleshooting Geral

## Troubleshooting Geral

### Build Errors

#### `idf.py: command not found`

- Você está no terminal errado
- Use o terminal **ESP-IDF Terminal** no VS Code

#### `Target mismatch`

```bash
idf.py set-target esp32
idf.py fullclean
idf.py build
```

#### Erro de dependências

```bash
idf.py fullclean
rm -rf managed_components
idf.py build
```

### Flash Errors

#### `Serial port not found`

- Verifique o driver USB instalado
- Confirme a porta no Gerenciador de Dispositivos
- Especifique a porta: `idf.py -p COM5 flash`

#### `Permission denied`

- Feche o monitor serial antes de dar flash
- Desconecte outros programas que usam a porta (Arduino IDE, PuTTY, etc.)

### Monitor não mostra nada

#### Baud rate errado

```bash
idf.py monitor -b 115200
```

#### Porta errada

```bash
idf.py -p COM5 monitor
```

#### Reset do ESP32

- Pressione o botão RESET após abrir o monitor
- Ou reconecte o cabo USB

**Para problemas específicos de sensores e displays, consulte o README de cada componente:**

- [DS18B20 Troubleshooting](DS18B20/README.md#troubleshooting)
- [ADXL345 Troubleshooting](ADXL345/README.md#troubleshooting)
- [MPU9250 Troubleshooting](MPU9250/README.md)
- [MAX30102 Troubleshooting](MAX30100/README.md#troubleshooting)
- [Round Display Troubleshooting](Round_Display/README.md#troubleshooting)

---

## Referências

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP-IDF VS Code Extension](https://github.com/espressif/vscode-esp-idf-extension)
- [DS18B20 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf)
- [ADXL345 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ADXL345.pdf)
- [MPU-9250 Product Specification](https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf)
- [AK8963 Datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/535561/AKM/AK8963.html)
- [MAX30102 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf)

---

## Notas

- Sempre use o **ESP-IDF Terminal**, não o PowerShell comum
- Cada componente é um projeto independente
- A pasta `build/` é ignorada pelo git (.gitignore)
- Para adicionar novos componentes, siga a mesma estrutura
- Mantenha a documentação atualizada ao adicionar novos testes

---

**Autor:** Guilherme Franco  
**Data:** Fevereiro 2026  
**Versão ESP-IDF:** v5.5.1
