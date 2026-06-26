#include "cpu/HX/Ucore.hh"

#include <cstdint>

#include "base/logging.hh"
#include "mem/request.hh"
#include "sim/faults.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

Ucore::Ucore(const UcoreParams &p)
    : BaseCPU(p),
      Ticked(*this, &baseStats.numCycles),
      thread(nullptr),
      dataPort(name() + ".dcache_port"),
      ibuffer(p.ibuffer),
      fetchCount(p.fetch_count),
      ibufferLineSize(p.cacheLineSize),
      fetchSize(p.fetchSize),
      num_thread(p.num_thread)
{
    fatal_if(FullSystem, "Ucore currently supports SE mode only");
    fatal_if(numThreads != 1, "Ucore currently supports one thread only");
    fatal_if(p.workload.size() != 1, "Ucore requires exactly one workload");
    fatal_if(!p.mmu, "Ucore requires an MMU");
    fatal_if(!ibuffer, "Ucore requires an Ibuffer");
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
    ibuffer->start();
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

Addr
Ucore::translateInstAddr(Addr vaddr)
{
    RequestPtr req = std::make_shared<Request>();
    req->setContext(thread->contextId());
    req->setVirt(
        vaddr, sizeof(uint32_t), Request::INST_FETCH,
        instRequestorId(), vaddr);

    Fault fault = thread->mmu->translateFunctional(
        req, thread->getTC(), BaseMMU::Execute);
    fatal_if(fault != NoFault,
             "Ucore instruction address translation failed at %#x: %s",
             vaddr, fault->name());
    fatal_if(!req->hasPaddr(),
             "Ucore translation did not produce a physical address");

    return req->getPaddr();
}

void
Ucore::evaluate()
{
    // Read Ibuffer's stable output from the previous cycle.
    const IbufferOut &ibuffer_out = ibuffer->output();

    // pc_vld is always asserted; pc_data is Ucore's current PC.
    auto &out = outputBuffer.write(curCycle());
    out = UcoreOut{};

    out.pc_vld = true;
    out.pc_data = static_cast<PC>(currentPC);
    
    // Advance PC only when Ibuffer returned valid instruction data.
    if (ibuffer_out.ins_vld) {
        currentPC += 16;
        ++fetchedCount;
    }

    if (fetchedCount >= fetchCount) {
        stop();
        exitSimLoop("Ucore finished fetching requested machine-code words");
    }
}

void
Ucore::wakeup(ThreadID tid)
{
    fatal_if(tid != 0, "Ucore received invalid thread ID %u", tid);
    if (thread->status() == ThreadContext::Suspended)
        thread->activate();
    ibuffer->start();
    start();
}

} // namespace gem5
