#include "cpu/HX/Ucore.hh"

#include "base/logging.hh"
#include "debug/Ucore.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

Ucore::Ucore(const UcoreParams &p)
    : TickedObject(p),
      ibuffer(p.ibuffer),
      currentPC(p.initial_pc),
      fetchCount(p.fetch_count),
      ibufferLineSize(p.cacheLineSize),
      fetchSize(p.fetchSize),
      num_thread(p.num_thread)
{
    fatal_if(!ibuffer, "Ucore requires an Ibuffer");
    fatal_if(fetchCount == 0, "Ucore fetch_count must be greater than zero");
    fatal_if(fetchSize == 0, "Ucore fetchSize must be greater than zero");
    fatal_if(ibufferLineSize == 0,
             "Ucore cacheLineSize must be greater than zero");
    fatal_if(ibufferLineSize % fetchSize != 0,
             "Ucore cacheLineSize must be a multiple of fetchSize");
    ibuffer->ucore = this;
}

void
Ucore::startup()
{
    ClockedObject::startup();
    DPRINTF(Ucore, "initial PC = %#x\n", currentPC);
    ibuffer->start();
    start();
}

Port &
Ucore::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "port") {
        panic_if(idx != InvalidPortID, "Ucore port is not a vector port");
        return ibuffer->getPort();
    }

    return ClockedObject::getPort(if_name, idx);
}

void
Ucore::evaluate()
{
    const IbufferOut &ibuffer_out = ibuffer->output();

    PC pc = outputBuffer.read(curCycle()).pc_data;

    auto &out = outputBuffer.write(curCycle());
    out = UcoreOut{};
    out.pc_vld = true;
    out.pc_data = pc;

    if (ibuffer_out.ins_vld) {
        DPRINTF(Ucore, "fetched instruction @ PC %#x: %s\n", pc,
                ibuffer_out.ins_data.toString().c_str());
        out.pc_data = pc + fetchSize;
        ++fetchedCount;
    }

    if (fetchedCount >= fetchCount) {
        stop();
        ibuffer->stop();
        exitSimLoop("Ucore finished fetching requested machine-code words");
    }
}

} // namespace gem5