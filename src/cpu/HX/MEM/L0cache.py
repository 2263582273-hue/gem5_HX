from m5.objects.Cache import Cache
from m5.params import Param


class L0cache(Cache):
    type = "L0cache"
    cxx_header = "cpu/HX/MEM/L0cache.hh"
    cxx_class = "gem5::L0cache"

    # 32 sets * 4 ways * 512 bytes/line = 64 KiB.
    size = "64KiB"
    assoc = 4
    is_read_only = True

    tag_latency = 1
    data_latency = 1
    response_latency = 1
    mshrs = 8
    tgts_per_mshr = 8

    num_arbiter_ports = Param.Unsigned(
        8, "轮询仲裁器处理的输入端口数量"
    )
