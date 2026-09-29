#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

bool Failed(HRESULT hr, const char* operation)
{
    if (SUCCEEDED(hr))
        return false;

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Error,
        std::string("D3D11: ") + operation + " failed.");
    return true;
}

std::string FromWide(const wchar_t* text)
{
    if (!text || !*text)
        return {};

    const int length =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            text,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (length <= 1)
        return {};

    std::string result(
        static_cast<std::size_t>(length),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        text,
        -1,
        result.data(),
        length,
        nullptr,
        nullptr);

    if (!result.empty() &&
        result.back() == '\0') {
        result.pop_back();
    }

    return result;
}

DXGI_FORMAT ToDxgiFormat(Hamun::RHI::VertexFormat format)
{
    using Hamun::RHI::VertexFormat;
    switch (format) {
        case VertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT ToDxgiFormat(Hamun::RHI::TextureFormat format)
{
    using Hamun::RHI::TextureFormat;
    switch (format) {
        case TextureFormat::RGBA8_UNorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

const char* SemanticName(Hamun::RHI::VertexSemantic semantic)
{
    using Hamun::RHI::VertexSemantic;
    switch (semantic) {
        case VertexSemantic::Position: return "POSITION";
        case VertexSemantic::Normal: return "NORMAL";
        case VertexSemantic::TexCoord: return "TEXCOORD";
        case VertexSemantic::Color: return "COLOR";
    }
    return "TEXCOORD";
}

D3D11_FILTER ToNativeFilter(Hamun::RHI::SamplerFilter filter)
{
    return filter == Hamun::RHI::SamplerFilter::Nearest
        ? D3D11_FILTER_MIN_MAG_MIP_POINT
        : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D11_TEXTURE_ADDRESS_MODE ToNativeAddress(
    Hamun::RHI::SamplerAddressMode mode)
{
    return mode == Hamun::RHI::SamplerAddressMode::Clamp
        ? D3D11_TEXTURE_ADDRESS_CLAMP
        : D3D11_TEXTURE_ADDRESS_WRAP;
}

UINT AlignConstantBufferSize(UINT size)
{
    return (size + 15u) & ~15u;
}

const char* FeatureLevelName(D3D_FEATURE_LEVEL level)
{
    switch (level) {
        case D3D_FEATURE_LEVEL_11_1: return "11_1";
        case D3D_FEATURE_LEVEL_11_0: return "11_0";
        case D3D_FEATURE_LEVEL_10_1: return "10_1";
        case D3D_FEATURE_LEVEL_10_0: return "10_0";
        default: return "unknown";
    }
}

} // namespace

#endif

namespace Hamun::RHI {

#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)

class D3D11Buffer final : public IBuffer {
public:
    D3D11Buffer(
        ComPtr<ID3D11Buffer> buffer,
        ID3D11DeviceContext* context,
        std::uint64_t size,
        BufferUsage usage,
        bool dynamic)
        : buffer_(std::move(buffer))
        , context_(context)
        , size_(size)
        , usage_(usage)
        , dynamic_(dynamic)
    {
    }

    std::uint64_t Size() const noexcept override { return size_; }
    BufferUsage Usage() const noexcept override { return usage_; }

    bool Update(
        const void* data,
        std::uint64_t size,
        std::uint64_t offset) override
    {
        if (!data ||
            !context_ ||
            size == 0 ||
            offset + size > size_)
            return false;

        if (dynamic_) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context_->Map(
                    buffer_.Get(),
                    0,
                    D3D11_MAP_WRITE_DISCARD,
                    0,
                    &mapped)))
                return false;

            std::memcpy(
                static_cast<std::byte*>(mapped.pData) + offset,
                data,
                static_cast<std::size_t>(size));

            context_->Unmap(buffer_.Get(), 0);
            return true;
        }

        D3D11_BOX box{};
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + size);
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;

        context_->UpdateSubresource(
            buffer_.Get(),
            0,
            &box,
            data,
            0,
            0);
        return true;
    }

    ID3D11Buffer* Native() const noexcept
    {
        return buffer_.Get();
    }

private:
    ComPtr<ID3D11Buffer> buffer_;
    ID3D11DeviceContext* context_ = nullptr;
    std::uint64_t size_ = 0;
    BufferUsage usage_ = BufferUsage::Vertex;
    bool dynamic_ = false;
};

class D3D11Texture final : public ITexture {
public:
    D3D11Texture(
        ComPtr<ID3D11Texture2D> texture,
        ComPtr<ID3D11ShaderResourceView> srv,
        std::uint32_t width,
        std::uint32_t height,
        TextureFormat format)
        : texture_(std::move(texture))
        , srv_(std::move(srv))
        , width_(width)
        , height_(height)
        , format_(format)
    {
    }

    std::uint32_t Width() const noexcept override { return width_; }
    std::uint32_t Height() const noexcept override { return height_; }
    TextureFormat Format() const noexcept override { return format_; }

    ID3D11ShaderResourceView* Srv() const noexcept
    {
        return srv_.Get();
    }

private:
    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    TextureFormat format_ = TextureFormat::RGBA8_UNorm;
};

class D3D11Sampler final : public ISampler {
public:
    explicit D3D11Sampler(ComPtr<ID3D11SamplerState> sampler)
        : sampler_(std::move(sampler))
    {
    }

    ID3D11SamplerState* Native() const noexcept
    {
        return sampler_.Get();
    }

private:
    ComPtr<ID3D11SamplerState> sampler_;
};

class D3D11Shader final : public IShader {
public:
    D3D11Shader(
        ShaderStage stage,
        ComPtr<ID3DBlob> bytecode,
        ComPtr<ID3D11VertexShader> vertexShader,
        ComPtr<ID3D11PixelShader> pixelShader)
        : stage_(stage)
        , bytecode_(std::move(bytecode))
        , vertexShader_(std::move(vertexShader))
        , pixelShader_(std::move(pixelShader))
    {
    }

    ShaderStage Stage() const noexcept override { return stage_; }

    ID3DBlob* Bytecode() const noexcept { return bytecode_.Get(); }
    ID3D11VertexShader* VertexShader() const noexcept
    {
        return vertexShader_.Get();
    }
    ID3D11PixelShader* PixelShader() const noexcept
    {
        return pixelShader_.Get();
    }

private:
    ShaderStage stage_;
    ComPtr<ID3DBlob> bytecode_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
};

class D3D11Pipeline final : public IPipeline {
public:
    D3D11Pipeline(
        ComPtr<ID3D11InputLayout> inputLayout,
        ComPtr<ID3D11VertexShader> vertexShader,
        ComPtr<ID3D11PixelShader> pixelShader,
        ComPtr<ID3D11DepthStencilState> depthState,
        ComPtr<ID3D11RasterizerState> rasterizerState)
        : inputLayout_(std::move(inputLayout))
        , vertexShader_(std::move(vertexShader))
        , pixelShader_(std::move(pixelShader))
        , depthState_(std::move(depthState))
        , rasterizerState_(std::move(rasterizerState))
    {
    }

    ID3D11InputLayout* InputLayout() const noexcept
    {
        return inputLayout_.Get();
    }

    ID3D11VertexShader* VertexShader() const noexcept
    {
        return vertexShader_.Get();
    }

    ID3D11PixelShader* PixelShader() const noexcept
    {
        return pixelShader_.Get();
    }

    ID3D11DepthStencilState* DepthState() const noexcept
    {
        return depthState_.Get();
    }

    ID3D11RasterizerState* RasterizerState() const noexcept
    {
        return rasterizerState_.Get();
    }

private:
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11DepthStencilState> depthState_;
    ComPtr<ID3D11RasterizerState> rasterizerState_;
};

class D3D11SwapChainView final : public ISwapChain {
public:
    void Bind(
        std::uint32_t width,
        std::uint32_t height,
        const std::uint64_t* frameSerial)
    {
        width_ = width;
        height_ = height;
        frameSerial_ = frameSerial;
    }

    std::uint32_t Width() const noexcept override { return width_; }
    std::uint32_t Height() const noexcept override { return height_; }

    std::uint32_t FrameIndex() const noexcept override
    {
        return frameSerial_
            ? static_cast<std::uint32_t>(*frameSerial_ & 1ull)
            : 0u;
    }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    const std::uint64_t* frameSerial_ = nullptr;
};

class D3D11FenceView final : public IFence {
public:
    void Bind(const std::uint64_t* completedSerial)
    {
        completedSerial_ = completedSerial;
    }

    std::uint64_t CompletedValue() const noexcept override
    {
        return completedSerial_
            ? *completedSerial_
            : 0ull;
    }

private:
    const std::uint64_t* completedSerial_ = nullptr;
};

class D3D11CommandList final : public ICommandList {
public:
    void Prepare(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* rtv,
        ID3D11DepthStencilView* dsv,
        const D3D11_VIEWPORT& viewport,
        const D3D11_RECT& scissor)
    {
        context_ = context;
        rtv_ = rtv;
        dsv_ = dsv;
        viewport_ = viewport;
        scissor_ = scissor;
    }

    void BeginRenderPass(
        const std::array<float, 4>& clearColor) override
    {
        if (!context_ || !rtv_ || !dsv_)
            return;

        ID3D11RenderTargetView* targets[] = {rtv_};
        context_->OMSetRenderTargets(
            1,
            targets,
            dsv_);

        context_->RSSetViewports(
            1,
            &viewport_);

        context_->RSSetScissorRects(
            1,
            &scissor_);

        context_->ClearRenderTargetView(
            rtv_,
            clearColor.data());

        context_->ClearDepthStencilView(
            dsv_,
            D3D11_CLEAR_DEPTH,
            1.0f,
            0);
    }

    void SetPipeline(IPipeline& pipeline) override
    {
        auto* native =
            dynamic_cast<D3D11Pipeline*>(&pipeline);

        if (!native || !context_)
            return;

        context_->IASetInputLayout(
            native->InputLayout());

        context_->VSSetShader(
            native->VertexShader(),
            nullptr,
            0);

        context_->PSSetShader(
            native->PixelShader(),
            nullptr,
            0);

        context_->OMSetDepthStencilState(
            native->DepthState(),
            0);

        context_->RSSetState(
            native->RasterizerState());
    }

    void SetVertexBuffer(
        IBuffer& buffer,
        std::uint32_t stride) override
    {
        auto* native =
            dynamic_cast<D3D11Buffer*>(&buffer);

        if (!native ||
            !context_ ||
            stride == 0)
            return;

        ID3D11Buffer* buffers[] = {
            native->Native()
        };

        const UINT nativeStride =
            static_cast<UINT>(stride);
        const UINT offset = 0;

        context_->IASetVertexBuffers(
            0,
            1,
            buffers,
            &nativeStride,
            &offset);

        context_->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    void SetIndexBuffer(
        IBuffer& buffer,
        IndexType indexType) override
    {
        auto* native =
            dynamic_cast<D3D11Buffer*>(&buffer);

        if (!native || !context_)
            return;

        const DXGI_FORMAT format =
            indexType == IndexType::UInt16
                ? DXGI_FORMAT_R16_UINT
                : DXGI_FORMAT_R32_UINT;

        context_->IASetIndexBuffer(
            native->Native(),
            format,
            0);
    }

    void SetConstantBuffer(
        std::uint32_t slot,
        IBuffer& buffer) override
    {
        auto* native =
            dynamic_cast<D3D11Buffer*>(&buffer);

        if (!native || !context_)
            return;

        ID3D11Buffer* buffers[] = {
            native->Native()
        };

        context_->VSSetConstantBuffers(
            slot,
            1,
            buffers);

        context_->PSSetConstantBuffers(
            slot,
            1,
            buffers);
    }

    void SetTexture(
        std::uint32_t slot,
        ITexture& texture) override
    {
        auto* native =
            dynamic_cast<D3D11Texture*>(&texture);

        if (!native || !context_)
            return;

        ID3D11ShaderResourceView* views[] = {
            native->Srv()
        };

        context_->PSSetShaderResources(
            slot,
            1,
            views);
    }

    void SetSampler(
        std::uint32_t slot,
        ISampler& sampler) override
    {
        auto* native =
            dynamic_cast<D3D11Sampler*>(&sampler);

        if (!native || !context_)
            return;

        ID3D11SamplerState* samplers[] = {
            native->Native()
        };

        context_->PSSetSamplers(
            slot,
            1,
            samplers);
    }

    void Draw(
        std::uint32_t vertexCount,
        std::uint32_t firstVertex) override
    {
        if (context_) {
            context_->Draw(
                vertexCount,
                firstVertex);
        }
    }

    void DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t firstIndex,
        std::int32_t vertexOffset) override
    {
        if (context_) {
            context_->DrawIndexed(
                indexCount,
                firstIndex,
                vertexOffset);
        }
    }

    void EndRenderPass() override
    {
    }

private:
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    ID3D11DepthStencilView* dsv_ = nullptr;
    D3D11_VIEWPORT viewport_{};
    D3D11_RECT scissor_{};
};

#endif

class D3D11Backend final : public IBackend {
public:
    D3D11Backend() = default;
    ~D3D11Backend() override { Shutdown(); }

    std::string_view Name() const noexcept override
    {
        return "Direct3D 11";
    }

    BackendType Type() const noexcept override
    {
        return BackendType::D3D11;
    }

    const Capabilities& Caps() const noexcept override
    {
        return caps_;
    }

    const AdapterInfo& Adapter() const noexcept override
    {
        return adapterInfo_;
    }

    bool Initialize(
        const BackendCreateInfo& createInfo) override;

    void Shutdown() override;

    std::unique_ptr<IBuffer> CreateBuffer(
        const BufferDesc& desc) override;

    std::unique_ptr<ITexture> CreateTexture(
        const TextureDesc& desc) override;

    std::unique_ptr<ISampler> CreateSampler(
        const SamplerDesc& desc) override;

    std::unique_ptr<IShader> CreateShader(
        const ShaderDesc& desc) override;

    std::unique_ptr<IPipeline> CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc) override;

    ICommandList* BeginFrame() override;
    bool SubmitFrame() override;

    ISwapChain* SwapChain() noexcept override;
    IFence* FrameFence() noexcept override;

private:
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    bool CreateDeviceAndSwapChain(
        HWND hwnd,
        std::uint32_t width,
        std::uint32_t height,
        bool enableValidation);

    bool CreateFrameTargets(
        std::uint32_t width,
        std::uint32_t height);

    const char* ShaderTarget(
        ShaderStage stage) const noexcept;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11Texture2D> depthTexture_;
    ComPtr<ID3D11DepthStencilView> dsv_;

    D3D_FEATURE_LEVEL featureLevel_ =
        D3D_FEATURE_LEVEL_10_0;

    D3D11_VIEWPORT viewport_{};
    D3D11_RECT scissor_{};

    D3D11CommandList commandList_;
    D3D11SwapChainView swapChainView_;
    D3D11FenceView fenceView_;

    std::uint64_t frameSerial_ = 0;
    std::uint64_t completedSerial_ = 0;
    bool initialized_ = false;
#endif

    AdapterInfo adapterInfo_{};
    Capabilities caps_{};
};

bool D3D11Backend::Initialize(
    const BackendCreateInfo& createInfo)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!createInfo.nativeWindowHandle) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D11 requires a valid Win32 window handle.");
        return false;
    }

    if (!CreateDeviceAndSwapChain(
            static_cast<HWND>(
                createInfo.nativeWindowHandle),
            createInfo.width,
            createInfo.height,
            createInfo.enableValidation))
        return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(
            device_.As(
                &dxgiDevice))) {
        ComPtr<IDXGIAdapter> adapter;

        if (SUCCEEDED(
                dxgiDevice->GetAdapter(
                    &adapter))) {
            DXGI_ADAPTER_DESC adapterDesc{};

            if (SUCCEEDED(
                    adapter->GetDesc(
                        &adapterDesc))) {
                adapterInfo_.name =
                    FromWide(
                        adapterDesc.Description);

                adapterInfo_.dedicatedVideoMemory =
                    static_cast<std::uint64_t>(
                        adapterDesc.DedicatedVideoMemory);

                adapterInfo_.sharedSystemMemory =
                    static_cast<std::uint64_t>(
                        adapterDesc.SharedSystemMemory);

                adapterInfo_.vendorId =
                    adapterDesc.VendorId;

                adapterInfo_.deviceId =
                    adapterDesc.DeviceId;
            }
        }
    }

    if (!CreateFrameTargets(
            createInfo.width,
            createInfo.height))
        return false;

    caps_.compute =
        featureLevel_ >= D3D_FEATURE_LEVEL_11_0;
    caps_.asyncCompute = false;
    caps_.indirectDraw =
        featureLevel_ >= D3D_FEATURE_LEVEL_11_0;
    caps_.bindless = false;
    caps_.meshShaders = false;
    caps_.rayTracing = false;

    swapChainView_.Bind(
        createInfo.width,
        createInfo.height,
        &frameSerial_);

    fenceView_.Bind(&completedSerial_);

    initialized_ = true;

    Core::Log(
        Core::LogLevel::Info,
        std::string("D3D11 compatibility backend initialized at feature level ") +
            FeatureLevelName(featureLevel_) + ".");

    return true;
#else
    (void)createInfo;
    Core::Log(
        Core::LogLevel::Warning,
        "D3D11 backend is unavailable in this build.");
    return false;
#endif
}

void D3D11Backend::Shutdown()
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!device_)
        return;

    if (context_) {
        context_->ClearState();
        context_->Flush();
    }

    dsv_.Reset();
    depthTexture_.Reset();
    rtv_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();

    initialized_ = false;
#endif
}

std::unique_ptr<IBuffer> D3D11Backend::CreateBuffer(
    const BufferDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_ ||
        desc.size == 0 ||
        desc.size > UINT_MAX)
        return {};

    const bool dynamic =
        desc.usage == BufferUsage::Constant ||
        desc.usage == BufferUsage::Upload;

    UINT byteWidth =
        static_cast<UINT>(desc.size);

    if (desc.usage == BufferUsage::Constant)
        byteWidth = AlignConstantBufferSize(byteWidth);

    D3D11_BUFFER_DESC nativeDesc{};
    nativeDesc.ByteWidth = byteWidth;
    nativeDesc.Usage =
        dynamic
            ? D3D11_USAGE_DYNAMIC
            : D3D11_USAGE_DEFAULT;
    nativeDesc.CPUAccessFlags =
        dynamic
            ? D3D11_CPU_ACCESS_WRITE
            : 0;

    switch (desc.usage) {
        case BufferUsage::Vertex:
            nativeDesc.BindFlags =
                D3D11_BIND_VERTEX_BUFFER;
            break;

        case BufferUsage::Index:
            nativeDesc.BindFlags =
                D3D11_BIND_INDEX_BUFFER;
            break;

        case BufferUsage::Constant:
            nativeDesc.BindFlags =
                D3D11_BIND_CONSTANT_BUFFER;
            break;

        case BufferUsage::Upload:
            nativeDesc.BindFlags = 0;
            break;
    }

    D3D11_SUBRESOURCE_DATA initial{};
    const D3D11_SUBRESOURCE_DATA* initialPtr = nullptr;

    if (!dynamic && desc.initialData) {
        initial.pSysMem = desc.initialData;
        initialPtr = &initial;
    }

    ComPtr<ID3D11Buffer> buffer;

    if (Failed(
            device_->CreateBuffer(
                &nativeDesc,
                initialPtr,
                &buffer),
            "CreateBuffer"))
        return {};

    auto result =
        std::make_unique<D3D11Buffer>(
            std::move(buffer),
            context_.Get(),
            byteWidth,
            desc.usage,
            dynamic);

    if (dynamic &&
        desc.initialData &&
        !result->Update(
            desc.initialData,
            desc.size,
            0))
        return {};

    return result;
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<ITexture> D3D11Backend::CreateTexture(
    const TextureDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_ ||
        desc.width == 0 ||
        desc.height == 0)
        return {};

    D3D11_TEXTURE2D_DESC nativeDesc{};
    nativeDesc.Width = desc.width;
    nativeDesc.Height = desc.height;
    nativeDesc.MipLevels = 1;
    nativeDesc.ArraySize = 1;
    nativeDesc.Format =
        ToDxgiFormat(desc.format);
    nativeDesc.SampleDesc.Count = 1;
    nativeDesc.Usage =
        D3D11_USAGE_DEFAULT;
    nativeDesc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial{};
    const D3D11_SUBRESOURCE_DATA* initialPtr = nullptr;

    if (desc.initialData) {
        initial.pSysMem = desc.initialData;
        initial.SysMemPitch =
            desc.rowPitch != 0
                ? desc.rowPitch
                : desc.width * 4u;
        initialPtr = &initial;
    }

    ComPtr<ID3D11Texture2D> texture;

    if (Failed(
            device_->CreateTexture2D(
                &nativeDesc,
                initialPtr,
                &texture),
            "CreateTexture2D"))
        return {};

    ComPtr<ID3D11ShaderResourceView> srv;

    if (Failed(
            device_->CreateShaderResourceView(
                texture.Get(),
                nullptr,
                &srv),
            "CreateShaderResourceView"))
        return {};

    return std::make_unique<D3D11Texture>(
        std::move(texture),
        std::move(srv),
        desc.width,
        desc.height,
        desc.format);
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<ISampler> D3D11Backend::CreateSampler(
    const SamplerDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_)
        return {};

    D3D11_SAMPLER_DESC nativeDesc{};
    nativeDesc.Filter =
        ToNativeFilter(desc.filter);
    nativeDesc.AddressU =
        ToNativeAddress(desc.addressU);
    nativeDesc.AddressV =
        ToNativeAddress(desc.addressV);
    nativeDesc.AddressW =
        ToNativeAddress(desc.addressW);
    nativeDesc.ComparisonFunc =
        D3D11_COMPARISON_ALWAYS;
    nativeDesc.MinLOD = 0.0f;
    nativeDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> sampler;

    if (Failed(
            device_->CreateSamplerState(
                &nativeDesc,
                &sampler),
            "CreateSamplerState"))
        return {};

    return std::make_unique<D3D11Sampler>(
        std::move(sampler));
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<IShader> D3D11Backend::CreateShader(
    const ShaderDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_ ||
        desc.source.empty() ||
        desc.entryPoint.empty())
        return {};

    UINT flags =
        D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    flags |=
        D3DCOMPILE_DEBUG |
        D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;

    const HRESULT compileResult =
        D3DCompile(
            desc.source.data(),
            desc.source.size(),
            "HamunD3D11Shader",
            nullptr,
            nullptr,
            desc.entryPoint.c_str(),
            ShaderTarget(desc.stage),
            flags,
            0,
            &bytecode,
            &errors);

    if (FAILED(compileResult)) {
        if (errors) {
            Core::Log(
                Core::LogLevel::Error,
                std::string_view(
                    static_cast<const char*>(
                        errors->GetBufferPointer()),
                    errors->GetBufferSize()));
        }
        return {};
    }

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;

    if (desc.stage == ShaderStage::Vertex) {
        if (Failed(
                device_->CreateVertexShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &vertexShader),
                "CreateVertexShader"))
            return {};
    } else {
        if (Failed(
                device_->CreatePixelShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &pixelShader),
                "CreatePixelShader"))
            return {};
    }

    return std::make_unique<D3D11Shader>(
        desc.stage,
        std::move(bytecode),
        std::move(vertexShader),
        std::move(pixelShader));
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<IPipeline>
D3D11Backend::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    auto* vs =
        dynamic_cast<D3D11Shader*>(
            desc.vertexShader);
    auto* ps =
        dynamic_cast<D3D11Shader*>(
            desc.pixelShader);

    if (!initialized_ ||
        !vs ||
        !ps ||
        vs->Stage() != ShaderStage::Vertex ||
        ps->Stage() != ShaderStage::Pixel)
        return {};

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    elements.reserve(
        desc.vertexAttributes.size());

    for (const VertexAttribute& attribute :
         desc.vertexAttributes) {
        D3D11_INPUT_ELEMENT_DESC element{};
        element.SemanticName =
            SemanticName(attribute.semantic);
        element.SemanticIndex =
            attribute.semanticIndex;
        element.Format =
            ToDxgiFormat(attribute.format);
        element.InputSlot = 0;
        element.AlignedByteOffset =
            attribute.offset;
        element.InputSlotClass =
            D3D11_INPUT_PER_VERTEX_DATA;
        element.InstanceDataStepRate = 0;

        elements.push_back(element);
    }

    ComPtr<ID3D11InputLayout> inputLayout;

    if (!elements.empty()) {
        if (Failed(
                device_->CreateInputLayout(
                    elements.data(),
                    static_cast<UINT>(
                        elements.size()),
                    vs->Bytecode()->GetBufferPointer(),
                    vs->Bytecode()->GetBufferSize(),
                    &inputLayout),
                "CreateInputLayout"))
            return {};
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable =
        desc.depthTest ? TRUE : FALSE;
    depthDesc.DepthWriteMask =
        D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc =
        D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable = FALSE;

    ComPtr<ID3D11DepthStencilState> depthState;

    if (Failed(
            device_->CreateDepthStencilState(
                &depthDesc,
                &depthState),
            "CreateDepthStencilState"))
        return {};

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode =
        D3D11_FILL_SOLID;
    rasterDesc.CullMode =
        D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise =
        FALSE;
    rasterDesc.DepthClipEnable =
        TRUE;
    rasterDesc.ScissorEnable =
        TRUE;

    ComPtr<ID3D11RasterizerState> rasterizerState;

    if (Failed(
            device_->CreateRasterizerState(
                &rasterDesc,
                &rasterizerState),
            "CreateRasterizerState"))
        return {};

    ComPtr<ID3D11VertexShader> vertexShader =
        vs->VertexShader();

    ComPtr<ID3D11PixelShader> pixelShader =
        ps->PixelShader();

    return std::make_unique<D3D11Pipeline>(
        std::move(inputLayout),
        std::move(vertexShader),
        std::move(pixelShader),
        std::move(depthState),
        std::move(rasterizerState));
#else
    (void)desc;
    return {};
#endif
}

ICommandList* D3D11Backend::BeginFrame()
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_)
        return nullptr;

    commandList_.Prepare(
        context_.Get(),
        rtv_.Get(),
        dsv_.Get(),
        viewport_,
        scissor_);

    return &commandList_;
#else
    return nullptr;
#endif
}

bool D3D11Backend::SubmitFrame()
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    if (!initialized_)
        return false;

    if (Failed(
            swapChain_->Present(1, 0),
            "SwapChain::Present"))
        return false;

    ++frameSerial_;
    completedSerial_ = frameSerial_;
    return true;
#else
    return false;
#endif
}

ISwapChain* D3D11Backend::SwapChain() noexcept
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    return initialized_
        ? &swapChainView_
        : nullptr;
#else
    return nullptr;
#endif
}

IFence* D3D11Backend::FrameFence() noexcept
{
#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)
    return initialized_
        ? &fenceView_
        : nullptr;
#else
    return nullptr;
#endif
}

#if defined(HAMUN_ENABLE_D3D11) && defined(_WIN32)

bool D3D11Backend::CreateDeviceAndSwapChain(
    HWND hwnd,
    std::uint32_t width,
    std::uint32_t height,
    bool enableValidation)
{
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferDesc.Width = width;
    swapDesc.BufferDesc.Height = height;
    swapDesc.BufferDesc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.OutputWindow = hwnd;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect =
        DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    const D3D_FEATURE_LEVEL fallbackLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    UINT flags =
        D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    if (enableValidation)
        flags |= D3D11_CREATE_DEVICE_DEBUG;

    auto createDevice = [&](D3D_DRIVER_TYPE driverType,
                            UINT createFlags,
                            const D3D_FEATURE_LEVEL* levels,
                            UINT levelCount) {
        return D3D11CreateDeviceAndSwapChain(
            nullptr,
            driverType,
            nullptr,
            createFlags,
            levels,
            levelCount,
            D3D11_SDK_VERSION,
            &swapDesc,
            &swapChain_,
            &device_,
            &featureLevel_,
            &context_);
    };

    HRESULT hr = createDevice(
        D3D_DRIVER_TYPE_HARDWARE,
        flags,
        requestedLevels,
        static_cast<UINT>(
            std::size(requestedLevels)));

    if (hr == E_INVALIDARG) {
        hr = createDevice(
            D3D_DRIVER_TYPE_HARDWARE,
            flags,
            fallbackLevels,
            static_cast<UINT>(
                std::size(fallbackLevels)));
    }

    if (FAILED(hr) &&
        enableValidation) {
        flags &=
            ~D3D11_CREATE_DEVICE_DEBUG;

        hr = createDevice(
            D3D_DRIVER_TYPE_HARDWARE,
            flags,
            requestedLevels,
            static_cast<UINT>(
                std::size(requestedLevels)));

        if (hr == E_INVALIDARG) {
            hr = createDevice(
                D3D_DRIVER_TYPE_HARDWARE,
                flags,
                fallbackLevels,
                static_cast<UINT>(
                    std::size(fallbackLevels)));
        }
    }

    if (FAILED(hr)) {
        Core::Log(
            Core::LogLevel::Warning,
            "D3D11 hardware device unavailable; trying WARP.");

        hr = createDevice(
            D3D_DRIVER_TYPE_WARP,
            0,
            fallbackLevels,
            static_cast<UINT>(
                std::size(fallbackLevels)));
    }

    return !Failed(
        hr,
        "D3D11CreateDeviceAndSwapChain");
}

bool D3D11Backend::CreateFrameTargets(
    std::uint32_t width,
    std::uint32_t height)
{
    ComPtr<ID3D11Texture2D> backBuffer;

    if (Failed(
            swapChain_->GetBuffer(
                0,
                IID_PPV_ARGS(&backBuffer)),
            "Get swap-chain back buffer"))
        return false;

    if (Failed(
            device_->CreateRenderTargetView(
                backBuffer.Get(),
                nullptr,
                &rtv_),
            "CreateRenderTargetView"))
        return false;

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format =
        DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage =
        D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags =
        D3D11_BIND_DEPTH_STENCIL;

    if (Failed(
            device_->CreateTexture2D(
                &depthDesc,
                nullptr,
                &depthTexture_),
            "Create depth texture"))
        return false;

    if (Failed(
            device_->CreateDepthStencilView(
                depthTexture_.Get(),
                nullptr,
                &dsv_),
            "CreateDepthStencilView"))
        return false;

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width =
        static_cast<float>(width);
    viewport_.Height =
        static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_.left = 0;
    scissor_.top = 0;
    scissor_.right =
        static_cast<LONG>(width);
    scissor_.bottom =
        static_cast<LONG>(height);

    return true;
}

const char* D3D11Backend::ShaderTarget(
    ShaderStage stage) const noexcept
{
    const bool vertex =
        stage == ShaderStage::Vertex;

    if (featureLevel_ >=
        D3D_FEATURE_LEVEL_11_0) {
        return vertex
            ? "vs_5_0"
            : "ps_5_0";
    }

    if (featureLevel_ ==
        D3D_FEATURE_LEVEL_10_1) {
        return vertex
            ? "vs_4_1"
            : "ps_4_1";
    }

    return vertex
        ? "vs_4_0"
        : "ps_4_0";
}

#endif

std::unique_ptr<IBackend> CreateD3D11Backend()
{
    return std::make_unique<D3D11Backend>();
}

} // namespace Hamun::RHI
