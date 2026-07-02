#include "cpu/HX/MEM/L0cache.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/L0cache.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/mshr.hh"

namespace gem5
{

RoundRobinArbiter::RoundRobinArbiter(std::size_t num_ports)
    : portCount(num_ports)
{
    fatal_if(portCount == 0,
             "RoundRobinArbiter must have at least one input port");
}

int
RoundRobinArbiter::arbitrate(const std::vector<bool> &valid)
{
    for (std::size_t offset = 0; offset < portCount; ++offset) {
        const std::size_t port = (nextPriority + offset) % portCount;
        if (valid[port]) {
            nextPriority = (port + 1) % portCount;
            return static_cast<int>(port);
        }
    }

    return -1;
}

L0cache::L0cache(const L0cacheParams &params)
    : Cache(params),
      Ticked(static_cast<ClockedObject &>(*this)),
      arbiter(params.num_arbiter_ports)
{
    fatal_if(params.num_arbiter_ports != NumReadRequestPorts,
             "L0cache requires exactly %zu arbiter ports, got %zu",
             NumReadRequestPorts,
             static_cast<std::size_t>(params.num_arbiter_ports));
}

void
L0cache::setUcore(Ucore *ptr)
{
    fatal_if(!ptr, "L0cache cannot bind to a null Ucore");
    fatal_if(ucore && ucore != ptr,
             "L0cache is already bound to another Ucore");
    ucore = ptr;
}

void
L0cache::startup()
{
    Cache::startup();
    DPRINTF(L0cache, "starting L0 cache\n");
    start();
}

void
L0cache::evaluate()
{
    //控制逻辑如下，每周期依次执行下列行为:

    //一、处理当拍输入请求
    //令L0cache与 Ibuffer_num 个Ibuffer相连，每个Ibuffer有2个请求端口，每个请求端口绑定请求队列request_queue，因此共有 2*Ibuffer_num 个请求队列
        //1.1. 从每个Ibuffer的2个request_queue中返回一个请求，若均非空则 “非预取” 优先级高于 “预取”，若均为空则返回无效请求；初始请求状态为“request”
        //1.2. “prefetch”单元: 内部存有 Ibuffer_num 个请求，分别为各Ibuffer request_queue中上一个被“弹出”的请求。"prefetch"根据输入的请求是否有效、以及内部请求的 prf_num 值每周期输出 Ibuffer_num 个请求，若不足同样用无效请求占位。
        //1.3. “hit_check”单元：
            //1.3.1 对输入的有效请求，访问tag表，得到hit和miss请求；更新LRU_CNT；
            //1.3.2 hit且为预取的请求：该请求来源有2处，其一是来自request_queue，其二是“prefetch”单元产生的。第一种将其从队列中“弹出” (被“弹出”的请求会自动进入2中的“prefetch”单元，prf_num为0，后续不赘述)。第二种情况将该请求prf_num+1，若溢出则置为无效。
        //1.4. “bankconflict”单元：
            //1.4.1 计算各有效hit请求的bank_index值，计算过程见附1。对冲突的请求“仲裁”。
            //1.4.2 仲裁出的hit请求优先级最高，不会阻塞，因此从request_queue中“弹出”并输出给SRAM，请求状态为“issue_read”。
        //1.5. "miss merge"单元：
            //1.5.1 “仲裁”出一个有效miss请求，“非预取” 优先级高于 “预取”。并找出与该请求相同line_addr的miss请求。这些请求的来源同样有2处，其一是来自request_queue，其二是“prefetch”单元产生的。对第一种将它们从队列中“弹出”，第二种情况prf_num保持不变。
            //1.5.2 将这些请求合并为一个新的请求，输出;请求状态为“miss”


    //二、miss process
        //2.1 检查"miss merge"单元的有效输出。若已有同 line MSHR，则与之合并，否则新建缺失项。(是否要与MSHR合并?)
        //2.2 “仲裁” MSHR所有缺失项，并读取LRU_CNT，若仲裁出的请求无分配空间(LRU_CNT在该group全为1)，则不断仲裁直至满足条件。将最终请求发送至下级内存。将该请求状态改为“inflight”。
        //2.3 检查当拍返回数据，复制到MSHR对应请求的数据空间中，同时令FIFO_occupation = FIFO_occupation + bank_width，代表FIFO占用。检查last信号，若有效则将该请求从MSHR中移出并插入待写队列write_queue中，请求状态为“issue_write”，同时FIFO_occupation = FIFO_occupation - cachelinesize；
        //2.4 检查写回队列write_queue，若非空则“仲裁”出1个写请求(是仲裁出1个写请求还是每个bank 1个写请求?)输出给SRAM。
        //2.5 检查重发射队列reissue_queue，若非空则结合1.3.1中的hit情况“仲裁”出1个读请求输出给SRAM。请求状态为“reissue_read”
                //(2.4和2.5中仲裁出新的请求时，是否要“非预取”的写回和重发射优先?)

    //三、读写SRAM
        //3.1 检查1.4.2、2.4和2.5中的输出，对它们进行逐bank仲裁，优先级为“issue_read”>“reissue_read”>“issue_write”。
        //3.2 根据仲裁结果对每个bank读/写数据
        //3.3 读写完毕后，根据请求的来源，进行如下操作：(a) 若请求来自1.4.2(hit的请求)，则向上级返回数据；(b) 若请求来自2.4(write_queue)，则将请求从队列中移出并送入reissue_queue，改变请求状态；更新tag表和LRU_CNT；(c) 若请求来自2.5(reissue_queue)，则将请求从队列中移出，并向上级返回数据。

    //四、全局计数器LRU_CNT
        //4.1 LRU_CNT逐拍更新，若某set当拍各way均为1，则置为0.

    //附1：地址计算
    /*对于完整地址addr(单位为byte)，其地址转换过程如下：

        行地址line_addr = addr / cachelinesize (e.g.512，4096bit=512B)
        行偏移line_offset = addr % cachelinesize

        tag = line_addr / set_num (set_num是group数量，e.g.32)
        index = line_addr % set_num

        bank编号bank_index = line_offset / bank_width (bank_width时bank位宽，e.g.128B)
        bank偏移bank_offset = line_offset % bank_width
    */

    //附2：请求Request的描述性模型
    /*
        class L0Request
    {
        public:
            bool valid = false;
            //请求状态和来源
            RequestState state = RequestState::Invalid;
            RequestSource source = RequestSource::RequestQueue;

            // Cacheline地址信息
            Addr lineAddr = 0;
            Addr setIndex = 0;
            Addr tag = 0;

            // 预取深度。仅prefetch单元产生的请求递增
            uint32_t prfNum = 0;

            // MSHR及下级内存事务标识
            int mshrId = -1;
            uint64_t transactionId = 0;

            // miss分配的目标way；分配前为-1
            int victimWay = -1;

            // 所有合并到该cacheline事务的原始请求
            std::vector<RequestTarget> targets;

            // 下级内存返回的完整cacheline
            std::vector<uint8_t> lineData;

            // 每个bank_width是否已经返回；
            std::vector<bool> receivedMask;

    };
    对于hit请求(非预取)，其状态变化为: Request->IssueRead->Complete        
    对于miss请求，其状态变化为: Request->Miss->Inflight->IssueWrite->ReissueRead-> Complete
    */





    // std::vector<bool> valid(NumReadRequestPorts, false);
    // for (std::size_t port = 0; port < NumReadRequestPorts; ++port) {
    //     valid[port] = !readRequestPorts[port].empty();
    // }

    // const int selected = arbitrate(valid);
    // if (selected < 0) {
    //     return;
    // }

    // // This example only observes the request. A real implementation should
    // // remove it after the hit response or miss transaction is accepted.
    // const Readrequest &req = readRequestPorts[selected].front();
    // CacheBlk *blk = findCacheLine(req.addr);

    // if (blk) {
    //     // A cache-line read: blk->data points to blkSize (512) bytes.
    //     const uint8_t first_byte = blk->data[0];
    //     DPRINTF(L0cache,
    //             "port %d hit addr=%#x, first byte=%#x\n",
    //             selected, req.addr, first_byte);

    //     // For a writable cache, writebackCacheLine(blk, clockEdge())
    //     // evicts the line and queues the generated writeback packet. This
    //     // L0 is read-only, so calling it here would correctly fail.
    //     return;
    // }

    // if (MSHR *mshr = findMshr(req.addr)) {
    //     DPRINTF(L0cache, "port %d addr=%#x already has MSHR %p\n",
    //             selected, req.addr, mshr);
    //     return;
    // }

    // DPRINTF(L0cache,
    //         "port %d miss addr=%#x; allocateMshr() needs the original "
    //         "PacketPtr from the requester\n",
    //         selected, req.addr);
}

CacheBlk *
L0cache::findCacheLine(Addr addr, bool is_secure) const
{
    CacheBlk *blk = tags->findBlock({addr, is_secure});
    return blk && blk->isValid() && blk->isSet(CacheBlk::ReadableBit) ?
        blk : nullptr;

/*
class CacheBlk
{
    uint8_t *data; // cacheline 的实际数据

    // 继承自 TaggedEntry
    tag;           // 地址的 tag 部分
    valid;         // 当前 line 是否有效
    secure;        // 是否属于 Secure 地址空间

    // 一致性状态
    ReadableBit;
    WritableBit;
    DirtyBit;

    // 其他信息
    whenReady;     // 数据何时可访问
    prefetch 状态;
    replacement 状态;
};
CacheBlk = Cache line 的数据 + tag + valid + 一致性和替换元数据
*/
}

// CacheBlk *
// L0cache::fillCacheLine(PacketPtr fill_pkt, Tick ready_time)
// {
//     fatal_if(!fill_pkt || !fill_pkt->isResponse() || !fill_pkt->hasData(),
//              "L0cache fill requires a response packet containing data");

//     PacketList writebacks;
//     CacheBlk *blk = tags->findBlock(
//         {fill_pkt->getAddr(), fill_pkt->isSecure()});
//     blk = handleFill(fill_pkt, blk, writebacks, true);

//     for (PacketPtr writeback : writebacks) {
//         allocateWriteBuffer(writeback, ready_time);
//     }
//     return blk;

// }

// MSHR *
// L0cache::findMshr(Addr addr, bool is_secure) const
// {
//     const Addr block_addr = addr & ~(static_cast<Addr>(blkSize) - 1);
//     return mshrQueue.findMatch(block_addr, is_secure);
// }

// MSHR *
// L0cache::allocateMshr(PacketPtr pkt, Tick ready_time)
// {
//     fatal_if(!pkt, "L0cache cannot allocate an MSHR for a null packet");
//     fatal_if(mshrQueue.isFull(), "L0cache MSHR queue is full");
//     fatal_if(findMshr(pkt->getAddr(), pkt->isSecure()),
//              "L0cache already has an MSHR for address %#x", pkt->getAddr());

//     // allocateMissBuffer takes ownership of the outstanding transaction and
//     // schedules it on the memory-side request queue.
//     return allocateMissBuffer(pkt, ready_time);
// }

// void
// L0cache::writebackCacheLine(CacheBlk *blk, Tick ready_time)
// {
//     fatal_if(isReadOnly, "L0cache is read-only and cannot write back lines");
//     fatal_if(!blk || !blk->isValid(),
//              "L0cache cannot write back an invalid cache line");

//     PacketPtr writeback = evictBlock(blk);
//     if (writeback) {
//         allocateWriteBuffer(writeback, ready_time);
//     }
// }













///////////////////////////////////////////////////////////////////////////
void
L0cache::regStats()
{
    Ticked::regStats();
    Cache::regStats();
}

void
L0cache::serialize(CheckpointOut &cp) const
{
    Ticked::serialize(cp);
    Cache::serialize(cp);
}

void
L0cache::unserialize(CheckpointIn &cp)
{
    Ticked::unserialize(cp);
    Cache::unserialize(cp);
    
}

} // namespace gem5
