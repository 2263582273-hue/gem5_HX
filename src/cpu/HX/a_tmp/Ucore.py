from m5.objects.BaseCPU import BaseCPU
from m5.objects.RiscvCPU import RiscvCPU
from m5.objects.RiscvMMU import RiscvMMU
from m5.params import Param


class Ucore(BaseCPU, RiscvCPU):
    type = "Ucore"
    cxx_header = "cpu/HX/Ucore.hh"
    cxx_class = "gem5::Ucore"

    mmu = RiscvMMU()

    fetch_count = Param.Unsigned(
        8, "Number of 4-byte machine-code words to fetch before exiting"
    )
