# Transmissão de Dados via LoRa — FPGA + BitDogLab

Projeto desenvolvido com o objetivo de realizar **transmissão de dados via LoRa** utilizando um **FPGA Colorlight i9** e o microcontrolador **BitDogLab (Raspberry Pi Pico W)**.

O projeto integra:
- Leitura de dados do **sensor AHT10** (temperatura e umidade);
- Comunicação **LoRa** entre FPGA e microcontrolador;
- Processamento e exibição dos dados no terminal serial.

---

## Descrição Geral

O sistema é dividido em duas partes principais:

- **FPGA (hardware/):**
  - Responsável pela interface LoRa e controle da transmissão;
  - Implementado em **LiteX** e **Python**, com integração em **C** para o firmware embarcado no softcore.

- **BitDogLab (software/):**
  - Atua como estação receptora LoRa;
  - Processa e exibe os dados recebidos do FPGA;
  - Desenvolvido em **C** com o SDK do Raspberry Pi Pico.

---

## Diagrama de Blocos do Sistema
```bash
    ┌──────────────────────────────┐
    │        Sensor AHT10          │
    │   (Temperatura / Umidade)    │
    └──────────────┬───────────────┘
                   │ I²C
                   ▼
    ┌──────────────────────────────┐
    │            FPGA              │
    │   (Colorlight i9 - LiteX)    │
    │ ┌──────────────────────────┐ │
    │ │   Controlador LoRa TX    │ │
    │ │   Módulo SPI / UART      │ │
    │ │   Softcore (firmware C)  │ │
    │ └──────────────────────────┘ │
    └──────────────┬───────────────┘
                   │ LoRa (SPI)
                   ▼
    ┌──────────────────────────────┐
    │        BitDogLab RX          │
    │   (Raspberry Pi Pico SDK)    │
    │  - Recepção via LoRa         │
    │  - Exibição no terminal      │
    └──────────────────────────────┘

```

---

## ⚙️ Especificações Técnicas

### 🔹 Frequências do sistema
```bash
| Sinal / Clock        | Frequência | Descrição                                |
|----------------------|------------|------------------------------------------|
| Clock principal FPGA | 25 MHz     | Clock do sistema gerado pelo oscilador   |
| Clock CPU (LiteX)    | 50 MHz     | Clock do softcore do firmware C          |
| Clock LoRa SPI       | 8 MHz      | Clock de comunicação SPI com módulo LoRa |
| Clock UART debug     | 115200 bps | Comunicação serial com terminal          |
```

---

### 🔹 Pinos Utilizados (Colorlight i9)
```bash
| Sinal       | FPGA Pin | Descrição                       |
|--------------|-----------|-------------------------------|
| **SCL (AHT10)** | E12 | Clock do barramento I²C          |
| **SDA (AHT10)** | D12 | Dados do barramento I²C          |
| **LoRa_MOSI**   | B6  | Dados SPI para o módulo LoRa     |
| **LoRa_MISO**   | B7  | Dados SPI recebidos do módulo    |
| **LoRa_SCK**    | C7  | Clock SPI                        |
| **LoRa_CS**     | A8  | Chip Select do módulo LoRa       |
| **UART_TX**     | D9  | Transmissão serial (debug/logs)  |
| **UART_RX**     | C9  | Recepção serial (debug/logs)     |
| **GND**         | —   | Referência comum                 |
| **VCC (3.3V)**  | —   | Alimentação dos periféricos      |
```
*(Os pinos podem variar conforme a revisão da placa. Ajuste no arquivo de constraints conforme necessário.)*

---

## Estrutura do Projeto
```bash
.
├── hardware/ → Projeto FPGA
├── software/ → Projeto BitDogLab
└── README.md
```

---

## Ambiente de Desenvolvimento

### Requisitos
- **OSS CAD Suite** (para sintetizar e gravar o FPGA);
- **LiteX** (framework em Python para geração do design);
- **openFPGALoader** (para carregar o bitstream no FPGA);
- **SDK do Raspberry Pi Pico** (para compilar o código C);
- **Extensão Raspberry Pi Pico** no VS Code.

---

## Hardware (FPGA)

Deve estar dentro da pasta `hardware`:

```bash
cd hardware
```
1️⃣ Ativar o ambiente do OSS CAD Suite
```bash
source [SEU-PATH]/oss-cad-suite/environment
```
Substitua [SEU-PATH] pelo caminho onde o OSS CAD Suite está instalado.

2️⃣ Compilar o código em Python e subir
```bash
python3 litex/tarefa05_soc.py --board i9 --revision 7.2 --build --load --ecppack-compress
```
3️⃣ Compilar o código em C (firmware da FPGA)
```bash
cd firmware
make
cd ..
```
4️⃣ Gravar o firmware na FPGA
```bash
sudo [SEU-PATH]/oss-cad-suite/bin/openFPGALoader -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```
5️⃣ Subir o código C e iniciar o terminal serial da FPGA
```bash
litex_term /dev/ttyACM0 --kernel firmware/firmware.bin
```
*Lembre-se: após entrar no terminal serial pressione Enter até que apareça litex>, digite reboot e pressione Enter novamente.

---

## Software (BitDogLab)

Primeiro, abra o diretório do projeto no VS Code.

1️⃣ Gerar arquivos de build
```bash
cmake -G Ninja -S . -B build
cmake --build build
```
2️⃣ Gravar o firmware
Após a compilação, use a própria interface da extensão Raspberry Pi Pico para:
- Conectar a BitDogLab via USB (bootsel);
- Clicar em “Run Project”.

