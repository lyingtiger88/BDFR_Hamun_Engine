#pragma once

#include <string>

namespace Hamun {

class RenderPass {
public:
    explicit RenderPass(std::string name)
        : name_(std::move(name))
    {
    }

    virtual ~RenderPass() = default;

    virtual void Setup() {}
    virtual void Execute() {}

    [[nodiscard]] const std::string& GetName() const noexcept
    {
        return name_;
    }

private:
    std::string name_;
};

} // namespace Hamun
