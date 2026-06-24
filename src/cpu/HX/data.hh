#ifndef __CPU_HX_data_HH__
#define __CPU_HX_data_HH__

#include <array>
#include <atomic>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
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

    
}




#endif