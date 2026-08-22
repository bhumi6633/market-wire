#include "efe/replay.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace efe {
std::size_t ReplayEngine::run(const std::string& path, const ReplayOptions& o, const Callback& cb) {
    if(o.mode==ReplayMode::Scaled && o.speed<=0.0) throw std::invalid_argument("replay speed must be > 0");
    CaptureReader reader(path); std::optional<std::uint64_t> prev; std::size_t count=0;
    while(auto rec=reader.next()) {
        if(prev) {
            const auto delta=rec->monotonic_ns-*prev;
            if(o.mode==ReplayMode::Realtime && delta) std::this_thread::sleep_for(std::chrono::nanoseconds(delta));
            if(o.mode==ReplayMode::Scaled && delta) std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<std::uint64_t>(delta/o.speed)));
        }
        if(o.mode==ReplayMode::Step) { std::cout << "press ENTER for next packet..." << std::flush; std::cin.get(); }
        cb(*rec); prev=rec->monotonic_ns; ++count;
    }
    return count;
}
}  // namespace efe
