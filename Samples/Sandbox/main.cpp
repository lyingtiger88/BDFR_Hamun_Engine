#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>
#include <iostream>
int main() {
  Hamun::Core::Log(Hamun::Core::LogLevel::Info,"BDFR Hamun Engine sandbox booting");
#if defined(_WIN32)
  auto backend=Hamun::RHI::CreateBackend(Hamun::RHI::BackendType::D3D12);
#else
  auto backend=Hamun::RHI::CreateBackend(Hamun::RHI::BackendType::Vulkan);
#endif
  if(backend) backend->Initialize();

  Hamun::World::StreamingScheduler streaming;
  Hamun::World::WorldPosition observer{};
  Hamun::World::StreamingRequest req;
  req.id=1; req.resource="Example/CityCell_2_0"; req.position.cellX=2; req.velocityX=-60.0;
  streaming.Enqueue(req,observer);
  if(auto next=streaming.TryPop()) std::cout<<"Next stream request: "<<next->resource<<" score="<<next->score<<"\n";

  Hamun::Graph::Program p;
  p.constants={6.0,7.0};
  p.code={{Hamun::Graph::OpCode::PushConstant,0},{Hamun::Graph::OpCode::PushConstant,1},{Hamun::Graph::OpCode::Multiply,0},{Hamun::Graph::OpCode::Return,0}};
  Hamun::Graph::VM vm;
  auto v=vm.Execute(p);
  if(auto result=std::get_if<double>(&v)) std::cout<<"HamunGraph VM test: 6 * 7 = "<<*result<<"\n";
  if(backend) backend->Shutdown();
}
