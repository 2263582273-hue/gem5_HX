#ifndef __CPU_HX_UCORE_HH__
#define __CPU_HX_UCORE_HH__

#include "cpu/HX/MEM/Ibuffer.hh"
#include "cpu/base.hh"
#include "cpu/simple_thread.hh"
#include "mem/port.hh"
#include "params/Ucore.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

/** Fetch-only CPU: functional address translation plus timing Ibuffer access. */
class Ucore : public BaseCPU, public Ticked
{
  private:
    class DataPort : public RequestPort
    {
      public:
        explicit DataPort(const std::string &name) : RequestPort(name) {}

      protected:
        bool recvTimingResp(PacketPtr pkt) override
        {
            delete pkt;
            return true;
        }
        void recvReqRetry() override {}
    };

    SimpleThread *thread;
    DataPort dataPort;
    Ibuffer ibuffer;

    Addr currentPC = 0;
    const unsigned fetchCount;
    const Addr ibufferLineSize;
    const Addr fetchSize;
    unsigned fetchedCount = 0;

    Addr translateInstAddr(Addr vaddr);

  protected:
    Port &getInstPort() override { return ibuffer.getPort(); }
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
