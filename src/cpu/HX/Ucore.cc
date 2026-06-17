#include "Ucore.hh"
#include "base/trace.hh"
#include "sim/ticked_object.hh"
#include "debug/Ucore.hh"
#include "sim/sim_exit.hh"

namespace gem5 {

Ucore::Ucore(const UcoreParams &p):
TickedObject(p) {
    pc=0;
}

Ucore::~Ucore() {

}

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
}