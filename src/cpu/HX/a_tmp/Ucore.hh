#ifndef __CPU_HX_UCORE_HH__
#define __CPU_HX_UCORE_HH__

#include "cpu/base.hh"
#include "cpu/simple_thread.hh"
#include "mem/port.hh"
#include "params/Ucore.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

/** Minimal fetch-only CPU. It does not decode or execute instructions yet. */
class Ucore : public BaseCPU, public Ticked
{
  private:
    class CpuPort : public RequestPort
    {
      public:
        explicit CpuPort(const std::string &name) : RequestPort(name) {}

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override {}
    };

    SimpleThread *thread;
    CpuPort instPort;
    CpuPort dataPort;

    Addr currentPC = 0;
    const unsigned fetchCount;
    unsigned fetchedCount = 0;

  protected:
    Port &getInstPort() override { return instPort; }
    Port &getDataPort() override { return dataPort; }

  public:
    explicit Ucore(const UcoreParams &p);
    ~Ucore() override;

    void init() override;
    void startup() override;
    void regStats() override;
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;
    void serializeThread(CheckpointOut &cp, ThreadID tid) const override;
    void unserializeThread(CheckpointIn &cp, ThreadID tid) override;

    void evaluate() override;
    void wakeup(ThreadID tid) override;

    Counter totalInsts() const override { return 0; }
    Counter totalOps() const override { return 0; }
};

} // namespace gem5

#endif // __CPU_HX_UCORE_HH__
