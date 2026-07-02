import argparse
import os

import m5
from m5.objects import *


def strip_txt_comment(line):
    comment_positions = [
        pos for pos in (line.find("#"), line.find(";"), line.find("//"))
        if pos != -1
    ]
    if comment_positions:
        line = line[: min(comment_positions)]
    return line.strip()


def get_txt_entry(path):
    # Keep this default in sync with TextInstObject's DefaultLoadAddr.
    default_entry = 0x10000

    with open(path, "r", encoding="utf-8") as inst_file:
        for line in inst_file:
            text = strip_txt_comment(line)
            if not text:
                continue
            if text.startswith("@"):
                return int(text[1:].strip(), 0)
            return default_entry

    raise ValueError(f"{path}: no instructions found")


parser = argparse.ArgumentParser()
parser.add_argument("--initial-pc", type=lambda value: int(value, 0), default=None)
parser.add_argument("--fetch-count", type=int, default=10)
parser.add_argument(
    "--inst-txt",
    type=str,
    default=None,
    help="Text/hex RISC-V instruction file to preload into DRAM",
)
args = parser.parse_args()

inst_txt = os.path.realpath(args.inst_txt) if args.inst_txt else None
initial_pc = args.initial_pc
if initial_pc is None:
    initial_pc = get_txt_entry(inst_txt) if inst_txt else 0

system = System()
system.cache_line_size = 512
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
system.membus = SystemXBar()

system.l0cache = L0cache()
system.ucore = Ucore(
    fetch_count=args.fetch_count,
    initial_pc=initial_pc,
    l0cache=system.l0cache,
)
# Ucore is no longer a BaseCPU. Its public port forwards to the Ibuffer port.
system.ucore.port = system.membus.cpu_side_ports

system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
if inst_txt:
    system.mem_ctrl.dram.image_file = inst_txt
system.mem_ctrl.port = system.membus.mem_side_ports

system.system_port = system.membus.cpu_side_ports

root = Root(full_system=False, system=system)
m5.instantiate()

if inst_txt:
    print(f"Loading instructions from: {inst_txt}")
print(f"Fetching from initial PC: {initial_pc:#x}")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
