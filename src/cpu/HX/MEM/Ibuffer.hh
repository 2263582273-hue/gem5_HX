#ifndef __CPU_HX_MEM_IBUFFER_HH__
#define __CPU_HX_MEM_IBUFFER_HH__

#include <cstdint>
#include <string>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/port.hh"

namespace gem5
{

class Ucore;

class Ibuffer
{
  public:
    struct Ibuffercacheline
    {
        bool valid = false;
        Addr lineAddr = 0;
        uint8_t *inst = nullptr;
    };

    Ibuffer(Ucore &ucore, Addr cache_line_size);
    ~Ibuffer();

    /** Try to read the cache line containing the physical address. */
    Ibuffercacheline fetchIbuffer(Addr paddr);

    Port &getPort() { return ibufferPort; }

  private:
    class IbufferPort : public RequestPort
    {
      private:
        Ibuffer &ibuffer;

      public:
        IbufferPort(const std::string &name, Ibuffer &ibuffer)
            : RequestPort(name), ibuffer(ibuffer)
        {}

      protected:
        bool recvTimingResp(PacketPtr pkt) override
        {
            return ibuffer.recvTimingResp(pkt);
        }

        void recvReqRetry() override
        {
            ibuffer.recvReqRetry();
        }
    };

    Ucore &ucore;
    const Addr cacheLineSize;
    IbufferPort ibufferPort;

    Ibuffercacheline line0;
    Ibuffercacheline line1;

    bool requestOutstanding = false;
    PacketPtr retryPkt = nullptr;
    Addr outstandingLineAddr = 0;

    bool recvTimingResp(PacketPtr pkt);
    void recvReqRetry();
};

} // namespace gem5

#endif // __CPU_HX_MEM_IBUFFER_HH__
