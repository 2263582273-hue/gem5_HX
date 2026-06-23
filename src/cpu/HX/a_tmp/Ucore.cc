#include "cpu/HX/Ucore.hh"

#include <cstdint>
#include <cstring>

#include "base/logging.hh"
#include "debug/Ucore.hh"
#include "mem/se_translating_port_proxy.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

bool
Ucore::CpuPort::recvTimingResp(PacketPtr pkt)
{
    // Functional fetches do not use this path yet. Keep valid BaseCPU ports.
    delete pkt;
    return true;
}

Ucore::Ucore(const UcoreParams &p)
    : BaseCPU(p),
      Ticked(*this, &baseStats.numCycles),
      thread(nullptr),
      instPort(name() + ".icache_port"),
      dataPort(name() + ".dcache_port"),
      fetchCount(p.fetch_count)
{
    fatal_if(FullSystem, "Ucore currently supports SE mode only");
    fatal_if(numThreads != 1, "Ucore currently supports one thread only");
    fatal_if(p.workload.size() != 1, "Ucore requires exactly one workload");
    fatal_if(!p.mmu, "Ucore requires an MMU");
    fatal_if(p.isa.size() != 1, "Ucore requires exactly one ISA object");
    fatal_if(p.decoder.size() != 1,
             "Ucore requires exactly one instruction decoder");
    fatal_if(fetchCount == 0, "Ucore fetch_count must be greater than zero");

    thread = new SimpleThread(
        this, 0, p.system, p.workload[0], p.mmu, p.isa[0], p.decoder[0]);
    threadContexts.push_back(thread->getTC());
}

Ucore::~Ucore()
{
    delete thread;
}

void
Ucore::init()
{
    BaseCPU::init();
}

void
Ucore::startup()
{
    BaseCPU::startup();
    currentPC = thread->pcState().instAddr();
    inform("Ucore ELF entry PC = %#x", currentPC);
    start();
}

void
Ucore::regStats()
{
    BaseCPU::regStats();
    Ticked::regStats();
}

void
Ucore::serialize(CheckpointOut &cp) const
{
    BaseCPU::serialize(cp);
    Ticked::serialize(cp);
}

void
Ucore::unserialize(CheckpointIn &cp)
{
    BaseCPU::unserialize(cp);
    Ticked::unserialize(cp);
}

void
Ucore::serializeThread(CheckpointOut &cp, ThreadID tid) const
{
    fatal_if(tid != 0, "Ucore received invalid thread ID %u", tid);
    thread->serialize(cp);
}

void
Ucore::unserializeThread(CheckpointIn &cp, ThreadID tid)
{
    fatal_if(tid != 0, "Ucore received invalid thread ID %u", tid);
    thread->unserialize(cp);
}

void
Ucore::evaluate()
{
    constexpr unsigned FetchSize = sizeof(uint32_t);
    uint8_t bytes[FetchSize] = {};

    SETranslatingPortProxy proxy(
        thread->getTC(), SETranslatingPortProxy::Never);
    proxy.readBlob(currentPC, bytes, FetchSize);

    uint32_t machineCode = 0;
    std::memcpy(&machineCode, bytes, sizeof(machineCode));

    inform("Ucore fetch[%u]: PC=%#x machine_code=%#010x",
           fetchedCount, currentPC, machineCode);
    DPRINTF(Ucore, "fetch[%u] PC=%#x machine_code=%#010x\n",
            fetchedCount, currentPC, machineCode);

    currentPC += FetchSize;
    ++fetchedCount;

    if (fetchedCount >= fetchCount) {
        stop();
        exitSimLoop("Ucore finished fetching ELF machine code");
    }
}

void
Ucore::wakeup(ThreadID tid)
{
    fatal_if(tid != 0, "Ucore received invalid thread ID %u", tid);
    if (thread->status() == ThreadContext::Suspended)
        thread->activate();
    start();
}

} // namespace gem5
