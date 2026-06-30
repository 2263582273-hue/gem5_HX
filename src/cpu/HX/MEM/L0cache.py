from m5.objects.TickedObject import TickedObject
from m5.params import Param


class L0cache(TickedObject):
    type = "L0cache"
    cxx_header = "cpu/HX/MEM/L0cache.hh"
    cxx_class = "gem5::L0cache"

    num_arbiter_ports = Param.Unsigned(
        1, "轮询仲裁器处理的输入端口数量"
    )

