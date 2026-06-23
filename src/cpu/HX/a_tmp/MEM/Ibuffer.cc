#include "cpu/HX/MEM/Ibuffer.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>

#include "base/logging.hh"
#include "mem/request.hh"

namespace gem5
{
Ibuffer::Ibuffer(Ucore &ucore, Addr cacheLineSize):
    ucore(ucore),
    cacheLineSize(cacheLineSize),
    ibufferport("ibuffer.icache_port", *this, 0) {
    warp_ack=false;
    fetch_hit0=false;
    fetch_hit1=false;    
}

Ibuffer::~Ibuffer()
{
    delete retryPkt;
    delete[] line0.inst;
    delete[] line1.inst;
}

Ibuffer::Ibuffercacheline Ibuffer::fetchIbuffer(Addr PC) {
    if (warp_ack) {
        fetch_hit0=false;
        fetch_hit1=false;
        Ibuffercacheline line;
        return line;
    }
    else {
        if (line0.valid&&(line0.PC==PC)) {
            fetch_hit0=true;
            return line0;
        } else if (line1.valid&&(line1.PC==PC)) {
            fetch_hit1=true;
            return line1;
        } else {
            warp_ack=true;
            fetch_hit0=false;
            fetch_hit1=false;

            outstandingPC = PC;
            RequestPtr request = std::make_shared<Request>(
                PC, cacheLineSize, Request::INST_FETCH,
                Request::funcRequestorId);
            PacketPtr pkt = Packet::createRead(request);
            pkt->allocate();
            if (!ibufferport.sendTimingReq(pkt))
                retryPkt = pkt;

            Ibuffercacheline line;
            return line;
        }
    }

}

bool
Ibuffer::recvTimingResp(PacketPtr pkt)
{
    const Addr lineNumber = outstandingPC / cacheLineSize;
    Ibuffercacheline &line = (lineNumber & 1) == 0 ? line0 : line1;

    delete[] line.inst;
    line.inst = new uint8_t[cacheLineSize];
    std::memcpy(line.inst, pkt->getConstPtr<uint8_t>(), cacheLineSize);
    line.PC = outstandingPC;
    line.valid = true;

    delete pkt;
    warp_ack = false;
    return true;
}

void
Ibuffer::recvReqRetry()
{
    if (retryPkt && ibufferport.sendTimingReq(retryPkt))
        retryPkt = nullptr;
}

} // namespace gem5
