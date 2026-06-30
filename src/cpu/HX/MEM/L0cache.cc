#include "cpu/HX/MEM/L0cache.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/L0cache.hh"

namespace gem5
{

RoundRobinArbiter::RoundRobinArbiter(std::size_t num_ports)
    : portCount(num_ports)
{
    fatal_if(portCount == 0,
             "RoundRobinArbiter must have at least one input port");
}

int
RoundRobinArbiter::arbitrate(const std::vector<bool> &valid)
{
    for (std::size_t offset = 0; offset < portCount; ++offset) {
        const std::size_t port = (nextPriority + offset) % portCount;
        if (valid[port]) {
            nextPriority = (port + 1) % portCount;
            return static_cast<int>(port);
        }
    }

    return -1;
}

L0cache::L0cache(const L0cacheParams &params)
    : TickedObject(params),
      arbiter(params.num_arbiter_ports)
{
}

void
L0cache::setUcore(Ucore *ptr)
{
    fatal_if(!ptr, "L0cache cannot bind to a null Ucore");
    fatal_if(ucore && ucore != ptr,
             "L0cache is already bound to another Ucore");
    ucore = ptr;
}

void
L0cache::startup()
{
    ClockedObject::startup();
    DPRINTF(L0cache, "starting L0 cache\n");
    start();
}

void
L0cache::evaluate()
{
    //
}

} // namespace gem5
