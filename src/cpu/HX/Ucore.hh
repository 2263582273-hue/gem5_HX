#include "sim/ticked_object.hh"
#include "params/Ucore.hh"

namespace gem5 {

class Ucore: public TickedObject {
        
public:
    Ucore(const UcoreParams &p);
    ~Ucore(); 

    void evaluate() override;
    void startup() override;

    //stopsim()是gem5退出仿真的函数
    void stopsim();
    int pc;

};
}