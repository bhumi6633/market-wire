#pragma once

#include "efe/capture.hpp"

#include <functional>
#include <string>

namespace efe {

enum class ReplayMode { MaxSpeed, Realtime, Scaled, Step };
struct ReplayOptions { ReplayMode mode{ReplayMode::MaxSpeed}; double speed{1.0}; };

class ReplayEngine {
public:
    using Callback = std::function<void(const CaptureRecord&)>;
    static std::size_t run(const std::string& path, const ReplayOptions& options, const Callback& callback);
};

}  // namespace efe
