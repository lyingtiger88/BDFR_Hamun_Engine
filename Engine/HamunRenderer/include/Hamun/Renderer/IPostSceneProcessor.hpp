#pragma once

#include <Hamun/RHI/RHI.hpp>

namespace Hamun::Renderer {

class IPostSceneProcessor {
public:
    virtual ~IPostSceneProcessor() = default;

    virtual bool Execute(
        RHI::ICommandList& commands) = 0;
};

} // namespace Hamun::Renderer
