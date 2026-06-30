#ifndef __CPU_HX_UCORE_HH__
#define __CPU_HX_UCORE_HH__

#include <cstdint>
#include <string>

#include "base/types.hh"
#include "cpu/HX/MEM/Ibuffer.hh"
#include "cpu/HX/data.hh"
#include "mem/port.hh"
#include "params/Ucore.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

class L0cache;

/** Fetch-only ticked object. It drives PC values without BaseCPU state. */
class Ucore : public TickedObject
{
  private:
    Ibuffer *ibuffer;
    L0cache *l0cache;

    Addr currentPC = 0;
    const unsigned fetchCount;
    const Addr ibufferLineSize;
    const Addr fetchSize;
    unsigned fetchedCount = 0;

    // Kept as a configuration parameter for later multi-thread work.
    const unsigned num_thread;

  public:
    explicit Ucore(const UcoreParams &p);
    ~Ucore() override = default;

    void startup() override;
    void evaluate() override;

    L0cache *getL0cache() const { return l0cache; }

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;

    // Output signals from Ucore to Ibuffer.
    TimeBuffer<UcoreOut> outputBuffer;

    const UcoreOut &
    output(Cycles delay = Cycles(1)) const
    {
        return outputBuffer.read(curCycle(), delay);
    }
};

} // namespace gem5

#endif // __CPU_HX_UCORE_HH__
