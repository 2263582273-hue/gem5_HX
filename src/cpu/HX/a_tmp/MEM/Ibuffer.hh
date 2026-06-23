#ifndef __CPU_HX_MEM_IBUFFER_HH__
#define __CPU_HX_MEM_IBUFFER_HH__

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/addr_range.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/port.hh"

namespace gem5
{
/*前向声明Ucore*/
class Ucore;

class Ibuffer{
    public:
        Ibuffer(Ucore &ucore,Addr cacheLineSize);
        ~Ibuffer();

    protected:
        Ucore &ucore;
    public:
        struct Ibuffercacheline {
            bool valid=false;
            Addr PC=0;
            /*inst是指向指令机器码的指针*/
            uint8_t *inst = nullptr;

        };
        /*每个Ibuffer中有两行cacheline*/
        Ibuffercacheline line0,line1;

    public:
        /*一级缓存是一个阻塞性缓存，当发生取指请求失效时会置高 warp_ack 信号，从而将 warp 发来的取指请求阻塞住，
        在对应的 cacheline 指令返回前 warp 发起的取指请求会一直置高有效且不应发生变化*/
        bool warp_ack;
        /*fetch_hit0对应line0是否命中，命中置为1；fetch_hit1对应line1是否命中*/
        bool fetch_hit0, fetch_hit1;
        Ibuffercacheline fetchIbuffer(Addr PC);

    
    protected:

    Addr cacheLineSize = 128;

    class Ibufferport : public RequestPort
    {
        protected:
            /*owner*/
            Ibuffer &ibuffer;

        public:
            /*portId应该和threadId一一对应*/
            PortID portId;
            Ibufferport(std::string name, Ibuffer &ibuffer,PortID portId):
            RequestPort(name),ibuffer(ibuffer),portId(portId)
            { }
        protected:
            bool recvTimingResp(PacketPtr pkt)
            {return ibuffer.recvTimingResp(pkt); }
            void recvReqRetry() {ibuffer.recvReqRetry(); }

    };
    Ibufferport ibufferport;
    PacketPtr retryPkt = nullptr;
    Addr outstandingPC = 0;
    bool recvTimingResp(PacketPtr pkt);
    void recvReqRetry();        



};


} // namespace gem5

#endif // __CPU_HX_MEM_IBUFFER_HH__
