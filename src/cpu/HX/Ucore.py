from m5.objects.TickedObject import TickedObject
from m5.params import Param, RequestPort
from m5.proxy import Parent


class Ibuffer(TickedObject):
    type = "Ibuffer"
    cxx_header = "cpu/HX/MEM/Ibuffer.hh"
    cxx_class = "gem5::Ibuffer"

    # ucore = Param.SimObject(Parent.any, "Ucore this Ibuffer belongs to")
    cache_line_size = Param.Unsigned(
        Parent.cacheLineSize, "Ibuffer cache-line size in bytes"
    )


class Ucore(TickedObject):
    type = "Ucore"
    cxx_header = "cpu/HX/Ucore.hh"
    cxx_class = "gem5::Ucore"

    port = RequestPort("Memory-side request port used by the Ibuffer")

    initial_pc = Param.Addr(0, "Initial PC for txt/raw-code driven fetch")
    fetch_count = Param.Unsigned(
        10, "Number of machine-code words to fetch before exiting"
    )
    # cacheLineSize and fetchSize are both in bytes.
    # cacheLineSize should be a multiple of fetchSize.
    cacheLineSize = Param.Unsigned(16 * 8, "Ibuffer cache-line size in bytes")
    fetchSize = Param.Unsigned(16, "fetchsize in bytes")
    num_thread = Param.Unsigned(16, "Number of threads")
    ibuffer = Param.Ibuffer(Ibuffer(), "Instruction buffer")
    l0cache = Param.L0cache("与 Ucore 连接的 L0cache")
