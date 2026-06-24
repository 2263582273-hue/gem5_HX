#include "cpu/HX/MEM/Ibuffer.hh"

#include <cstring>
#include <iomanip>
#include <sstream>
#include "cpu/HX/Ucore.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "debug/Ibuffer.hh"
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
    // // Align paddr down to the containing cache-line base address.
    // const Addr line_addr = paddr & ~(cacheLineSize - 1);

    /*这里认为pc的地址和cacheline的行地址永远是对齐的*/
    const Addr line_addr = paddr ;
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

    std::ostringstream data;
    const uint8_t *pkt_data = pkt->getConstPtr<uint8_t>();
    for (Addr i = 0; i < cacheLineSize; ++i) {
        if (i != 0)
            data << ' ';
        data << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<unsigned>(pkt_data[i]);
    }

    DPRINTF(Ibuffer,
            "received cache line: addr=%#x size=%u slot=line%u data=%s\n",
            line.lineAddr, static_cast<unsigned>(cacheLineSize),
            (line_number & 1) ? 1 : 0, data.str());
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
