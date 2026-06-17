#include "sim/ticked_object.hh"
#include "params/Ucore.hh"

namespace gem5 {

class Ucore: public TickedObject {
        
public:
    Ucore(const UcoreParams &p);
    ~Ucore(); 

    void evaluate() override;

    int pc;

};
}