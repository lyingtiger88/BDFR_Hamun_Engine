#pragma once
#include <cstdint>
namespace Hamun::World {
struct LocalPosition { float x=0, y=0, z=0; };
struct WorldPosition {
  std::int64_t cellX=0, cellY=0, cellZ=0;
  LocalPosition local{};
  static constexpr double DefaultCellSizeMeters=256.0;
  double X(double s=DefaultCellSizeMeters) const noexcept { return double(cellX)*s+local.x; }
  double Y(double s=DefaultCellSizeMeters) const noexcept { return double(cellY)*s+local.y; }
  double Z(double s=DefaultCellSizeMeters) const noexcept { return double(cellZ)*s+local.z; }
};
}
