#include "Ucore.hh"
#include "base/trace.hh"
#include "sim/ticked_object.hh"

namespace gem5 {

Ucore::Ucore(const UcoreParams &p):
TickedObject(p) {
    pc=0;
}

Ucore::~Ucore() {

}

void Ucore::evaluate() {
    if (pc>=100) {
        stop();
    } else {
        pc++;
        DPRINTF(Ucore,"Ucore's PC is: %u\n",pc);
    }

}
}