#ifndef __CPU_HX_MEM_IBUFFER_HH__
#define __CPU_HX_MEM_IBUFFER_HH__

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include "cpu/HX/data.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/port.hh"
#include "params/Ibuffer.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

class Ucore;

class Ibuffer : public TickedObject
{
  /*Ibuffer的内部寄存器*/
  public:

    //cache line 结构
    struct Ibuffercacheline
    {
        bool valid = false;
        Addr lineAddr = 0;
        uint8_t *inst = nullptr;
    };
    //记录是否有已发射的miss
    struct Oldmiss 
    {
      bool valid = false;
      Addr lineAddr = 0;
    };

    
    struct Reg
    {
        Ibuffercacheline line0;
        Ibuffercacheline line1;
        Oldmiss miss0;
        Oldmiss miss1;
    };

    //寄存器实例化
    TimeBuffer<Reg> reg;
    TimeBuffer<IbufferOut> outputBuffer;
  
  // Ibuffer initialization
    Ibuffer(const IbufferParams &params);
    ~Ibuffer();

    /** Try to read the cache line containing the physical address. */
    Ibuffercacheline fetchIbuffer(Addr paddr);
    
    const IbufferOut &
    output(Cycles delay = Cycles(1)) const
    {
        return outputBuffer.read(curCycle(), delay);
    }
    
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
    IbufferPort ibufferPort;
        bool requestOutstanding = false;
    PacketPtr retryPkt = nullptr;
    Addr outstandingLineAddr = 0;
    bool recvTimingResp(PacketPtr pkt);
    void recvReqRetry();
    std::array<std::unique_ptr<uint8_t[]>, 2> lineStorage;
  //Ibuffer内部配置
    const Addr cacheLineSize;

  //Ibuffer核心逻辑 
    void evaluate() override;


  /*外部连接模块*/
  public:
    Ucore *ucore;



};

} // namespace gem5

#endif // __CPU_HX_MEM_IBUFFER_HH__
