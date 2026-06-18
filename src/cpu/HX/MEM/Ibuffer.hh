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
        Ibuffer();
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
        Ibuffercacheline line1,line2;

    public:
        /*一级缓存是一个阻塞性缓存，当发生取指请求失效时会置低 warp_ack 信号，从而将 warp 发来的取指请求阻塞住，
        在对应的 cacheline 指令返回前 warp 发起的取指请求会一直置高有效且不应发生变化*/
        bool warp_ack;
        



};


} // namespace gem5

#endif // __CPU_HX_MEM_IBUFFER_HH__
