#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <Hamun/World/WorldPosition.hpp>
namespace Hamun::World {
struct StreamingRequest {
  std::uint64_t id=0;
  std::string resource;
  WorldPosition position{};
  double velocityX=0, velocityY=0, velocityZ=0;
  double score=0;
};
class StreamingScheduler {
public:
  void Enqueue(StreamingRequest request,const WorldPosition& observer,double predictionSeconds=2.0);
  std::optional<StreamingRequest> TryPop();
  std::size_t PendingCount() const;
private:
  struct Compare {
    bool operator()(const StreamingRequest&a,const StreamingRequest&b) const noexcept { return a.score>b.score; }
  };
  mutable std::mutex mutex_;
  std::priority_queue<StreamingRequest,std::vector<StreamingRequest>,Compare> requests_;
};
}
