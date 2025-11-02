# Transmissão de Dados via LoRa — FPGA + BitDogLab

Projeto desenvolvido com o objetivo de realizar **transmissão de dados via LoRa** utilizando um **FPGA Colorlight i9** e o microcontrolador **BitDogLab (Raspberry Pi Pico W)**.

O sistema é dividido em dois componentes principais:
- **FPGA (hardware/):** responsável pela coleta, processamento e transmissão dos dados via LoRa.
- **BitDogLab (software/):** responsável pela recepção e visualização dos dados transmitidos pelo FPGA.

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

