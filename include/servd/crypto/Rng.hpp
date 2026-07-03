#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <stdexcept>
#include <sys/random.h>

namespace servd {

    class Rng {
    public:
        static void fill(void* buf, size_t len) {
            size_t done = 0;
            auto* bytes = static_cast<uint8_t*>(buf);
            while (done < len) {
                ssize_t r = getrandom(bytes + done, len - done, 0);
                if (r < 0) {
                    throw std::runtime_error("getrandom() failed");
                }
                done += static_cast<size_t>(r);
            }
        }

        template<size_t N>
        static std::array<uint8_t, N> gen() {
            std::array<uint8_t, N> buf;
            fill(buf.data(), buf.size());
            return buf;
        }
    };

}
