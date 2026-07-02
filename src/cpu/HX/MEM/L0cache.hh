#ifndef __CPU_HX_MEM_L0CACHE_HH__
#define __CPU_HX_MEM_L0CACHE_HH__

#include <array>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

#include "base/types.hh"
#include "mem/cache/cache.hh"
#include "params/L0cache.hh"
#include "sim/ticked_object.hh"
#include "mem/cache/tags/base.hh"

namespace gem5
{

class Ucore;
class CacheBlk;
class MSHR;

/** 使用循环轮转优先级，从有效输入端口中选择一个端口。 */
class RoundRobinArbiter
{
  public:
    explicit RoundRobinArbiter(std::size_t num_ports);

    /**
     * 选择一个有效端口。
     *
     * 从当前最高优先级端口开始循环查找。成功授权后，获选端口的下一个
     * 端口将在下一次调用时具有最高优先级。没有端口有效时不改变优先级。
     *
     * @param valid 每个已配置输入端口对应一个有效位。
     * @return 获选端口编号；没有端口有效时返回 -1。
     */
    int arbitrate(const std::vector<bool> &valid);

    std::size_t numPorts() const { return portCount; }
    void reset() { nextPriority = 0; }

  private:
    const std::size_t portCount;
    std::size_t nextPriority = 0;
};

struct Readrequest
{
  Addr addr; //完整地址，单位是byte
  Addr lineaddr; // addr/linesize (addr>>9)
  Addr lineoffset; // addr%linesize (addr%2^9)
  Addr set; // lineaddr%setsize (lineaddr%32)
  Addr tag; // lineaddr/setsize (lineaddr/32)
  Addr bank; // lineoffset/bankwidth (lineoffset/2^7)
  Addr bankoffset; // lineoffset%bankwidth (lineoffset%2^7)

  bool isPre; //是否是预取的
  uint8_t port; //属于哪一个端口
};


class L0cache : public Cache, public Ticked
{
  //
  public:
    static constexpr std::size_t NumReadRequestPorts = 8;
    using ReadRequestQueue = std::queue<Readrequest>;

    // Each queue represents one independent external read-request port.
    std::array<ReadRequestQueue, NumReadRequestPorts> readRequestPorts;

  private:
    RoundRobinArbiter arbiter;
    Ucore *ucore = nullptr;

  public:
    explicit L0cache(const L0cacheParams &params);
    ~L0cache() override = default;

    void startup() override;
    void evaluate() override;

    using ClockedObject::regStats;
    using ClockedObject::serialize;
    using ClockedObject::unserialize;

    void regStats() override;
    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    void setUcore(Ucore *ptr);
    Ucore *getUcore() const { return ucore; }

    CacheBlk *findCacheLine(Addr addr, bool is_secure = false) const;
    CacheBlk *fillCacheLine(PacketPtr fill_pkt, Tick ready_time);
    MSHR *findMshr(Addr addr, bool is_secure = false) const;
    MSHR *allocateMshr(PacketPtr pkt, Tick ready_time);
    void writebackCacheLine(CacheBlk *blk, Tick ready_time);
    
    int arbitrate(const std::vector<bool> &valid)
    {
        return arbiter.arbitrate(valid);
        
    }
    
};

} // namespace gem5

#endif // __CPU_HX_MEM_L0CACHE_HH__
