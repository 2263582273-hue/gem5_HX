#include "cpu/HX/MEM/Ibuffer.hh"

#include <cstring>
#include <iomanip>
#include <sstream>
#include "base/types.hh"
#include "cpu/HX/Ucore.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "cpu/HX/data.hh"
#include "debug/Ibuffer.hh"
#include "mem/request.hh"

namespace gem5
{

Ibuffer::Ibuffer(const IbufferParams &params)
    : TickedObject(params),
      ucore(dynamic_cast<Ucore *>(params.ucore)),
      cacheLineSize(params.cache_line_size),
      ibufferPort((params.ucore ? params.ucore->name() : name()) +
                  ".ibuffer_port", *this)
{
    fatal_if(!ucore, "Ibuffer requires a Ucore parent");
    fatal_if(cacheLineSize == 0 || !isPowerOf2(cacheLineSize),
             "Ibuffer cache line size must be a non-zero power of two");
}

Ibuffer::~Ibuffer()
{
    delete retryPkt;
}
void
Ibuffer::evaluate()
{
    /*更新内部寄存器*/
    Reg &reg_read = reg.read(curCycle());
    Reg &reg_write = reg.write(curCycle());

    /*更新输入变量*/
    UcoreOut &input = ucore->outputBuffer.read(curCycle());
    L0bufferOut l0_input; //后续应该改成L0bufferOut &l0_input= l0->outputBuffer.read(curCycle());
    
    /*获得自身的输出寄存器端口*/
    IbufferOut &output = outputBuffer.write(curCycle());

    if (!input.pc_vld) {
        output.ins_vld=0;
        return;
    }
    //计算Pc所在行号和偏置 _n是now的意思
    Addr line_n = input.pc_data/cacheLineSize;
    Addr offset_n = input.pc_data % cacheLineSize;

    //预取的行 _p是prefetch的意思
    Addr line_p = line_n + 1;

    //奇偶变换
        // 读
    bool is_odd = line_n & 1;
    Ibuffercacheline & cacheline_n = (is_odd)? reg_read.line1 : reg_read.line0;
    Ibuffercacheline & cacheline_p = (!is_odd)? reg_read.line1 : reg_read.line0;
    Oldmiss & miss_n = (is_odd)? reg_read.miss1 : reg_read.miss0;
    Oldmiss & miss_p = (!is_odd)? reg_read.miss1 : reg_read.miss0;
        // 写
    Ibuffercacheline &cacheline_n_w = is_odd ? reg_write.line1 : reg_write.line0;
    Ibuffercacheline &cacheline_p_w = is_odd ? reg_write.line0 : reg_write.line1;
    Oldmiss &miss_n_w = is_odd ? reg_write.miss1 : reg_write.miss0;
    Oldmiss &miss_p_w = is_odd ? reg_write.miss0 : reg_write.miss1;

    //计算fet_hit和pref_hit
    bool fet_hit = cacheline_n.valid && (cacheline_n.lineAddr == line_n);
    bool pref_hit = cacheline_p.valid && (cacheline_p.lineAddr == line_p);
    
    //更改内部的寄存器

    if (!fet_hit) {
        if (miss_n.valid && (miss_n.lineAddr == line_n)) {}
        else {
            /* 预留函数位，根据冲裁结果决定是否写入miss */
            miss_n_w.valid = 1;
            miss_n_w.lineAddr = line_n;
        }
    }

    if (!pref_hit) {
        if (miss_p.valid && (miss_p.lineAddr == line_p)) {}
        else {
            /* 预留函数位，根据冲裁结果决定是否写入miss */
            miss_p_w.valid = 1;
            miss_p_w.lineAddr = line_n;
        }
    }
    
    if (l0_input.L0_ivld[0] || l0_input.L0_ivld[1]) {
        if (l0_input.L0_ivld[0]) {
            if (reg_read.miss0.valid && (reg_read.miss0.lineAddr == l0_input.L0_iaddr[0])) {
                reg_write.miss0.valid = 0;
                reg_write.line0.valid = 1;
                reg_write.line0.lineAddr = l0_input.L0_iaddr[0];
                reg_write.line0.inst = l0_input.L0_idata[0];
            }
        }
        if (l0_input.L0_ivld[1]) {
            if (reg_read.miss0.valid && (reg_read.miss0.lineAddr == l0_input.L0_iaddr[1])) {
                reg_write.miss0.valid = 0;
                reg_write.line0.valid = 1;
                reg_write.line0.lineAddr = l0_input.L0_iaddr[1];
                reg_write.line0.inst = l0_input.L0_idata[1];
            }
        }
        
    }

    //更新本周期的输出信号
    output.ins_vld = fet_hit;
    std::memcpy(output.ins_data.bytes.data(), cacheline_n.inst + offset_n, Inst::Size);
    output.L0_ovld[0] = reg_read.miss0.valid;
    output.L0_oaddr[0] = reg_read.miss0.lineAddr;
    output.L0_ovld[1] = reg_read.miss1.valid;
    output.L0_oaddr[1] = reg_read.miss1.lineAddr;
    
    
 

    

    // if (!is_odd && reg_read.line0.valid && reg_read.line0.lineAddr == pc_line) {
    //     output.ins_vld = 1;
    //     std::memcpy(output.ins_data.bytes.data(), reg_read.line0.inst + line_offset, Inst::Size);
    //     return;
    // } else if (is_odd && reg_read.line1.valid && reg_read.line1.lineAddr == pc_line) {
    //     output.ins_vld = 1;
    //     std::memcpy(output.ins_data.bytes.data(), reg_read.line1.inst + line_offset, Inst::Size);
    //     return;
    // } else {
    //     /*发生miss的处�?/
    //     if (reg_read.miss
    // }

    // // line0/line1 are register-like state; inherit last cycle first.
    // auto &lines = lineBuffer.write(curCycle());
    // lines = lineBuffer.read(curCycle());

    // // Read Ucore's stable PC output from the previous cycle.
    // const UcoreOut &ucore_out = ucore->output();

    // auto &out = outputBuffer.write(curCycle());
    // out = IbufferOut{};

    // // No valid PC means no valid instruction output this cycle.
    // if (!ucore_out.pc_vld)
    //     return;

    // const Addr line_addr = ucore_out.pc_data;
    // const Ibuffercacheline *hit_line = nullptr;
    // if (lines.line0.valid && lines.line0.lineAddr == line_addr) {
    //     hit_line = &lines.line0;
    // } else if (lines.line1.valid && lines.line1.lineAddr == line_addr) {
    //     hit_line = &lines.line1;
    // }

    // if (!hit_line) {
    //     fetchIbuffer(line_addr);
    //     return;
    // }

    // out.ins_vld = true;
    // out.pc = ucore_out.pc_data;
    // std::memcpy(out.ins_data.bytes.data(), hit_line->inst, Inst::Size);
}

Ibuffer::Ibuffercacheline
Ibuffer::fetchIbuffer(Addr paddr)
{
    // // Align paddr down to the containing cache-line base address.
    // const Addr line_addr = paddr & ~(cacheLineSize - 1);

    /*这里认为pc的地址和cacheline的行地址永远是对齐的*/
    const Addr line_addr = paddr ;
    const auto &lines = lineBuffer.read(curCycle(), Cycles(0));
    if (lines.line0.valid && lines.line0.lineAddr == line_addr)
        return lines.line0;
    if (lines.line1.valid && lines.line1.lineAddr == line_addr)
        return lines.line1;

    // This is a blocking Ibuffer: only one line fill may be outstanding.
    if (requestOutstanding || retryPkt)
        return {};

    RequestPtr request = std::make_shared<Request>(
        line_addr, cacheLineSize, Request::INST_FETCH,
        ucore->instRequestorId());
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
    const unsigned slot = (line_number & 1) ? 1 : 0;
    auto &lines = lineBuffer.write(curCycle());
    lines = lineBuffer.read(curCycle());
    Ibuffercacheline &line = slot ? lines.line1 : lines.line0;

    lineStorage[slot] = std::make_unique<uint8_t[]>(cacheLineSize);
    std::memcpy(lineStorage[slot].get(), pkt->getConstPtr<uint8_t>(),
                cacheLineSize);
    line.inst = lineStorage[slot].get();
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
