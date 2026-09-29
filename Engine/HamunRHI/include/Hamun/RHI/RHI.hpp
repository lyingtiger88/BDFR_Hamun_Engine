#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Hamun::RHI {

enum class BackendType : std::uint8_t {
    D3D12,
    D3D11,
    Vulkan,
    OpenGLES
};

enum class BufferUsage : std::uint8_t {
    Vertex,
    Index,
    Constant,
    Upload
};

enum class IndexType : std::uint8_t {
    UInt16,
    UInt32
};

enum class ShaderStage : std::uint8_t {
    Vertex,
    Pixel
};

enum class VertexFormat : std::uint8_t {
    Float2,
    Float3,
    Float4
};

enum class VertexSemantic : std::uint8_t {
    Position,
    Normal,
    TexCoord,
    Color
};

enum class TextureFormat : std::uint8_t {
    RGBA8_UNorm
};

enum class SamplerFilter : std::uint8_t {
    Nearest,
    Linear
};

enum class SamplerAddressMode : std::uint8_t {
    Clamp,
    Repeat
};

struct Capabilities {
    bool compute = false;
    bool asyncCompute = false;
    bool indirectDraw = false;
    bool bindless = false;
    bool meshShaders = false;
    bool rayTracing = false;
};

struct AdapterInfo {
    std::string name;
    std::uint64_t dedicatedVideoMemory = 0;
    std::uint64_t sharedSystemMemory = 0;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
};

struct BackendCreateInfo {
    void* nativeWindowHandle = nullptr;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool enableValidation = true;
};

struct BufferDesc {
    std::uint64_t size = 0;
    BufferUsage usage = BufferUsage::Vertex;
    const void* initialData = nullptr;
};

struct TextureDesc {
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    TextureFormat format = TextureFormat::RGBA8_UNorm;
    const void* initialData = nullptr;
    std::uint32_t rowPitch = 0;
};

struct SamplerDesc {
    SamplerFilter filter = SamplerFilter::Linear;
    SamplerAddressMode addressU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressW = SamplerAddressMode::Repeat;
};

struct ShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
    std::string entryPoint = "main";
};

struct VertexAttribute {
    VertexSemantic semantic = VertexSemantic::Position;
    std::uint32_t semanticIndex = 0;
    VertexFormat format = VertexFormat::Float3;
    std::uint32_t offset = 0;
};

class IBuffer;
class ITexture;
class ISampler;
class IShader;
class IPipeline;
class ICommandList;
class ISwapChain;
class IFence;

struct GraphicsPipelineDesc {
    IShader* vertexShader = nullptr;
    IShader* pixelShader = nullptr;
    std::vector<VertexAttribute> vertexAttributes;
    std::uint32_t vertexStride = 0;
    std::uint32_t constantBufferCount = 0;
    std::uint32_t textureCount = 0;
    std::uint32_t samplerCount = 0;
    bool depthTest = true;
};

class IBuffer {
public:
    virtual ~IBuffer() = default;

    [[nodiscard]] virtual std::uint64_t Size() const noexcept = 0;
    [[nodiscard]] virtual BufferUsage Usage() const noexcept = 0;

    virtual bool Update(
        const void* data,
        std::uint64_t size,
        std::uint64_t offset = 0) = 0;
};

class ITexture {
public:
    virtual ~ITexture() = default;

    [[nodiscard]] virtual std::uint32_t Width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t Height() const noexcept = 0;
    [[nodiscard]] virtual TextureFormat Format() const noexcept = 0;
};

class ISampler {
public:
    virtual ~ISampler() = default;
};

class IShader {
public:
    virtual ~IShader() = default;
    [[nodiscard]] virtual ShaderStage Stage() const noexcept = 0;
};

class IPipeline {
public:
    virtual ~IPipeline() = default;
};

class ISwapChain {
public:
    virtual ~ISwapChain() = default;
    [[nodiscard]] virtual std::uint32_t Width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t Height() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t FrameIndex() const noexcept = 0;
};

class IFence {
public:
    virtual ~IFence() = default;
    [[nodiscard]] virtual std::uint64_t CompletedValue() const noexcept = 0;
};

class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void BeginRenderPass(
        const std::array<float, 4>& clearColor) = 0;
    virtual void SetPipeline(IPipeline& pipeline) = 0;
    virtual void SetVertexBuffer(
        IBuffer& buffer,
        std::uint32_t stride) = 0;
    virtual void SetIndexBuffer(
        IBuffer& buffer,
        IndexType indexType) = 0;
    virtual void SetConstantBuffer(
        std::uint32_t slot,
        IBuffer& buffer) = 0;
    virtual void SetTexture(
        std::uint32_t slot,
        ITexture& texture) = 0;
    virtual void SetSampler(
        std::uint32_t slot,
        ISampler& sampler) = 0;
    virtual void Draw(
        std::uint32_t vertexCount,
        std::uint32_t firstVertex = 0) = 0;
    virtual void DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t firstIndex = 0,
        std::int32_t vertexOffset = 0) = 0;
    virtual void EndRenderPass() = 0;
};

class IBackend {
public:
    virtual ~IBackend() = default;

    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    [[nodiscard]] virtual BackendType Type() const noexcept = 0;
    [[nodiscard]] virtual const Capabilities& Caps() const noexcept = 0;

    [[nodiscard]] virtual const AdapterInfo& Adapter() const noexcept
    {
        static const AdapterInfo unknown{};
        return unknown;
    }

    virtual bool Initialize(const BackendCreateInfo& createInfo) = 0;
    virtual void Shutdown() = 0;

    virtual std::unique_ptr<IBuffer> CreateBuffer(
        const BufferDesc& desc) = 0;
    virtual std::unique_ptr<ITexture> CreateTexture(
        const TextureDesc& desc) = 0;
    virtual std::unique_ptr<ISampler> CreateSampler(
        const SamplerDesc& desc) = 0;
    virtual std::unique_ptr<IShader> CreateShader(
        const ShaderDesc& desc) = 0;
    virtual std::unique_ptr<IPipeline> CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc) = 0;

    virtual ICommandList* BeginFrame() = 0;
    virtual bool SubmitFrame() = 0;

    [[nodiscard]] virtual ISwapChain* SwapChain() noexcept = 0;
    [[nodiscard]] virtual IFence* FrameFence() noexcept = 0;
};

std::unique_ptr<IBackend> CreateBackend(BackendType type);

std::string BuildCapabilityReport(
    const IBackend& backend);

} // namespace Hamun::RHI
