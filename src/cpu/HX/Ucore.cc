#include "Ucore.hh"
#include "MEM/Ibuffer.hh"
#include "base/trace.hh"
#include "sim/ticked_object.hh"
#include "debug/Ucore.hh"
#include "sim/sim_exit.hh"

namespace gem5 {

Ucore::Ucore(const UcoreParams &p):
    TickedObject(p),
    cacheLineSize(p.cacheLineSize),
    NumThread(p.ThreadNum),
    fetchInfo(p.ThreadNum),
    ibuffer(*this, p.cacheLineSize),
    icacheport(p.name+ ".icache_port", *this, 0) {
    pc=0;
    
    }

Ucore::~Ucore() {

}

/*重写simobject的startup()，启动ucore*/
void Ucore::startup() {
    start();
}

void Ucore::evaluate() {
    if (pc>=100) {
        stop();
        stopsim();
    } else {
        pc++;
        DPRINTF(Ucore,"Ucore's PC is: %u\n",pc);
    }

}

void Ucore::stopsim() {
    exitSimLoop("Simulation complete!");
}

bool Ucore::recvTimingResp(PacketPtr pkt) {
    return true;    
}

void Ucore::recvReqRetry() {
    
}
}