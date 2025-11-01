#!/usr/bin/env python3


from migen import *

from litex.gen import *

from litex.build.io import DDROutput

from litex_boards.platforms import colorlight_i5

from litex.soc.cores.clock import *
from litex.soc.integration.soc_core import *
from litex.soc.integration.builder import *
from litex.soc.cores.video import VideoHDMIPHY
from litex.soc.cores.led import LedChaser
from litex.build.generic_platform import Subsignal, Pins, IOStandard

from litex.soc.interconnect.csr import *
from litex.soc.cores.bitbang import I2CMaster
from litex.soc.cores.spi import SPIMaster
from litex.soc.cores.gpio import GPIOIn, GPIOOut

from litedram.modules import M12L64322A
from litedram.phy import GENSDRPHY, HalfRateGENSDRPHY

from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII


# CRG (Clock Reset Generator)

class _CRG(LiteXModule):
    def __init__(self, platform, sys_clk_freq, use_internal_osc=False, with_usb_pll=False, with_video_pll=False, sdram_rate="1:1"):
        self.rst    = Signal()
        self.cd_sys = ClockDomain()
        if sdram_rate == "1:2":
            self.cd_sys2x    = ClockDomain()
            self.cd_sys2x_ps = ClockDomain()
        else:
            self.cd_sys_ps = ClockDomain()

        # Clock / Reset
        if not use_internal_osc:
            clk = platform.request("clk25")
            clk_freq = 25e6
        else:
            clk = Signal()
            div = 5
            self.specials += Instance("OSCG",
                p_DIV = div,
                o_OSC = clk
            )
            clk_freq = 310e6/div

        rst_n = platform.request("cpu_reset_n")

        # PLL principal
        self.pll = pll = ECP5PLL()
        self.comb += pll.reset.eq(~rst_n | self.rst)
        pll.register_clkin(clk, clk_freq)
        pll.create_clkout(self.cd_sys,    sys_clk_freq)
        if sdram_rate == "1:2":
            pll.create_clkout(self.cd_sys2x,    2*sys_clk_freq)
            pll.create_clkout(self.cd_sys2x_ps, 2*sys_clk_freq, phase=180)
        else:
           pll.create_clkout(self.cd_sys_ps, sys_clk_freq, phase=180)

        # USB PLL (opcional)
        if with_usb_pll:
            self.usb_pll = usb_pll = ECP5PLL()
            self.comb += usb_pll.reset.eq(~rst_n | self.rst)
            usb_pll.register_clkin(clk, clk_freq)
            self.cd_usb_12 = ClockDomain()
            self.cd_usb_48 = ClockDomain()
            usb_pll.create_clkout(self.cd_usb_12, 12e6, margin=0)
            usb_pll.create_clkout(self.cd_usb_48, 48e6, margin=0)

        # Video PLL (opcional)
        if with_video_pll:
            self.video_pll = video_pll = ECP5PLL()
            self.comb += video_pll.reset.eq(~rst_n | self.rst)
            video_pll.register_clkin(clk, clk_freq)
            self.cd_hdmi   = ClockDomain()
            self.cd_hdmi5x = ClockDomain()
            video_pll.create_clkout(self.cd_hdmi,    40e6, margin=0)
            video_pll.create_clkout(self.cd_hdmi5x, 200e6, margin=0)

        # SDRAM clock output via DDR output primitive
        sdram_clk = ClockSignal("sys2x_ps" if sdram_rate == "1:2" else "sys_ps")
        self.specials += DDROutput(1, 0, platform.request("sdram_clock"), sdram_clk)


# Tarefa05SoC 

class Tarefa05SoC(SoCCore):
    def __init__(self, board="i5", revision="7.0", toolchain="trellis", sys_clk_freq=60e6,
        with_led_chaser        = True,
        use_internal_osc       = False,
        sdram_rate             = "1:1",
        with_video_terminal    = False,
        with_video_framebuffer = False,
        **kwargs):
        board = board.lower()
        assert board in ["i5", "i9"]
        platform = colorlight_i5.Platform(board=board, revision=revision, toolchain=toolchain)

        # CRG 
        with_usb_pll   = kwargs.get("uart_name", None) == "usb_acm"
        with_video_pll = with_video_terminal or with_video_framebuffer
        self.crg = _CRG(platform, sys_clk_freq,
            use_internal_osc = use_internal_osc,
            with_usb_pll     = with_usb_pll,
            with_video_pll   = with_video_pll,
            sdram_rate       = sdram_rate
        )

        # SoCCore init 
        SoCCore.__init__(self, platform, int(sys_clk_freq), ident = "Tarefa05 SoC on Colorlight " + board.upper(), **kwargs)

        # LEDs 
        if with_led_chaser:
            ledn = platform.request_all("user_led_n")
            self.leds = LedChaser(pads=ledn, sys_clk_freq=sys_clk_freq)

        # SPI Flash 
        if board == "i5":
            from litespi.modules import GD25Q16 as SpiFlashModule
        if board == "i9":
            from litespi.modules import W25Q64 as SpiFlashModule

        from litespi.opcodes import SpiNorFlashOpCodes as Codes
        self.add_spi_flash(mode="1x", module=SpiFlashModule(Codes.READ_1_1_1))

        # SDR SDRAM 
        if not self.integrated_main_ram_size:
            sdrphy_cls = HalfRateGENSDRPHY if sdram_rate == "1:2" else GENSDRPHY
            self.sdrphy = sdrphy_cls(platform.request("sdram"))
            self.add_sdram("sdram",
                phy           = self.sdrphy,
                module        = M12L64322A(sys_clk_freq, sdram_rate),
                l2_cache_size = kwargs.get("l2_size", 8192)
            )

        # Pinos SPI para LoRa (RFM95) 
        spi_pads = [
            ("spi", 0,
                Subsignal("clk",  Pins("G20")),
                Subsignal("mosi", Pins("L18")),
                Subsignal("miso", Pins("M18")),
                Subsignal("cs_n", Pins("N17")),
                IOStandard("LVCMOS33")
            ),
            ("lora_reset", 0, Pins("L20"), IOStandard("LVCMOS33"))
        ]

        platform.add_extension(spi_pads)

        # Core SPI master e CSR 'spi' (mantém compatibilidade com firmware)
        self.spi = SPIMaster(pads=platform.request("spi"), data_width=8, sys_clk_freq=sys_clk_freq, spi_clk_freq=1e6)
        self.add_csr("spi")

        # GPIO para reset do LoRa
        self.submodules.lora_reset = GPIOOut(platform.request("lora_reset"))
        self.add_csr("lora_reset")

        # Pinos I2C para AHT10 
        i2c_pads = [
            ("i2c", 0,
                Subsignal("scl", Pins("U17")),
                Subsignal("sda", Pins("U18")),
                IOStandard("LVCMOS33")
            )
        ]

        platform.add_extension(i2c_pads)

        # Core I2C bitbang e CSR 'i2c'
        self.submodules.i2c = I2CMaster(pads=platform.request("i2c"))
        self.add_csr("i2c")


# Build / parser 

def main():
    from litex.build.parser import LiteXArgumentParser
    parser = LiteXArgumentParser(platform=colorlight_i5.Platform, description="Tarefa05: LiteX SoC (Colorlight).")
    parser.add_target_argument("--board",            default="i5",             help="Tipo de placa (i5 ou i9).")
    parser.add_target_argument("--revision",         default="7.0",            help="Revisão da placa.")
    parser.add_target_argument("--sys-clk-freq",     default=60e6, type=float, help="Frequência do clock do sistema.")
    ethopts = parser.target_group.add_mutually_exclusive_group()
    ethopts.add_argument("--with-ethernet",   action="store_true",      help="Habilita Ethernet.")
    ethopts.add_argument("--with-etherbone",  action="store_true",      help="Habilita Etherbone.")
    parser.add_target_argument("--remote-ip", default="192.168.1.100",  help="IP remoto do servidor TFTP.")
    parser.add_target_argument("--local-ip",  default="192.168.1.50",   help="IP local.")
    sdopts = parser.target_group.add_mutually_exclusive_group()
    sdopts.add_argument("--with-spi-sdcard",  action="store_true", help="Habilita SDCard em modo SPI.")
    sdopts.add_argument("--with-sdcard",      action="store_true", help="Habilita SDCard (modo nativo).")
    parser.add_target_argument("--eth-phy",          default=0, type=int, help="PHY Ethernet.")
    parser.add_target_argument("--use-internal-osc", action="store_true", help="Usa oscilador interno.")
    parser.add_target_argument("--sdram-rate",       default="1:1",       help="Taxa SDRAM (1:1 full ou 1:2 half).")
    viopts = parser.target_group.add_mutually_exclusive_group()
    viopts.add_argument("--with-video-terminal",    action="store_true", help="Habilita terminal de vídeo (HDMI).")
    viopts.add_argument("--with-video-framebuffer", action="store_true", help="Habilita framebuffer de vídeo (HDMI).")
    # Recursos do projeto LoRa/AHT10
    parser.add_target_argument("--with-lora",     action="store_true", help="Habilita SPI para módulo LoRa (RFM9x).")
    parser.add_target_argument("--with-aht10",    action="store_true", help="Habilita I2C para sensor AHT10.")
    parser.add_target_argument("--use-example-pins", action="store_true", help="Carrega arquivo de pinos de exemplo (edite pins_colorlight_i9_ext.py).")

    args = parser.parse_args()

    soc = Tarefa05SoC(board=args.board, revision=args.revision,
        toolchain              = args.toolchain,
        sys_clk_freq           = args.sys_clk_freq,
        with_ethernet          = args.with_ethernet,
        sdram_rate             = args.sdram_rate,
        with_etherbone         = args.with_etherbone,
        local_ip               = args.local_ip,
        remote_ip              = args.remote_ip,
        eth_phy                = args.eth_phy,
        use_internal_osc       = args.use_internal_osc,
        with_video_terminal    = args.with_video_terminal,
        with_video_framebuffer = args.with_video_framebuffer,
        with_lora_spi          = args.with_lora,
        with_i2c_aht10         = args.with_aht10,
        use_example_pins       = args.use_example_pins,
        **parser.soc_argdict
    )

    soc.platform.add_extension(colorlight_i5._sdcard_pmod_io)
    if args.with_spi_sdcard:
        soc.add_spi_sdcard()
    if args.with_sdcard:
        soc.add_sdcard()

    builder = Builder(soc, **parser.builder_argdict)
    if args.build:
        builder.build(**parser.toolchain_argdict)

    if args.load:
        prog = soc.platform.create_programmer()
        prog.load_bitstream(builder.get_bitstream_filename(mode="sram"))

if __name__ == "__main__":
    main()
