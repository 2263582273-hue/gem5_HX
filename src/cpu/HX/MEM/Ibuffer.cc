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

Ibuffer::WarpState::WarpState(unsigned line_size)
{
    lines.reserve(LinesPerWarp);
    for (unsigned i = 0; i < LinesPerWarp; ++i)
        lines.emplace_back(line_size);
}

Ibuffer::L2Port::L2Port(const std::string &name, Ibuffer &owner,
                        PortID bank_id)
    : RequestPort(name, bank_id), ibuffer(owner), bankId(bank_id)
{
}

bool
Ibuffer::L2Port::recvTimingResp(PacketPtr pkt)
{
    return ibuffer.recvTimingResp(bankId, pkt);
}

void
Ibuffer::L2Port::recvReqRetry()
{
    ibuffer.recvReqRetry(bankId);
}

Ibuffer::Ibuffer(const std::string &name, unsigned num_warps,
                 unsigned line_size, unsigned inst_size,
                 RequestorID requestor_id, unsigned num_banks)
    : lineBytes(line_size), instructionBytes(inst_size),
      requestorId(requestor_id), banks(num_banks)
{
    panic_if(num_warps == 0, "IBuffer requires at least one warp");
    panic_if(num_warps >
                 static_cast<unsigned>(std::numeric_limits<ThreadID>::max()),
             "IBuffer warp count exceeds the ThreadID range");
    panic_if(lineBytes == 0 || (lineBytes & (lineBytes - 1)) != 0,
             "IBuffer line size must be a non-zero power of two");
    panic_if(instructionBytes == 0 || instructionBytes > lineBytes ||
                 lineBytes % instructionBytes != 0,
             "IBuffer instruction size must divide the cache-line size");
    panic_if(num_banks == 0, "IBuffer requires at least one L2 bank");
    panic_if(num_banks >
                 static_cast<unsigned>(std::numeric_limits<PortID>::max()),
             "IBuffer bank count exceeds the PortID range");

    warps.reserve(num_warps);
    for (unsigned i = 0; i < num_warps; ++i)
        warps.emplace_back(lineBytes);

    ports.reserve(num_banks);
    for (unsigned bank = 0; bank < num_banks; ++bank) {
        ports.push_back(std::make_unique<L2Port>(
            name + ".ibuffer_l2_port[" + std::to_string(bank) + "]", *this,
            static_cast<PortID>(bank)));
    }
}

Ibuffer::~Ibuffer()
{
    // Packets accepted by a lower level are owned by the memory system and
    // return through recvTimingResp.  Only packets still queued locally belong
    // to the IBuffer at destruction.
    for (auto &bank : banks) {
        while (!bank.sendQueue.empty()) {
            discardQueuedPacket(bank.sendQueue.front());
            bank.sendQueue.pop_front();
        }
    }
}

Ibuffer::AccessStatus
Ibuffer::requestInstruction(ThreadID warp_id, Addr pc, uint8_t *inst)
{
    panic_if(inst == nullptr, "IBuffer received a null instruction buffer");
    panic_if(pc % instructionBytes != 0,
             "IBuffer PC %#x is not aligned to %u-byte instructions", pc,
             instructionBytes);

    WarpState &warp = warpState(warp_id);
    warp.refillReady = false;

    if (warp.error)
        return AccessStatus::Error;

    if (warp.blocked) {
        return pc == warp.pendingPc ? AccessStatus::Pending
                                    : AccessStatus::Blocked;
    }

    CacheLine &line = warp.lines[lineIndex(pc)];
    if (line.valid && line.tag == lineTag(pc)) {
        const size_t offset = static_cast<size_t>(pc - lineAddress(pc));
        assert(offset + instructionBytes <= line.data.size());
        std::memcpy(inst, line.data.data() + offset, instructionBytes);
        return AccessStatus::Hit;
    }

    startMiss(warp_id, pc);
    return AccessStatus::Miss;
}

bool
Ibuffer::isBlocked(ThreadID warp_id) const
{
    return warpState(warp_id).blocked;
}

Addr
Ibuffer::pendingPc(ThreadID warp_id) const
{
    const WarpState &warp = warpState(warp_id);
    panic_if(!warp.blocked, "Warp %d has no pending IBuffer request", warp_id);
    return warp.pendingPc;
}

bool
Ibuffer::refillReady(ThreadID warp_id) const
{
    return warpState(warp_id).refillReady;
}

bool
Ibuffer::takeRefilledInstruction(ThreadID warp_id, uint8_t *inst)
{
    panic_if(inst == nullptr, "IBuffer received a null instruction buffer");

    WarpState &warp = warpState(warp_id);
    if (!warp.refillReady)
        return false;

    CacheLine &line = warp.lines[lineIndex(warp.pendingPc)];
    panic_if(!line.valid || line.tag != lineTag(warp.pendingPc),
             "IBuffer completed refill is not present in the selected line");

    const size_t offset =
        static_cast<size_t>(warp.pendingPc - lineAddress(warp.pendingPc));
    assert(offset + instructionBytes <= line.data.size());
    std::memcpy(inst, line.data.data() + offset, instructionBytes);
    warp.refillReady = false;
    return true;
}

void
Ibuffer::clearError(ThreadID warp_id)
{
    WarpState &warp = warpState(warp_id);
    warp.error = false;
    warp.refillReady = false;
}

void
Ibuffer::reset()
{
    for (size_t warp_id = 0; warp_id < warps.size(); ++warp_id)
        invalidateWarp(static_cast<ThreadID>(warp_id));
}

void
Ibuffer::invalidateWarp(ThreadID warp_id)
{
    WarpState &warp = warpState(warp_id);

    for (auto &line : warp.lines)
        line.valid = false;

    // Incrementing the generation makes any already in-flight response stale.
    // Such a response is accepted from the port but discarded.
    ++warp.generation;
    warp.blocked = false;
    warp.refillReady = false;
    warp.error = false;
    warp.pendingPc = 0;
    warp.pendingLine = 0;
}

Port &
Ibuffer::getPort(PortID bank_id)
{
    panic_if(bank_id < 0 || static_cast<size_t>(bank_id) >= ports.size(),
             "Invalid IBuffer L2 bank port %d", bank_id);
    return *ports[bank_id];
}

Addr
Ibuffer::lineAddress(Addr pc) const
{
    return pc & ~static_cast<Addr>(lineBytes - 1);
}

unsigned
Ibuffer::lineIndex(Addr pc) const
{
    const Addr line_number = lineAddress(pc) / lineBytes;
    return static_cast<unsigned>(line_number % LinesPerWarp);
}

Addr
Ibuffer::lineTag(Addr pc) const
{
    const Addr line_number = lineAddress(pc) / lineBytes;
    return line_number / LinesPerWarp;
}

PortID
Ibuffer::bankIndex(Addr line_addr) const
{
    const Addr line_number = line_addr / lineBytes;
    return static_cast<PortID>(line_number % ports.size());
}

Ibuffer::WarpState &
Ibuffer::warpState(ThreadID warp_id)
{
    panic_if(warp_id < 0 || static_cast<size_t>(warp_id) >= warps.size(),
             "Invalid IBuffer warp ID %d", warp_id);
    return warps[warp_id];
}

const Ibuffer::WarpState &
Ibuffer::warpState(ThreadID warp_id) const
{
    panic_if(warp_id < 0 || static_cast<size_t>(warp_id) >= warps.size(),
             "Invalid IBuffer warp ID %d", warp_id);
    return warps[warp_id];
}

void
Ibuffer::startMiss(ThreadID warp_id, Addr pc)
{
    WarpState &warp = warpState(warp_id);
    assert(!warp.blocked);

    const Addr line_addr = lineAddress(pc);
    warp.blocked = true;
    warp.refillReady = false;
    warp.pendingPc = pc;
    warp.pendingLine = line_addr;

    RequestPtr request = std::make_shared<Request>(
        line_addr, lineBytes, Request::INST_FETCH, requestorId);
    PacketPtr packet = Packet::createRead(request);
    packet->allocate();
    packet->pushSenderState(
        new MissState(warp_id, line_addr, warp.generation));

    const PortID bank_id = bankIndex(line_addr);
    enqueueOrSend(bank_id, packet);
}

void
Ibuffer::enqueueOrSend(PortID bank_id, PacketPtr pkt)
{
    BankState &bank = banks[bank_id];

    // Once a port has been refused, gem5 requires us to wait for
    // recvReqRetry before attempting any more requests on that port.
    if (bank.waitingForRetry || !bank.sendQueue.empty()) {
        bank.sendQueue.push_back(pkt);
        return;
    }

    if (!ports[bank_id]->sendTimingReq(pkt)) {
        bank.waitingForRetry = true;
        bank.sendQueue.push_back(pkt);
    }
}

void
Ibuffer::sendQueuedRequests(PortID bank_id)
{
    BankState &bank = banks[bank_id];

    while (!bank.sendQueue.empty()) {
        PacketPtr packet = bank.sendQueue.front();
        if (!ports[bank_id]->sendTimingReq(packet)) {
            bank.waitingForRetry = true;
            return;
        }
        bank.sendQueue.pop_front();
    }
}

bool
Ibuffer::recvTimingResp(PortID bank_id, PacketPtr pkt)
{
    panic_if(pkt == nullptr, "IBuffer received a null response packet");
    panic_if(!pkt->isResponse(),
             "IBuffer bank %d received a non-response packet", bank_id);

    auto *miss = dynamic_cast<MissState *>(pkt->popSenderState());
    panic_if(miss == nullptr,
             "IBuffer response does not contain IBuffer sender state");

    const ThreadID warp_id = miss->warpId;
    const Addr response_line = miss->lineAddr;
    const uint64_t response_generation = miss->generation;
    delete miss;

    panic_if(bankIndex(response_line) != bank_id,
             "IBuffer response returned through the wrong L2 bank");

    WarpState &warp = warpState(warp_id);
    const bool current_request = warp.blocked &&
                                 warp.generation == response_generation &&
                                 warp.pendingLine == response_line;

    if (current_request && !pkt->isError()) {
        panic_if(!pkt->hasData() || pkt->getSize() != lineBytes,
                 "IBuffer refill has invalid payload size");

        CacheLine &line = warp.lines[lineIndex(response_line)];
        std::copy_n(pkt->getConstPtr<uint8_t>(), lineBytes, line.data.begin());
        line.tag = lineTag(response_line);
        line.valid = true;

        warp.blocked = false;
        warp.refillReady = true;
    } else if (current_request) {
        warp.blocked = false;
        warp.refillReady = false;
        warp.error = true;
    }
    // A response invalidated by reset/invalidateWarp is intentionally dropped.

    delete pkt;
    return true; // The IBuffer never applies response backpressure.
}

void
Ibuffer::recvReqRetry(PortID bank_id)
{
    panic_if(bank_id < 0 || static_cast<size_t>(bank_id) >= banks.size(),
             "Invalid IBuffer retry bank %d", bank_id);

    BankState &bank = banks[bank_id];
    panic_if(!bank.waitingForRetry, "Unexpected IBuffer retry from bank %d",
             bank_id);

    bank.waitingForRetry = false;
    sendQueuedRequests(bank_id);
}

void
Ibuffer::discardQueuedPacket(PacketPtr pkt)
{
    if (pkt->senderState) {
        auto *miss = dynamic_cast<MissState *>(pkt->popSenderState());
        panic_if(miss == nullptr,
                 "Queued IBuffer packet has unexpected sender state");
        delete miss;
    }
    delete pkt;
}

} // namespace gem5
