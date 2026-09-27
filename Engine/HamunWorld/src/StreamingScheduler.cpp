#include <Hamun/World/StreamingScheduler.hpp>
#include <cmath>
#include <utility>
namespace Hamun::World {
void StreamingScheduler::Enqueue(StreamingRequest request,const WorldPosition& observer,double predictionSeconds) {
  const double px=request.position.X()+request.velocityX*predictionSeconds;
  const double py=request.position.Y()+request.velocityY*predictionSeconds;
  const double pz=request.position.Z()+request.velocityZ*predictionSeconds;
  const double dx=px-observer.X(), dy=py-observer.Y(), dz=pz-observer.Z();
  request.score=std::sqrt(dx*dx+dy*dy+dz*dz);
  std::scoped_lock lock(mutex_);
  requests_.push(std::move(request));
}
std::optional<StreamingRequest> StreamingScheduler::TryPop() {
  std::scoped_lock lock(mutex_);
  if(requests_.empty()) return std::nullopt;
  auto r=requests_.top(); requests_.pop(); return r;
}
std::size_t StreamingScheduler::PendingCount() const {
  std::scoped_lock lock(mutex_); return requests_.size();
}
}
