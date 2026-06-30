#ifndef __CPU_HX_data_HH__
#define __CPU_HX_data_HH__

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include "base/types.hh"
namespace  gem5{


    struct Inst
    {
        static constexpr unsigned Size = 16;
        std::array<uint8_t, Size> bytes{};

        std::string
        toString() const
        {
            std::ostringstream os;

            for (unsigned i = 0; i < Size; ++i) {
                if (i != 0)
                    os << ' ';

                os << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(bytes[i]);
            }

            return os.str();
        }
    };
    typedef uint32_t PC ;

    // template <class T, std::size_t Depth = 2>
    // class TimeBuffer
    // {
    //   private:
    //     static_assert(Depth >= 2, "TimeBuffer depth must be at least 2");

    //     std::array<T, Depth> slots{};

    //   public:
    //     const T &
    //     read(Cycles cycle, Cycles delay = Cycles(1)) const
    //     {
    //         const uint64_t cycle_value = static_cast<uint64_t>(cycle);
    //         const uint64_t delay_value = static_cast<uint64_t>(delay);
    //         const uint64_t read_cycle =
    //             cycle_value > delay_value ? cycle_value - delay_value : 0;
    //         return slots[read_cycle % Depth];
    //     }

    //     T &
    //     write(Cycles cycle)
    //     {
    //         return slots[static_cast<uint64_t>(cycle) % Depth];
    //     }
    // };

    template <class T, std::size_t Depth = 2>
class TimeBuffer
{
  private:
    std::array<T, Depth> slots{};
    std::array<uint64_t, Depth> writeCycles{};
    std::array<bool, Depth> valid{};
    std::size_t nextSlot = 0;
    T initialValue{};

    const T &
    latest(uint64_t targetCycle) const
    {
        const T *result = &initialValue;
        uint64_t latestCycle = 0;
        bool found = false;

        for (std::size_t i = 0; i < Depth; ++i) {
            if (valid[i] && writeCycles[i] <= targetCycle &&
                (!found || writeCycles[i] > latestCycle)) {
                result = &slots[i];
                latestCycle = writeCycles[i];
                found = true;
            }
        }

        return *result;
    }

  public:
    const T &
    read(Cycles cycle, Cycles delay = Cycles(1)) const
    {
        const uint64_t c = static_cast<uint64_t>(cycle);
        const uint64_t d = static_cast<uint64_t>(delay);

        if (c < d)
            return initialValue;

        return latest(c - d);
    }

    T &
    write(Cycles cycle)
    {
        const uint64_t c = static_cast<uint64_t>(cycle);

        // 同一周期多次调用 write()，返回同一个槽位。
        for (std::size_t i = 0; i < Depth; ++i) {
            if (valid[i] && writeCycles[i] == c)
                return slots[i];
        }

        // 新值默认继承之前的寄存器值。
        T previous = c == 0 ? initialValue : latest(c - 1);

        const std::size_t index = nextSlot;
        nextSlot = (nextSlot + 1) % Depth;

        slots[index] = previous;
        writeCycles[index] = c;
        valid[index] = true;
        return slots[index];
    }
};


    struct UcoreOut
    {
        //给Ibuffer的输出
        bool pc_vld = false;
        PC pc_data = 0;
    };

    struct IbufferOut
    {
        //给Ucore的输出
        bool ins_vld = false;
        Inst ins_data;
        PC pc_data = 0;
        //给L0buffer的输出
        bool L0_ovld[2];
        Addr L0_oaddr[2];

    };

    struct L0bufferOut
    {
        //给Ibuffer的输出
        bool L0_ivld[2];
        uint8_t* L0_idata[2];
        Addr L0_iaddr[2];
        
    };

}




#endif
