from m5.objects.BaseCPU import BaseCPU
from m5.objects.RiscvCPU import RiscvCPU
from m5.objects.RiscvMMU import RiscvMMU
from m5.objects.TickedObject import TickedObject
from m5.params import Param
from m5.proxy import Parent


class Ibuffer(TickedObject):
    type = "Ibuffer"
    cxx_header = "cpu/HX/MEM/Ibuffer.hh"
    cxx_class = "gem5::Ibuffer"

    ucore = Param.BaseCPU(Parent.any, "CPU this Ibuffer belongs to")
    cache_line_size = Param.Unsigned(
        Parent.cacheLineSize, "Ibuffer cache-line size in bytes"
    )


class Ucore(BaseCPU, RiscvCPU):
    type = "Ucore"
    cxx_header = "cpu/HX/Ucore.hh"
    cxx_class = "gem5::Ucore"

    mmu = RiscvMMU()

    fetch_count = Param.Unsigned(
        10, "Number of 4-byte machine-code words to fetch before exiting"
    )
    # cacheLineSize是cache里的行宽度，fetchSize是指令宽度，两者的单位都是字节byte
    # cacheLineSize应该是fetchSize的整数倍
    cacheLineSize = Param.Unsigned(16 * 8, "Ibuffer cache-line size in bytes")
    fetchSize = Param.Unsigned(16, "fetchsize in bytes")
    num_thread = Param.Unsigned(16, "线程数量")
    ibuffer = Param.Ibuffer(Ibuffer(), "Instruction buffer")
