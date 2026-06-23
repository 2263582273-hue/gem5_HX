#include "cpu/HX/MEM/Ibuffer.hh"

#include <cstring>
#include "cpu/HX/Ucore.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "cpu/HX/Ucore.hh"
#include "mem/request.hh"

namespace gem5
{

Ibuffer::Ibuffer(Ucore &ucore, Addr cache_line_size)
    : ucore(ucore),
      cacheLineSize(cache_line_size),
      ibufferPort(ucore.name() + ".ibuffer_port", *this)
{
    fatal_if(cacheLineSize == 0 || !isPowerOf2(cacheLineSize),
             "Ibuffer cache line size must be a non-zero power of two");
}

Ibuffer::~Ibuffer()
{
    delete retryPkt;
    delete[] line0.inst;
    delete[] line1.inst;
}

Ibuffer::Ibuffercacheline
Ibuffer::fetchIbuffer(Addr paddr)
{
    const Addr line_addr = paddr & ~(cacheLineSize - 1);

    if (line0.valid && line0.lineAddr == line_addr)
        return line0;
    if (line1.valid && line1.lineAddr == line_addr)
        return line1;

    // This is a blocking Ibuffer: only one line fill may be outstanding.
    if (requestOutstanding || retryPkt)
        return {};

    RequestPtr request = std::make_shared<Request>(
        line_addr, cacheLineSize, Request::INST_FETCH,
        ucore.instRequestorId());
    PacketPtr pkt = Packet::createRead(request);
    pkt->allocate();

    outstandingLineAddr = line_addr;
    if (ibufferPort.sendTimingReq(pkt)) {
        requestOutstanding = true;
    } else {
        retryPkt = pkt;
    }

    return {};
}

bool
Ibuffer::recvTimingResp(PacketPtr pkt)
{
    fatal_if(!requestOutstanding,
             "Ibuffer received a response without an outstanding request");
    fatal_if(pkt->isError(), "Ibuffer received an error response");

    const Addr line_number = outstandingLineAddr / cacheLineSize;
    Ibuffercacheline &line = (line_number & 1) ? line1 : line0;

    delete[] line.inst;
    line.inst = new uint8_t[cacheLineSize];
    std::memcpy(line.inst, pkt->getConstPtr<uint8_t>(), cacheLineSize);
    line.lineAddr = outstandingLineAddr;
    line.valid = true;

    delete pkt;
    requestOutstanding = false;
    return true;
}

void
Ibuffer::recvReqRetry()
{
    if (retryPkt && ibufferPort.sendTimingReq(retryPkt)) {
        retryPkt = nullptr;
        requestOutstanding = true;
    }
}

} // namespace gem5
