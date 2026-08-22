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
            if (rec->monotonic_ns < *prev) throw std::runtime_error("capture timestamps are not monotonic");
            const auto delta=rec->monotonic_ns-*prev;
            if(o.mode==ReplayMode::Realtime && delta) std::this_thread::sleep_for(std::chrono::nanoseconds(delta));
            if(o.mode==ReplayMode::Scaled && delta) {
                const auto scaled = static_cast<long double>(delta) / static_cast<long double>(o.speed);
                if (scaled > static_cast<long double>(std::chrono::nanoseconds::max().count())) throw std::overflow_error("scaled replay delay is too large");
                std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<std::uint64_t>(scaled)));
            }
        }
        if(o.mode==ReplayMode::Step) {
            std::istream& input = o.step_input ? *o.step_input : std::cin;
            std::ostream& output = o.step_output ? *o.step_output : std::cout;
            output << "press ENTER for next packet..." << std::flush;
            if (!input.get()) throw std::runtime_error("step replay input ended");
        }
        cb(*rec); prev=rec->monotonic_ns; ++count;
    }
    return count;
}
}  // namespace efe
