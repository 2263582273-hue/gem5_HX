#ifndef __CPU_HX_MEM_L0CACHE_HH__
#define __CPU_HX_MEM_L0CACHE_HH__

#include <cstddef>
#include <vector>

#include "params/L0cache.hh"
#include "sim/ticked_object.hh"

namespace gem5
{

class Ucore;

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


class L0cache : public TickedObject
{
  private:
    RoundRobinArbiter arbiter;
    Ucore *ucore = nullptr;

  public:
    explicit L0cache(const L0cacheParams &params);
    ~L0cache() override = default;

    void startup() override;
    void evaluate() override;

    void setUcore(Ucore *ptr);
    Ucore *getUcore() const { return ucore; }

    int arbitrate(const std::vector<bool> &valid)
    {
        return arbiter.arbitrate(valid);
    }
};

} // namespace gem5

#endif // __CPU_HX_MEM_L0CACHE_HH__
