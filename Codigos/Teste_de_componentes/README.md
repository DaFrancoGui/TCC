# Teste de Componentes - ESP32

Este diretório contém os testes individuais de cada componente utilizado no projeto de TCC, implementados em **ESP-IDF**.

## Componentes Disponíveis

Cada componente possui seu próprio README com:

- Especificações técnicas
- Pinout e conexões
- Instruções de compilação
- Troubleshooting específico
- Exemplos de código

## Índice

- [Preparação do Ambiente](#preparação-do-ambiente)
- [Comandos Básicos](#comandos-básicos)

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
