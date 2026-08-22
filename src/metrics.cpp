#include "efe/metrics.hpp"

#include <algorithm>
#include <numeric>

namespace efe {
LatencySummary LatencySamples::summarize() const {
    if(samples_.empty()) return {};
    auto s=samples_; std::sort(s.begin(),s.end());
    auto q=[&](double p){ const auto idx=static_cast<std::size_t>(p*static_cast<double>(s.size()-1)); return s[idx]; };
    long double sum=0; for(auto v:s) sum+=static_cast<long double>(v);
    return LatencySummary{static_cast<double>(sum/static_cast<long double>(s.size())),q(.50),q(.95),q(.99),q(.999),s.back()};
}
}  // namespace efe
