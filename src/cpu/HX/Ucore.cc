#include "cpu/HX/Ucore.hh"

#include <cstdint>
#include <cstring>

#include "base/logging.hh"
#include "debug/Ucore.hh"
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
      ibuffer(*this, p.cacheLineSize),
      fetchCount(p.fetch_count),
      ibufferLineSize(p.cacheLineSize),
      fetchSize(p.fetchSize)
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

    // Translation is functional: it changes no timing and returns immediately.
    const Addr physical_pc = translateInstAddr(currentPC);
    const auto line = ibuffer.fetchIbuffer(physical_pc);

    // A miss returns an invalid line. evaluate() will retry on the next cycle.
    if (!line.valid) {
        DPRINTF(Ucore,
                "Ibuffer miss or line fill pending: vPC=%#x pPC=%#x\n",
                currentPC, physical_pc);
        return;
    }

    const Addr line_offset = physical_pc - line.lineAddr;
    fatal_if(line_offset + fetchSize > ibufferLineSize,
             "Ucore instruction crosses an Ibuffer cache line");

    uint8_t *machine_code = new uint8_t[fetchSize];
    std::memcpy(machine_code, line.inst + line_offset, fetchSize);

    std::ostringstream code;
    for (Addr i = 0; i < fetchSize; ++i) {
        if (i != 0)
            code << ' ';
        code << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(machine_code[i]);
    }

    const std::string code_str = code.str();
    DPRINTF(Ucore,
        "fetch[%u] vPC=%#x pPC=%#x machine_code=%s\n",
        fetchedCount, currentPC, physical_pc, code_str.c_str());

    currentPC += fetchSize;
    ++fetchedCount;

    // 用完后
    delete[] machine_code;
    if (fetchedCount >= fetchCount) {
        stop();
        exitSimLoop("Ucore finished fetching 10 machine-code words");
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
