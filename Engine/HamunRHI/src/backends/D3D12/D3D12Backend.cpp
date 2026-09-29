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

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#if __has_include(<dxcapi.h>)
#include <dxcapi.h>
#define HAMUN_HAS_DXC_API 1
#elif __has_include(<dxc/dxcapi.h>)
#include <dxc/dxcapi.h>
#define HAMUN_HAS_DXC_API 1
#endif

namespace {

using Microsoft::WRL::ComPtr;

constexpr UINT kFrameCount = 2;
constexpr UINT kRtvDescriptorCapacity = 64;
constexpr UINT kDsvDescriptorCapacity = 32;
constexpr UINT kSrvDescriptorCapacity = 256;
constexpr UINT kSamplerDescriptorCapacity = 64;

bool Failed(HRESULT hr, const char* operation)
{
    if (SUCCEEDED(hr))
        return false;

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Error,
        std::string("D3D12: ") + operation + " failed.");
    return true;
}

std::wstring ToWide(std::string_view text)
{
    if (text.empty())
        return {};

    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);

    if (length <= 0)
        return {};

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        length);
    return result;
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

D3D12_HEAP_PROPERTIES UploadHeapProperties()
{
    D3D12_HEAP_PROPERTIES props{};
    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

D3D12_HEAP_PROPERTIES DefaultHeapProperties()
{
    D3D12_HEAP_PROPERTIES props{};
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

D3D12_RESOURCE_DESC BufferResourceDesc(UINT64 size)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

D3D12_RESOURCE_DESC TextureResourceDesc(
    UINT width,
    UINT height,
    DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    return desc;
}

D3D12_RESOURCE_DESC DepthResourceDesc(UINT width, UINT height)
{
    auto desc = TextureResourceDesc(width, height, DXGI_FORMAT_D32_FLOAT);
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    return desc;
}

D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
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

        case TextureFormat::RG16_Float:
            return DXGI_FORMAT_R16G16_FLOAT;

        case TextureFormat::RGBA16_Float:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case TextureFormat::R32_Float:
            return DXGI_FORMAT_R32_FLOAT;
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

D3D12_FILTER ToNativeFilter(Hamun::RHI::SamplerFilter filter)
{
    return filter == Hamun::RHI::SamplerFilter::Nearest
        ? D3D12_FILTER_MIN_MAG_MIP_POINT
        : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D12_TEXTURE_ADDRESS_MODE ToNativeAddress(
    Hamun::RHI::SamplerAddressMode mode)
{
    return mode == Hamun::RHI::SamplerAddressMode::Clamp
        ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
        : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

UINT64 AlignConstantBufferSize(UINT64 size)
{
    return (size + 255ull) & ~255ull;
}

bool CompileLegacy(
    const Hamun::RHI::ShaderDesc& desc,
    std::vector<std::uint8_t>& bytecode)
{
    const char* target = "ps_5_1";

    switch (desc.stage) {
        case Hamun::RHI::ShaderStage::Vertex:
            target = "vs_5_1";
            break;

        case Hamun::RHI::ShaderStage::Pixel:
            target = "ps_5_1";
            break;

        case Hamun::RHI::ShaderStage::Compute:
            target = "cs_5_1";
            break;
    }

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;

    const HRESULT hr = D3DCompile(
        desc.source.data(),
        desc.source.size(),
        "HamunRuntimeShader",
        nullptr,
        nullptr,
        desc.entryPoint.c_str(),
        target,
        flags,
        0,
        &shader,
        &errors);

    if (FAILED(hr)) {
        if (errors) {
            Hamun::Core::Log(
                Hamun::Core::LogLevel::Error,
                std::string_view(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize()));
        }
        return false;
    }

    const auto* begin =
        static_cast<const std::uint8_t*>(shader->GetBufferPointer());
    bytecode.assign(begin, begin + shader->GetBufferSize());

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Warning,
        "DXC unavailable; using the temporary D3DCompile Shader Model 5.1 fallback.");
    return true;
}

#if defined(HAMUN_HAS_DXC_API)
bool CompileDxc(
    const Hamun::RHI::ShaderDesc& desc,
    std::vector<std::uint8_t>& bytecode)
{
    HMODULE module = LoadLibraryW(L"dxcompiler.dll");
    if (!module)
        return false;

    const auto createInstance =
        reinterpret_cast<DxcCreateInstanceProc>(
            GetProcAddress(module, "DxcCreateInstance"));

    if (!createInstance) {
        FreeLibrary(module);
        return false;
    }

    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;

    if (FAILED(createInstance(
            CLSID_DxcUtils,
            IID_PPV_ARGS(&utils))) ||
        FAILED(createInstance(
            CLSID_DxcCompiler,
            IID_PPV_ARGS(&compiler)))) {
        FreeLibrary(module);
        return false;
    }

    const std::wstring entryPoint = ToWide(desc.entryPoint);
    const wchar_t* profile = L"ps_6_0";

    switch (desc.stage) {
        case Hamun::RHI::ShaderStage::Vertex:
            profile = L"vs_6_0";
            break;

        case Hamun::RHI::ShaderStage::Pixel:
            profile = L"ps_6_0";
            break;

        case Hamun::RHI::ShaderStage::Compute:
            profile = L"cs_6_0";
            break;
    }

    std::vector<LPCWSTR> arguments = {
        L"-E",
        entryPoint.c_str(),
        L"-T",
        profile,
        L"-HV",
        L"2021",
        L"-Ges",
#if defined(_DEBUG)
        L"-Zi",
        L"-Od"
#else
        L"-O3"
#endif
    };

    DxcBuffer source{};
    source.Ptr = desc.source.data();
    source.Size = desc.source.size();
    source.Encoding = DXC_CP_UTF8;

    ComPtr<IDxcResult> result;
    const HRESULT compileHr = compiler->Compile(
        &source,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        nullptr,
        IID_PPV_ARGS(&result));

    if (FAILED(compileHr) || !result) {
        FreeLibrary(module);
        return false;
    }

    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(
        DXC_OUT_ERRORS,
        IID_PPV_ARGS(&errors),
        nullptr);

    if (errors && errors->GetStringLength() > 0) {
        Hamun::Core::Log(
            Hamun::Core::LogLevel::Warning,
            std::string_view(
                errors->GetStringPointer(),
                errors->GetStringLength()));
    }

    HRESULT status = E_FAIL;
    result->GetStatus(&status);
    if (FAILED(status)) {
        FreeLibrary(module);
        return false;
    }

    ComPtr<IDxcBlob> object;
    if (FAILED(result->GetOutput(
            DXC_OUT_OBJECT,
            IID_PPV_ARGS(&object),
            nullptr)) ||
        !object) {
        FreeLibrary(module);
        return false;
    }

    const auto* begin =
        static_cast<const std::uint8_t*>(
            object->GetBufferPointer());
    bytecode.assign(begin, begin + object->GetBufferSize());

    object.Reset();
    result.Reset();
    compiler.Reset();
    utils.Reset();
    FreeLibrary(module);

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Info,
        "Shader compiled with DXC Shader Model 6.0.");
    return true;
}
#endif

bool CompileShader(
    const Hamun::RHI::ShaderDesc& desc,
    std::vector<std::uint8_t>& bytecode)
{
#if defined(HAMUN_HAS_DXC_API)
    if (CompileDxc(desc, bytecode))
        return true;
#endif
    return CompileLegacy(desc, bytecode);
}

} // namespace

#endif

namespace Hamun::RHI {

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

class D3D12Buffer final : public IBuffer {
public:
    D3D12Buffer(
        ComPtr<ID3D12Resource> resource,
        std::uint64_t size,
        BufferUsage usage)
        : resource_(std::move(resource))
        , size_(size)
        , usage_(usage)
    {
    }

    std::uint64_t Size() const noexcept override { return size_; }
    BufferUsage Usage() const noexcept override { return usage_; }

    bool Update(
        const void* data,
        std::uint64_t size,
        std::uint64_t offset) override
    {
        if (!data || size == 0 || offset + size > size_)
            return false;

        void* mapped = nullptr;
        const D3D12_RANGE readRange{0, 0};
        if (FAILED(resource_->Map(0, &readRange, &mapped)))
            return false;

        std::memcpy(
            static_cast<std::byte*>(mapped) + offset,
            data,
            static_cast<std::size_t>(size));

        resource_->Unmap(0, nullptr);
        return true;
    }

    ID3D12Resource* Native() const noexcept { return resource_.Get(); }

    void* NativeResourceHandle() noexcept override
    {
        return resource_.Get();
    }

private:
    ComPtr<ID3D12Resource> resource_;
    std::uint64_t size_ = 0;
    BufferUsage usage_ = BufferUsage::Vertex;
};

class D3D12Texture final : public ITexture {
public:
    D3D12Texture(
        ComPtr<ID3D12Resource> resource,
        std::uint32_t width,
        std::uint32_t height,
        TextureFormat format,
        TextureUsage usage,
        D3D12_RESOURCE_STATES initialState,
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
        : resource_(std::move(resource))
        , width_(width)
        , height_(height)
        , format_(format)
        , usage_(usage)
        , state_(initialState)
        , srvGpuHandle_(srvGpuHandle)
        , uavGpuHandle_(uavGpuHandle)
        , rtvHandle_(rtvHandle)
        , dsvHandle_(dsvHandle)
    {
    }

    std::uint32_t Width() const noexcept override { return width_; }
    std::uint32_t Height() const noexcept override { return height_; }
    TextureFormat Format() const noexcept override { return format_; }
    TextureUsage Usage() const noexcept override { return usage_; }

    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle() const noexcept
    {
        return srvGpuHandle_;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE UavGpuHandle() const noexcept
    {
        return uavGpuHandle_;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle() const noexcept
    {
        return rtvHandle_;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle() const noexcept
    {
        return dsvHandle_;
    }

    void Transition(
        ID3D12GraphicsCommandList* commandList,
        D3D12_RESOURCE_STATES nextState)
    {
        if (!commandList ||
            state_ == nextState) {
            return;
        }

        const auto barrier =
            TransitionBarrier(
                resource_.Get(),
                state_,
                nextState);

        commandList->ResourceBarrier(
            1,
            &barrier);

        state_ = nextState;
    }

    void* NativeResourceHandle() noexcept override
    {
        return resource_.Get();
    }

private:
    ComPtr<ID3D12Resource> resource_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    TextureFormat format_ = TextureFormat::RGBA8_UNorm;
    TextureUsage usage_ = TextureUsage::ShaderResource;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
};

class D3D12Sampler final : public ISampler {
public:
    explicit D3D12Sampler(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
        : gpuHandle_(gpuHandle)
    {
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle() const noexcept
    {
        return gpuHandle_;
    }

private:
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};
};

class D3D12Shader final : public IShader {
public:
    D3D12Shader(
        ShaderStage stage,
        std::vector<std::uint8_t> bytecode)
        : stage_(stage)
        , bytecode_(std::move(bytecode))
    {
    }

    ShaderStage Stage() const noexcept override { return stage_; }

    D3D12_SHADER_BYTECODE NativeBytecode() const noexcept
    {
        return {
            bytecode_.data(),
            bytecode_.size()
        };
    }

private:
    ShaderStage stage_;
    std::vector<std::uint8_t> bytecode_;
};

class D3D12Pipeline final : public IPipeline {
public:
    D3D12Pipeline(
        ComPtr<ID3D12RootSignature> rootSignature,
        ComPtr<ID3D12PipelineState> pipelineState,
        std::uint32_t constantBufferCount,
        std::uint32_t textureCount,
        std::uint32_t samplerCount)
        : rootSignature_(std::move(rootSignature))
        , pipelineState_(std::move(pipelineState))
        , constantBufferCount_(constantBufferCount)
        , textureCount_(textureCount)
        , samplerCount_(samplerCount)
    {
    }

    ID3D12RootSignature* RootSignature() const noexcept
    {
        return rootSignature_.Get();
    }

    ID3D12PipelineState* PipelineState() const noexcept
    {
        return pipelineState_.Get();
    }

    std::uint32_t ConstantBufferCount() const noexcept
    {
        return constantBufferCount_;
    }

    std::uint32_t TextureCount() const noexcept
    {
        return textureCount_;
    }

    std::uint32_t SamplerCount() const noexcept
    {
        return samplerCount_;
    }

    std::uint32_t TextureRootIndex(std::uint32_t slot) const noexcept
    {
        return constantBufferCount_ + slot;
    }

    std::uint32_t SamplerRootIndex(std::uint32_t slot) const noexcept
    {
        return constantBufferCount_ + textureCount_ + slot;
    }

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    std::uint32_t constantBufferCount_ = 0;
    std::uint32_t textureCount_ = 0;
    std::uint32_t samplerCount_ = 0;
};

class D3D12ComputePipeline final : public IPipeline {
public:
    D3D12ComputePipeline(
        ComPtr<ID3D12RootSignature> rootSignature,
        ComPtr<ID3D12PipelineState> pipelineState,
        std::uint32_t storageBufferCount,
        std::uint32_t storageTextureCount)
        : rootSignature_(std::move(rootSignature))
        , pipelineState_(std::move(pipelineState))
        , storageBufferCount_(storageBufferCount)
        , storageTextureCount_(storageTextureCount)
    {
    }

    ID3D12RootSignature* RootSignature() const noexcept
    {
        return rootSignature_.Get();
    }

    ID3D12PipelineState* PipelineState() const noexcept
    {
        return pipelineState_.Get();
    }

    std::uint32_t StorageBufferCount() const noexcept
    {
        return storageBufferCount_;
    }

    std::uint32_t StorageTextureCount() const noexcept
    {
        return storageTextureCount_;
    }

    std::uint32_t StorageTextureRootIndex(
        std::uint32_t slot) const noexcept
    {
        return storageBufferCount_ + slot;
    }

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    std::uint32_t storageBufferCount_ = 0;
    std::uint32_t storageTextureCount_ = 0;
};

class D3D12SwapChainView final : public ISwapChain {
public:
    void Bind(
        IDXGISwapChain3* swapChain,
        std::uint32_t width,
        std::uint32_t height)
    {
        swapChain_ = swapChain;
        width_ = width;
        height_ = height;
    }

    std::uint32_t Width() const noexcept override { return width_; }
    std::uint32_t Height() const noexcept override { return height_; }

    std::uint32_t FrameIndex() const noexcept override
    {
        return swapChain_ ? swapChain_->GetCurrentBackBufferIndex() : 0;
    }

    void* NativeSwapChainHandle() noexcept override
    {
        return swapChain_;
    }

private:
    IDXGISwapChain3* swapChain_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

class D3D12FenceView final : public IFence {
public:
    void Bind(ID3D12Fence* fence) { fence_ = fence; }

    std::uint64_t CompletedValue() const noexcept override
    {
        return fence_ ? fence_->GetCompletedValue() : 0;
    }

private:
    ID3D12Fence* fence_ = nullptr;
};

class D3D12CommandList final : public ICommandList {
public:
    void* NativeCommandListHandle() noexcept override
    {
        return commandList_;
    }

    void Prepare(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* renderTarget,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        ID3D12DescriptorHeap* srvHeap,
        ID3D12DescriptorHeap* samplerHeap,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor)
    {
        commandList_ = commandList;
        renderTarget_ = renderTarget;
        rtv_ = rtv;
        dsv_ = dsv;
        viewport_ = viewport;
        scissor_ = scissor;
        currentPipeline_ = nullptr;
        currentComputePipeline_ = nullptr;
        activeColorTarget_ = nullptr;
        activeDepthTarget_ = nullptr;
        offscreenRenderPass_ = false;
        renderPassOpen_ = false;

        ID3D12DescriptorHeap* heaps[] = {
            srvHeap,
            samplerHeap
        };
        commandList_->SetDescriptorHeaps(
            static_cast<UINT>(std::size(heaps)),
            heaps);
    }

    void SetComputePipeline(
        IPipeline& pipeline) override
    {
        auto* native =
            dynamic_cast<D3D12ComputePipeline*>(
                &pipeline);

        if (!native || !commandList_)
            return;

        currentComputePipeline_ =
            native;

        commandList_->SetComputeRootSignature(
            native->RootSignature());

        commandList_->SetPipelineState(
            native->PipelineState());
    }

    void SetComputeStorageBuffer(
        std::uint32_t slot,
        IBuffer& buffer) override
    {
        auto* native =
            dynamic_cast<D3D12Buffer*>(
                &buffer);

        if (!native ||
            !commandList_ ||
            !currentComputePipeline_ ||
            buffer.Usage() != BufferUsage::Storage ||
            slot >=
                currentComputePipeline_
                    ->StorageBufferCount()) {
            return;
        }

        commandList_
            ->SetComputeRootUnorderedAccessView(
                slot,
                native->Native()
                    ->GetGPUVirtualAddress());
    }

    void SetComputeStorageTexture(
        std::uint32_t slot,
        ITexture& texture) override
    {
        auto* native =
            dynamic_cast<D3D12Texture*>(
                &texture);

        if (!native ||
            !commandList_ ||
            !currentComputePipeline_ ||
            !HasTextureUsage(
                texture.Usage(),
                TextureUsage::Storage) ||
            slot >=
                currentComputePipeline_
                    ->StorageTextureCount()) {
            return;
        }

        const auto handle =
            native->UavGpuHandle();

        if (handle.ptr == 0)
            return;

        native->Transition(
            commandList_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        commandList_
            ->SetComputeRootDescriptorTable(
                currentComputePipeline_
                    ->StorageTextureRootIndex(
                        slot),
                handle);
    }

    void Dispatch(
        std::uint32_t groupCountX,
        std::uint32_t groupCountY,
        std::uint32_t groupCountZ) override
    {
        if (!commandList_ ||
            groupCountX == 0 ||
            groupCountY == 0 ||
            groupCountZ == 0) {
            return;
        }

        commandList_->Dispatch(
            groupCountX,
            groupCountY,
            groupCountZ);
    }

    void BeginRenderPass(
        const std::array<float, 4>& clearColor) override
    {
        if (!commandList_ || !renderTarget_ || renderPassOpen_)
            return;

        const auto barrier = TransitionBarrier(
            renderTarget_,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->ResourceBarrier(1, &barrier);

        commandList_->RSSetViewports(1, &viewport_);
        commandList_->RSSetScissorRects(1, &scissor_);
        commandList_->OMSetRenderTargets(1, &rtv_, FALSE, &dsv_);
        commandList_->ClearRenderTargetView(
            rtv_, clearColor.data(), 0, nullptr);
        commandList_->ClearDepthStencilView(
            dsv_,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);

        renderPassOpen_ = true;
    }

    void BeginRenderPassToTexture(
        ITexture& colorTarget,
        ITexture* depthTarget,
        const std::array<float, 4>& clearColor) override
    {
        auto* color =
            dynamic_cast<D3D12Texture*>(
                &colorTarget);

        auto* depth =
            depthTarget
                ? dynamic_cast<D3D12Texture*>(
                    depthTarget)
                : nullptr;

        if (!commandList_ ||
            !color ||
            !HasTextureUsage(
                color->Usage(),
                TextureUsage::RenderTarget) ||
            color->RtvHandle().ptr == 0 ||
            renderPassOpen_) {
            return;
        }

        if (depthTarget &&
            (!depth ||
             !HasTextureUsage(
                 depth->Usage(),
                 TextureUsage::DepthStencil) ||
             depth->DsvHandle().ptr == 0)) {
            return;
        }

        color->Transition(
            commandList_,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        if (depth) {
            depth->Transition(
                commandList_,
                D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }

        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width =
            static_cast<float>(
                color->Width());
        viewport.Height =
            static_cast<float>(
                color->Height());
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor{};
        scissor.left = 0;
        scissor.top = 0;
        scissor.right =
            static_cast<LONG>(
                color->Width());
        scissor.bottom =
            static_cast<LONG>(
                color->Height());

        const auto rtv =
            color->RtvHandle();

        if (depth) {
            const auto dsv =
                depth->DsvHandle();

            commandList_->OMSetRenderTargets(
                1,
                &rtv,
                FALSE,
                &dsv);

            commandList_->ClearDepthStencilView(
                dsv,
                D3D12_CLEAR_FLAG_DEPTH,
                1.0f,
                0,
                0,
                nullptr);
        } else {
            commandList_->OMSetRenderTargets(
                1,
                &rtv,
                FALSE,
                nullptr);
        }

        commandList_->RSSetViewports(
            1,
            &viewport);

        commandList_->RSSetScissorRects(
            1,
            &scissor);

        commandList_->ClearRenderTargetView(
            rtv,
            clearColor.data(),
            0,
            nullptr);

        activeColorTarget_ = color;
        activeDepthTarget_ = depth;
        offscreenRenderPass_ = true;
        renderPassOpen_ = true;
    }

    void SetPipeline(IPipeline& pipeline) override
    {
        auto* native = dynamic_cast<D3D12Pipeline*>(&pipeline);
        if (!native || !commandList_)
            return;

        currentPipeline_ = native;
        commandList_->SetGraphicsRootSignature(native->RootSignature());
        commandList_->SetPipelineState(native->PipelineState());
    }

    void SetVertexBuffer(
        IBuffer& buffer,
        std::uint32_t stride) override
    {
        auto* native = dynamic_cast<D3D12Buffer*>(&buffer);
        if (!native || !commandList_ || stride == 0)
            return;

        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = native->Native()->GetGPUVirtualAddress();
        view.SizeInBytes = static_cast<UINT>(native->Size());
        view.StrideInBytes = stride;

        commandList_->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList_->IASetVertexBuffers(0, 1, &view);
    }

    void SetIndexBuffer(
        IBuffer& buffer,
        IndexType indexType) override
    {
        auto* native = dynamic_cast<D3D12Buffer*>(&buffer);
        if (!native || !commandList_)
            return;

        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = native->Native()->GetGPUVirtualAddress();
        view.SizeInBytes = static_cast<UINT>(native->Size());
        view.Format =
            indexType == IndexType::UInt16
                ? DXGI_FORMAT_R16_UINT
                : DXGI_FORMAT_R32_UINT;

        commandList_->IASetIndexBuffer(&view);
    }

    void SetConstantBuffer(
        std::uint32_t slot,
        IBuffer& buffer) override
    {
        auto* native = dynamic_cast<D3D12Buffer*>(&buffer);
        if (!native || !commandList_ || !currentPipeline_)
            return;

        if (slot >= currentPipeline_->ConstantBufferCount())
            return;

        commandList_->SetGraphicsRootConstantBufferView(
            slot,
            native->Native()->GetGPUVirtualAddress());
    }

    void SetTexture(
        std::uint32_t slot,
        ITexture& texture) override
    {
        auto* native = dynamic_cast<D3D12Texture*>(&texture);
        if (!native || !commandList_ || !currentPipeline_)
            return;

        if (slot >= currentPipeline_->TextureCount())
            return;

        native->Transition(
            commandList_,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        commandList_->SetGraphicsRootDescriptorTable(
            currentPipeline_->TextureRootIndex(slot),
            native->GpuHandle());
    }

    void SetSampler(
        std::uint32_t slot,
        ISampler& sampler) override
    {
        auto* native = dynamic_cast<D3D12Sampler*>(&sampler);
        if (!native || !commandList_ || !currentPipeline_)
            return;

        if (slot >= currentPipeline_->SamplerCount())
            return;

        commandList_->SetGraphicsRootDescriptorTable(
            currentPipeline_->SamplerRootIndex(slot),
            native->GpuHandle());
    }

    void Draw(
        std::uint32_t vertexCount,
        std::uint32_t firstVertex) override
    {
        if (commandList_)
            commandList_->DrawInstanced(
                vertexCount, 1, firstVertex, 0);
    }

    void DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t firstIndex,
        std::int32_t vertexOffset) override
    {
        if (commandList_)
            commandList_->DrawIndexedInstanced(
                indexCount,
                1,
                firstIndex,
                vertexOffset,
                0);
    }

    void EndRenderPass() override
    {
        if (!commandList_ ||
            !renderPassOpen_) {
            return;
        }

        if (offscreenRenderPass_) {
            constexpr D3D12_RESOURCE_STATES
                readableState =
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            if (activeColorTarget_) {
                activeColorTarget_->Transition(
                    commandList_,
                    readableState);
            }

            if (activeDepthTarget_) {
                activeDepthTarget_->Transition(
                    commandList_,
                    readableState);
            }

            activeColorTarget_ = nullptr;
            activeDepthTarget_ = nullptr;
            offscreenRenderPass_ = false;
        } else if (renderTarget_) {
            const auto barrier =
                TransitionBarrier(
                    renderTarget_,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT);

            commandList_->ResourceBarrier(
                1,
                &barrier);
        }

        renderPassOpen_ = false;
    }

private:
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    ID3D12Resource* renderTarget_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_{};
    D3D12Pipeline* currentPipeline_ = nullptr;
    D3D12ComputePipeline* currentComputePipeline_ = nullptr;
    D3D12Texture* activeColorTarget_ = nullptr;
    D3D12Texture* activeDepthTarget_ = nullptr;
    bool offscreenRenderPass_ = false;
    bool renderPassOpen_ = false;
};

#endif

class D3D12Backend final : public IBackend {
public:
    D3D12Backend();
    ~D3D12Backend() override;

    std::string_view Name() const noexcept override { return "Direct3D 12"; }
    BackendType Type() const noexcept override { return BackendType::D3D12; }
    const Capabilities& Caps() const noexcept override { return caps_; }
    const AdapterInfo& Adapter() const noexcept override { return adapterInfo_; }

    void* NativeDeviceHandle() noexcept override
    {
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
        return device_.Get();
#else
        return nullptr;
#endif
    }

    void* NativeCommandQueueHandle() noexcept override
    {
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
        return queue_.Get();
#else
        return nullptr;
#endif
    }

    bool Initialize(const BackendCreateInfo& createInfo) override;
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

    std::unique_ptr<IPipeline> CreateComputePipeline(
        const ComputePipelineDesc& desc) override;

    ICommandList* BeginFrame() override;
    bool SubmitFrame() override;

    ISwapChain* SwapChain() noexcept override;
    IFence* FrameFence() noexcept override;

private:
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    bool CreateDeviceAndQueue(bool enableValidation);
    bool CreateSwapChain(
        HWND hwnd,
        std::uint32_t width,
        std::uint32_t height);
    bool CreateFrameResources();
    bool CreateShaderVisibleHeaps();
    bool UploadTexture(
        ID3D12Resource* texture,
        const TextureDesc& desc);
    bool WaitForFrame(std::uint32_t submittedFrame);
    void WaitForGpu();

    ComPtr<IDXGIFactory6> factory_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    ComPtr<ID3D12DescriptorHeap> srvHeap_;
    ComPtr<ID3D12DescriptorHeap> samplerHeap_;
    ComPtr<ID3D12Resource> depthBuffer_;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> renderTargets_;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> allocators_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12Fence> fence_;

    D3D12CommandList commandListView_;
    D3D12SwapChainView swapChainView_;
    D3D12FenceView fenceView_;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_{};

    HANDLE fenceEvent_ = nullptr;
    std::array<std::uint64_t, kFrameCount> frameFenceValues_{};
    std::uint64_t nextFenceValue_ = 1;
    std::uint32_t frameIndex_ = 0;
    UINT rtvDescriptorSize_ = 0;
    UINT dsvDescriptorSize_ = 0;
    UINT srvDescriptorSize_ = 0;
    UINT samplerDescriptorSize_ = 0;
    UINT nextRtvDescriptor_ = kFrameCount;
    UINT nextDsvDescriptor_ = 1;
    UINT nextSrvDescriptor_ = 0;
    UINT nextSamplerDescriptor_ = 0;
    bool initialized_ = false;
#endif

    AdapterInfo adapterInfo_{};
    Capabilities caps_{};
};

D3D12Backend::D3D12Backend() = default;

D3D12Backend::~D3D12Backend()
{
    Shutdown();
}

bool D3D12Backend::Initialize(const BackendCreateInfo& createInfo)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!createInfo.nativeWindowHandle) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12 requires a valid native Win32 window handle.");
        return false;
    }

    if (!CreateDeviceAndQueue(createInfo.enableValidation))
        return false;

    if (!CreateSwapChain(
            static_cast<HWND>(createInfo.nativeWindowHandle),
            createInfo.width,
            createInfo.height))
        return false;

    if (!CreateFrameResources())
        return false;

    if (!CreateShaderVisibleHeaps())
        return false;

    swapChainView_.Bind(
        swapChain_.Get(),
        createInfo.width,
        createInfo.height);
    fenceView_.Bind(fence_.Get());

    initialized_ = true;

    Core::Log(
        Core::LogLevel::Info,
        "D3D12 RHI initialized with textures, samplers, descriptors and DXC-ready shaders.");
    return true;
#else
    (void)createInfo;
    Core::Log(
        Core::LogLevel::Warning,
        "D3D12 backend is unavailable in this build.");
    return false;
#endif
}

void D3D12Backend::Shutdown()
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!device_)
        return;

    WaitForGpu();

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    commandList_.Reset();
    for (auto& allocator : allocators_)
        allocator.Reset();
    for (auto& target : renderTargets_)
        target.Reset();

    depthBuffer_.Reset();
    samplerHeap_.Reset();
    srvHeap_.Reset();
    dsvHeap_.Reset();
    fence_.Reset();
    rtvHeap_.Reset();
    swapChain_.Reset();
    queue_.Reset();
    device_.Reset();
    adapter_.Reset();
    factory_.Reset();

    nextRtvDescriptor_ = kFrameCount;
    nextDsvDescriptor_ = 1;
    nextSrvDescriptor_ = 0;
    nextSamplerDescriptor_ = 0;
    initialized_ = false;
#endif
}

std::unique_ptr<IBuffer> D3D12Backend::CreateBuffer(
    const BufferDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_ || desc.size == 0)
        return {};

    const std::uint64_t allocationSize =
        desc.usage == BufferUsage::Constant
            ? AlignConstantBufferSize(desc.size)
            : desc.size;

    const bool storage =
        desc.usage == BufferUsage::Storage;

    if (storage &&
        (desc.stride == 0 ||
         allocationSize % desc.stride != 0 ||
         desc.initialData != nullptr)) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12 storage buffers require a valid stride and no direct initialData upload yet.");
        return {};
    }

    const D3D12_HEAP_PROPERTIES heapProps =
        storage
            ? DefaultHeapProperties()
            : UploadHeapProperties();

    auto resourceDesc =
        BufferResourceDesc(
            allocationSize);

    if (storage) {
        resourceDesc.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    ComPtr<ID3D12Resource> resource;

    if (Failed(
            device_->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                storage
                    ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                    : D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&resource)),
            "CreateCommittedResource(buffer)"))
        return {};

    auto result = std::make_unique<D3D12Buffer>(
        std::move(resource),
        allocationSize,
        desc.usage);

    if (desc.initialData &&
        !result->Update(desc.initialData, desc.size, 0))
        return {};

    return result;
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<ITexture> D3D12Backend::CreateTexture(
    const TextureDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_ ||
        desc.width == 0 ||
        desc.height == 0 ||
        desc.usage == TextureUsage::None) {
        return {};
    }

    const bool shaderResource =
        HasTextureUsage(
            desc.usage,
            TextureUsage::ShaderResource);

    const bool storage =
        HasTextureUsage(
            desc.usage,
            TextureUsage::Storage);

    const bool renderTarget =
        HasTextureUsage(
            desc.usage,
            TextureUsage::RenderTarget);

    const bool depthStencil =
        HasTextureUsage(
            desc.usage,
            TextureUsage::DepthStencil);

    if (depthStencil &&
        (storage || renderTarget)) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12 depth textures cannot also be storage/render-target textures in this bootstrap path.");
        return {};
    }

    if (renderTarget &&
        nextRtvDescriptor_ >=
            kRtvDescriptorCapacity) {
        return {};
    }

    if (depthStencil &&
        nextDsvDescriptor_ >=
            kDsvDescriptorCapacity) {
        return {};
    }

    const std::uint32_t descriptorCount =
        (shaderResource ? 1u : 0u) +
        (storage ? 1u : 0u);

    if (nextSrvDescriptor_ +
            descriptorCount >
        kSrvDescriptorCapacity) {
        return {};
    }

    if (desc.initialData &&
        storage) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12 storage textures do not support direct initialData upload yet.");
        return {};
    }

    const DXGI_FORMAT format =
        ToDxgiFormat(
            desc.format);

    const DXGI_FORMAT resourceFormat =
        depthStencil &&
        shaderResource
            ? DXGI_FORMAT_R32_TYPELESS
            : depthStencil
                ? DXGI_FORMAT_D32_FLOAT
                : format;

    const D3D12_HEAP_PROPERTIES heapProps =
        DefaultHeapProperties();

    auto resourceDesc =
        TextureResourceDesc(
            desc.width,
            desc.height,
            resourceFormat);

    if (storage) {
        resourceDesc.Flags |=
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (renderTarget) {
        resourceDesc.Flags |=
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }

    if (depthStencil) {
        resourceDesc.Flags |=
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    const D3D12_RESOURCE_STATES initialState =
        desc.initialData
            ? D3D12_RESOURCE_STATE_COPY_DEST
            : depthStencil
                ? D3D12_RESOURCE_STATE_DEPTH_WRITE
                : storage
                    ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                    : renderTarget
                        ? D3D12_RESOURCE_STATE_RENDER_TARGET
                        : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    D3D12_CLEAR_VALUE clearValue{};
    const D3D12_CLEAR_VALUE* clearValuePtr =
        nullptr;

    if (renderTarget) {
        clearValue.Format =
            format;
        clearValuePtr =
            &clearValue;
    }

    if (depthStencil) {
        clearValue.Format =
            DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth =
            1.0f;
        clearValue.DepthStencil.Stencil =
            0;
        clearValuePtr =
            &clearValue;
    }

    ComPtr<ID3D12Resource> texture;

    if (Failed(
            device_->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                initialState,
                clearValuePtr,
                IID_PPV_ARGS(&texture)),
            "CreateCommittedResource(texture)")) {
        return {};
    }

    if (desc.initialData &&
        !UploadTexture(
            texture.Get(),
            desc)) {
        return {};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE
        srvGpu{};

    D3D12_GPU_DESCRIPTOR_HANDLE
        uavGpu{};

    if (shaderResource) {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            srvHeap_
                ->GetCPUDescriptorHandleForHeapStart();

        cpu.ptr +=
            static_cast<SIZE_T>(
                nextSrvDescriptor_) *
            srvDescriptorSize_;

        srvGpu =
            srvHeap_
                ->GetGPUDescriptorHandleForHeapStart();

        srvGpu.ptr +=
            static_cast<UINT64>(
                nextSrvDescriptor_) *
            srvDescriptorSize_;

        D3D12_SHADER_RESOURCE_VIEW_DESC
            srv{};

        srv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        srv.Format =
            depthStencil
                ? DXGI_FORMAT_R32_FLOAT
                : format;

        srv.ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;

        srv.Texture2D.MostDetailedMip =
            0;

        srv.Texture2D.MipLevels =
            1;

        device_->CreateShaderResourceView(
            texture.Get(),
            &srv,
            cpu);

        ++nextSrvDescriptor_;
    }

    if (storage) {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            srvHeap_
                ->GetCPUDescriptorHandleForHeapStart();

        cpu.ptr +=
            static_cast<SIZE_T>(
                nextSrvDescriptor_) *
            srvDescriptorSize_;

        uavGpu =
            srvHeap_
                ->GetGPUDescriptorHandleForHeapStart();

        uavGpu.ptr +=
            static_cast<UINT64>(
                nextSrvDescriptor_) *
            srvDescriptorSize_;

        D3D12_UNORDERED_ACCESS_VIEW_DESC
            uav{};

        uav.Format =
            format;

        uav.ViewDimension =
            D3D12_UAV_DIMENSION_TEXTURE2D;

        device_->CreateUnorderedAccessView(
            texture.Get(),
            nullptr,
            &uav,
            cpu);

        ++nextSrvDescriptor_;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE
        rtvHandle{};

    D3D12_CPU_DESCRIPTOR_HANDLE
        dsvHandle{};

    if (renderTarget) {
        rtvHandle =
            rtvHeap_
                ->GetCPUDescriptorHandleForHeapStart();

        rtvHandle.ptr +=
            static_cast<SIZE_T>(
                nextRtvDescriptor_) *
            rtvDescriptorSize_;

        D3D12_RENDER_TARGET_VIEW_DESC
            rtvDesc{};

        rtvDesc.Format =
            format;

        rtvDesc.ViewDimension =
            D3D12_RTV_DIMENSION_TEXTURE2D;

        device_->CreateRenderTargetView(
            texture.Get(),
            &rtvDesc,
            rtvHandle);

        ++nextRtvDescriptor_;
    }

    if (depthStencil) {
        dsvHandle =
            dsvHeap_
                ->GetCPUDescriptorHandleForHeapStart();

        dsvHandle.ptr +=
            static_cast<SIZE_T>(
                nextDsvDescriptor_) *
            dsvDescriptorSize_;

        D3D12_DEPTH_STENCIL_VIEW_DESC
            dsvDesc{};

        dsvDesc.Format =
            DXGI_FORMAT_D32_FLOAT;

        dsvDesc.ViewDimension =
            D3D12_DSV_DIMENSION_TEXTURE2D;

        device_->CreateDepthStencilView(
            texture.Get(),
            &dsvDesc,
            dsvHandle);

        ++nextDsvDescriptor_;
    }

    return std::make_unique<D3D12Texture>(
        std::move(texture),
        desc.width,
        desc.height,
        desc.format,
        desc.usage,
        initialState,
        srvGpu,
        uavGpu,
        rtvHandle,
        dsvHandle);
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<ISampler> D3D12Backend::CreateSampler(
    const SamplerDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_ ||
        nextSamplerDescriptor_ >= kSamplerDescriptorCapacity)
        return {};

    D3D12_CPU_DESCRIPTOR_HANDLE cpu =
        samplerHeap_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr +=
        static_cast<SIZE_T>(nextSamplerDescriptor_) *
        samplerDescriptorSize_;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu =
        samplerHeap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr +=
        static_cast<UINT64>(nextSamplerDescriptor_) *
        samplerDescriptorSize_;

    D3D12_SAMPLER_DESC sampler{};
    sampler.Filter = ToNativeFilter(desc.filter);
    sampler.AddressU = ToNativeAddress(desc.addressU);
    sampler.AddressV = ToNativeAddress(desc.addressV);
    sampler.AddressW = ToNativeAddress(desc.addressW);
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    device_->CreateSampler(&sampler, cpu);
    ++nextSamplerDescriptor_;

    return std::make_unique<D3D12Sampler>(gpu);
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<IShader> D3D12Backend::CreateShader(
    const ShaderDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_ ||
        desc.source.empty() ||
        desc.entryPoint.empty())
        return {};

    std::vector<std::uint8_t> bytecode;
    if (!CompileShader(desc, bytecode))
        return {};

    return std::make_unique<D3D12Shader>(
        desc.stage,
        std::move(bytecode));
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<IPipeline> D3D12Backend::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    auto* vs = dynamic_cast<D3D12Shader*>(desc.vertexShader);
    auto* ps = dynamic_cast<D3D12Shader*>(desc.pixelShader);

    if (!initialized_ ||
        !vs ||
        !ps ||
        vs->Stage() != ShaderStage::Vertex ||
        ps->Stage() != ShaderStage::Pixel ||
        (!desc.vertexAttributes.empty() &&
         desc.vertexStride == 0))
        return {};

    const std::uint32_t rootParameterCount =
        desc.constantBufferCount +
        desc.textureCount +
        desc.samplerCount;

    std::vector<D3D12_ROOT_PARAMETER> rootParameters(
        rootParameterCount);
    std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges(
        desc.textureCount);
    std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges(
        desc.samplerCount);

    for (std::uint32_t i = 0;
         i < desc.constantBufferCount;
         ++i) {
        auto& parameter = rootParameters[i];
        parameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameter.Descriptor.ShaderRegister = i;
        parameter.Descriptor.RegisterSpace = 0;
        parameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;
    }

    for (std::uint32_t i = 0;
         i < desc.textureCount;
         ++i) {
        auto& range = srvRanges[i];
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = i;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = 0;

        auto& parameter =
            rootParameters[desc.constantBufferCount + i];
        parameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        parameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;
    }

    for (std::uint32_t i = 0;
         i < desc.samplerCount;
         ++i) {
        auto& range = samplerRanges[i];
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = i;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = 0;

        auto& parameter =
            rootParameters[
                desc.constantBufferCount +
                desc.textureCount +
                i];
        parameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        parameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters =
        static_cast<UINT>(rootParameters.size());
    rootDesc.pParameters =
        rootParameters.empty()
            ? nullptr
            : rootParameters.data();
    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRoot;
    ComPtr<ID3DBlob> errors;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRoot,
        &errors);

    if (FAILED(hr)) {
        if (errors) {
            Core::Log(
                Core::LogLevel::Error,
                std::string_view(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize()));
        }
        return {};
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    if (Failed(
            device_->CreateRootSignature(
                0,
                serializedRoot->GetBufferPointer(),
                serializedRoot->GetBufferSize(),
                IID_PPV_ARGS(&rootSignature)),
            "CreateRootSignature"))
        return {};

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    inputLayout.reserve(desc.vertexAttributes.size());

    for (const VertexAttribute& attribute : desc.vertexAttributes) {
        D3D12_INPUT_ELEMENT_DESC element{};
        element.SemanticName = SemanticName(attribute.semantic);
        element.SemanticIndex = attribute.semanticIndex;
        element.Format = ToDxgiFormat(attribute.format);
        element.InputSlot = 0;
        element.AlignedByteOffset = attribute.offset;
        element.InputSlotClass =
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        inputLayout.push_back(element);
    }

    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC raster{};
    raster.FillMode = D3D12_FILL_MODE_SOLID;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    raster.FrontCounterClockwise = FALSE;
    raster.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    raster.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    raster.SlopeScaledDepthBias =
        D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    raster.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = desc.depthTest ? TRUE : FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature.Get();
    pso.VS = vs->NativeBytecode();
    pso.PS = ps->NativeBytecode();
    pso.BlendState = blend;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = raster;
    pso.DepthStencilState = depth;
    pso.InputLayout = {
        inputLayout.data(),
        static_cast<UINT>(inputLayout.size())};
    pso.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] =
        ToDxgiFormat(
            desc.renderTargetFormat);
    pso.DSVFormat =
        desc.depthTest
            ? DXGI_FORMAT_D32_FLOAT
            : DXGI_FORMAT_UNKNOWN;
    pso.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipelineState;
    if (Failed(
            device_->CreateGraphicsPipelineState(
                &pso,
                IID_PPV_ARGS(&pipelineState)),
            "CreateGraphicsPipelineState"))
        return {};

    return std::make_unique<D3D12Pipeline>(
        std::move(rootSignature),
        std::move(pipelineState),
        desc.constantBufferCount,
        desc.textureCount,
        desc.samplerCount);
#else
    (void)desc;
    return {};
#endif
}

std::unique_ptr<IPipeline>
D3D12Backend::CreateComputePipeline(
    const ComputePipelineDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    auto* cs =
        dynamic_cast<D3D12Shader*>(
            desc.computeShader);

    if (!initialized_ ||
        !cs ||
        cs->Stage() != ShaderStage::Compute) {
        return {};
    }

    const std::uint32_t rootParameterCount =
        desc.storageBufferCount +
        desc.storageTextureCount;

    std::vector<D3D12_ROOT_PARAMETER>
        rootParameters(
            rootParameterCount);

    std::vector<D3D12_DESCRIPTOR_RANGE>
        storageTextureRanges(
            desc.storageTextureCount);

    for (std::uint32_t i = 0;
         i < desc.storageBufferCount;
         ++i) {
        auto& parameter =
            rootParameters[i];

        parameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_UAV;

        parameter.Descriptor.ShaderRegister =
            i;

        parameter.Descriptor.RegisterSpace =
            0;

        parameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;
    }

    for (std::uint32_t i = 0;
         i < desc.storageTextureCount;
         ++i) {
        auto& range =
            storageTextureRanges[i];

        range.RangeType =
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

        range.NumDescriptors =
            1;

        range.BaseShaderRegister =
            desc.storageBufferCount +
            i;

        range.RegisterSpace =
            0;

        range.OffsetInDescriptorsFromTableStart =
            0;

        auto& parameter =
            rootParameters[
                desc.storageBufferCount +
                i];

        parameter.ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

        parameter.DescriptorTable.NumDescriptorRanges =
            1;

        parameter.DescriptorTable.pDescriptorRanges =
            &range;

        parameter.ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters =
        static_cast<UINT>(
            rootParameters.size());

    rootDesc.pParameters =
        rootParameters.empty()
            ? nullptr
            : rootParameters.data();

    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serializedRoot;
    ComPtr<ID3DBlob> errors;

    const HRESULT rootHr =
        D3D12SerializeRootSignature(
            &rootDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRoot,
            &errors);

    if (FAILED(rootHr)) {
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

    ComPtr<ID3D12RootSignature>
        rootSignature;

    if (Failed(
            device_->CreateRootSignature(
                0,
                serializedRoot->GetBufferPointer(),
                serializedRoot->GetBufferSize(),
                IID_PPV_ARGS(
                    &rootSignature)),
            "Create compute root signature")) {
        return {};
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC
        pipelineDesc{};

    pipelineDesc.pRootSignature =
        rootSignature.Get();

    pipelineDesc.CS =
        cs->NativeBytecode();

    ComPtr<ID3D12PipelineState>
        pipelineState;

    if (Failed(
            device_->CreateComputePipelineState(
                &pipelineDesc,
                IID_PPV_ARGS(
                    &pipelineState)),
            "CreateComputePipelineState")) {
        return {};
    }

    return
        std::make_unique<
            D3D12ComputePipeline>(
                std::move(rootSignature),
                std::move(pipelineState),
                desc.storageBufferCount,
                desc.storageTextureCount);
#else
    (void)desc;
    return {};
#endif
}

ICommandList* D3D12Backend::BeginFrame()
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_)
        return nullptr;

    if (Failed(
            allocators_[frameIndex_]->Reset(),
            "CommandAllocator::Reset"))
        return nullptr;

    if (Failed(
            commandList_->Reset(
                allocators_[frameIndex_].Get(),
                nullptr),
            "CommandList::Reset"))
        return nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr +=
        static_cast<SIZE_T>(frameIndex_) *
        rtvDescriptorSize_;

    const D3D12_CPU_DESCRIPTOR_HANDLE dsv =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandListView_.Prepare(
        commandList_.Get(),
        renderTargets_[frameIndex_].Get(),
        rtv,
        dsv,
        srvHeap_.Get(),
        samplerHeap_.Get(),
        viewport_,
        scissor_);

    return &commandListView_;
#else
    return nullptr;
#endif
}

bool D3D12Backend::SubmitFrame()
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_)
        return false;

    const std::uint32_t submittedFrame = frameIndex_;

    if (Failed(
            commandList_->Close(),
            "CommandList::Close"))
        return false;

    ID3D12CommandList* lists[] = {
        commandList_.Get()
    };
    queue_->ExecuteCommandLists(1, lists);

    if (Failed(
            swapChain_->Present(1, 0),
            "SwapChain::Present"))
        return false;

    return WaitForFrame(submittedFrame);
#else
    return false;
#endif
}

ISwapChain* D3D12Backend::SwapChain() noexcept
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    return initialized_ ? &swapChainView_ : nullptr;
#else
    return nullptr;
#endif
}

IFence* D3D12Backend::FrameFence() noexcept
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    return initialized_ ? &fenceView_ : nullptr;
#else
    return nullptr;
#endif
}

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

bool D3D12Backend::CreateDeviceAndQueue(bool enableValidation)
{
    UINT factoryFlags = 0;

    if (enableValidation) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(
                D3D12GetDebugInterface(
                    IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            Core::Log(
                Core::LogLevel::Info,
                "D3D12 debug layer enabled.");
        }
    }

    if (Failed(
            CreateDXGIFactory2(
                factoryFlags,
                IID_PPV_ARGS(&factory_)),
            "CreateDXGIFactory2"))
        return false;

    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory_->EnumAdapterByGpuPreference(
                index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&candidate)) ==
            DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC1 adapterDesc{};
        candidate->GetDesc1(&adapterDesc);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(
                D3D12CreateDevice(
                    candidate.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    __uuidof(ID3D12Device),
                    nullptr))) {
            adapter_ = candidate;
            break;
        }
    }

    if (!adapter_) {
        ComPtr<IDXGIAdapter> warp;
        if (Failed(
                factory_->EnumWarpAdapter(
                    IID_PPV_ARGS(&warp)),
                "EnumWarpAdapter"))
            return false;

        if (Failed(
                warp.As(&adapter_),
                "Query WARP adapter"))
            return false;

        Core::Log(
            Core::LogLevel::Warning,
            "D3D12 hardware adapter unavailable; using WARP.");
    }

    if (Failed(
            D3D12CreateDevice(
                adapter_.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device_)),
            "D3D12CreateDevice"))
        return false;

    DXGI_ADAPTER_DESC1 adapterDesc{};
    if (SUCCEEDED(
            adapter_->GetDesc1(
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

    caps_.compute = true;
    caps_.asyncCompute = true;
    caps_.indirectDraw = true;

    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    if (SUCCEEDED(
            device_->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS,
                &options,
                sizeof(options)))) {
        caps_.bindless =
            options.ResourceBindingTier >=
            D3D12_RESOURCE_BINDING_TIER_2;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    if (SUCCEEDED(
            device_->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS5,
                &options5,
                sizeof(options5)))) {
        caps_.rayTracing =
            options5.RaytracingTier !=
            D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
    if (SUCCEEDED(
            device_->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS7,
                &options7,
                sizeof(options7)))) {
        caps_.meshShaders =
            options7.MeshShaderTier !=
            D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    return !Failed(
        device_->CreateCommandQueue(
            &queueDesc,
            IID_PPV_ARGS(&queue_)),
        "CreateCommandQueue");
}

bool D3D12Backend::CreateSwapChain(
    HWND hwnd,
    std::uint32_t width,
    std::uint32_t height)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kFrameCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (Failed(
            factory_->CreateSwapChainForHwnd(
                queue_.Get(),
                hwnd,
                &desc,
                nullptr,
                nullptr,
                &swapChain1),
            "CreateSwapChainForHwnd"))
        return false;

    factory_->MakeWindowAssociation(
        hwnd,
        DXGI_MWA_NO_ALT_ENTER);

    if (Failed(
            swapChain1.As(&swapChain_),
            "Query IDXGISwapChain3"))
        return false;

    frameIndex_ =
        swapChain_->GetCurrentBackBufferIndex();

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_.left = 0;
    scissor_.top = 0;
    scissor_.right = static_cast<LONG>(width);
    scissor_.bottom = static_cast<LONG>(height);
    return true;
}

bool D3D12Backend::CreateFrameResources()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors =
        kRtvDescriptorCapacity;

    if (Failed(
            device_->CreateDescriptorHeap(
                &rtvHeapDesc,
                IID_PPV_ARGS(&rtvHeap_)),
            "Create RTV heap"))
        return false;

    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kFrameCount; ++i) {
        if (Failed(
                swapChain_->GetBuffer(
                    i,
                    IID_PPV_ARGS(&renderTargets_[i])),
                "Get swap-chain buffer"))
            return false;

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            rtv);
        rtv.ptr += rtvDescriptorSize_;

        if (Failed(
                device_->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&allocators_[i])),
                "CreateCommandAllocator"))
            return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors =
        kDsvDescriptorCapacity;

    if (Failed(
            device_->CreateDescriptorHeap(
                &dsvHeapDesc,
                IID_PPV_ARGS(&dsvHeap_)),
            "Create DSV heap"))
        return false;

    const D3D12_HEAP_PROPERTIES defaultHeap =
        DefaultHeapProperties();

    const D3D12_RESOURCE_DESC depthDesc =
        DepthResourceDesc(
            static_cast<UINT>(viewport_.Width),
            static_cast<UINT>(viewport_.Height));

    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth = 1.0f;

    if (Failed(
            device_->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depthClear,
                IID_PPV_ARGS(&depthBuffer_)),
            "Create depth buffer"))
        return false;

    dsvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension =
        D3D12_DSV_DIMENSION_TEXTURE2D;

    device_->CreateDepthStencilView(
        depthBuffer_.Get(),
        &dsvDesc,
        dsvHeap_->GetCPUDescriptorHandleForHeapStart());

    if (Failed(
            device_->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocators_[frameIndex_].Get(),
                nullptr,
                IID_PPV_ARGS(&commandList_)),
            "CreateCommandList"))
        return false;

    if (Failed(
            commandList_->Close(),
            "Initial CommandList::Close"))
        return false;

    if (Failed(
            device_->CreateFence(
                0,
                D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(&fence_)),
            "CreateFence"))
        return false;

    fenceEvent_ =
        CreateEventW(
            nullptr,
            FALSE,
            FALSE,
            nullptr);

    if (!fenceEvent_) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12: CreateEventW failed.");
        return false;
    }

    return true;
}

bool D3D12Backend::CreateShaderVisibleHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.NumDescriptors = kSrvDescriptorCapacity;
    srvDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (Failed(
            device_->CreateDescriptorHeap(
                &srvDesc,
                IID_PPV_ARGS(&srvHeap_)),
            "Create shader-visible SRV heap"))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC samplerDesc{};
    samplerDesc.NumDescriptors =
        kSamplerDescriptorCapacity;
    samplerDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerDesc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (Failed(
            device_->CreateDescriptorHeap(
                &samplerDesc,
                IID_PPV_ARGS(&samplerHeap_)),
            "Create shader-visible sampler heap"))
        return false;

    srvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    samplerDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    return true;
}

bool D3D12Backend::UploadTexture(
    ID3D12Resource* texture,
    const TextureDesc& desc)
{
    if (!texture || !desc.initialData)
        return false;

    const D3D12_RESOURCE_DESC textureDesc =
        texture->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 uploadSize = 0;

    device_->GetCopyableFootprints(
        &textureDesc,
        0,
        1,
        0,
        &footprint,
        &numRows,
        &rowSize,
        &uploadSize);

    const D3D12_HEAP_PROPERTIES uploadHeap =
        UploadHeapProperties();
    const D3D12_RESOURCE_DESC uploadDesc =
        BufferResourceDesc(uploadSize);

    ComPtr<ID3D12Resource> upload;
    if (Failed(
            device_->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &uploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&upload)),
            "Create texture upload buffer"))
        return false;

    void* mapped = nullptr;
    const D3D12_RANGE readRange{0, 0};
    if (Failed(
            upload->Map(
                0,
                &readRange,
                &mapped),
            "Map texture upload buffer"))
        return false;

    const std::uint32_t sourceRowPitch =
        desc.rowPitch != 0
            ? desc.rowPitch
            : desc.width * 4u;

    const auto* source =
        static_cast<const std::uint8_t*>(desc.initialData);
    auto* destination =
        static_cast<std::uint8_t*>(mapped) +
        footprint.Offset;

    const std::size_t copyBytes =
        std::min<std::size_t>(
            static_cast<std::size_t>(rowSize),
            sourceRowPitch);

    for (UINT row = 0; row < numRows; ++row) {
        std::memcpy(
            destination +
                static_cast<std::size_t>(row) *
                footprint.Footprint.RowPitch,
            source +
                static_cast<std::size_t>(row) *
                sourceRowPitch,
            copyBytes);
    }

    upload->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;

    if (Failed(
            device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&allocator)),
            "Create texture upload allocator"))
        return false;

    if (Failed(
            device_->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocator.Get(),
                nullptr,
                IID_PPV_ARGS(&list)),
            "Create texture upload command list"))
        return false;

    D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
    destinationLocation.pResource = texture;
    destinationLocation.Type =
        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destinationLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
    sourceLocation.pResource = upload.Get();
    sourceLocation.Type =
        D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    sourceLocation.PlacedFootprint = footprint;

    list->CopyTextureRegion(
        &destinationLocation,
        0,
        0,
        0,
        &sourceLocation,
        nullptr);

    const auto barrier = TransitionBarrier(
        texture,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1, &barrier);

    if (Failed(
            list->Close(),
            "Close texture upload command list"))
        return false;

    ID3D12CommandList* lists[] = {
        list.Get()
    };
    queue_->ExecuteCommandLists(1, lists);
    WaitForGpu();
    return true;
}

bool D3D12Backend::WaitForFrame(
    std::uint32_t submittedFrame)
{
    const std::uint64_t fenceValue =
        nextFenceValue_++;

    if (Failed(
            queue_->Signal(
                fence_.Get(),
                fenceValue),
            "CommandQueue::Signal"))
        return false;

    frameFenceValues_[submittedFrame] =
        fenceValue;
    frameIndex_ =
        swapChain_->GetCurrentBackBufferIndex();

    const std::uint64_t waitValue =
        frameFenceValues_[frameIndex_];

    if (waitValue != 0 &&
        fence_->GetCompletedValue() < waitValue) {
        if (Failed(
                fence_->SetEventOnCompletion(
                    waitValue,
                    fenceEvent_),
                "Fence::SetEventOnCompletion"))
            return false;

        WaitForSingleObject(
            fenceEvent_,
            INFINITE);
    }

    return true;
}

void D3D12Backend::WaitForGpu()
{
    if (!queue_ ||
        !fence_ ||
        !fenceEvent_)
        return;

    const std::uint64_t value =
        nextFenceValue_++;

    if (FAILED(
            queue_->Signal(
                fence_.Get(),
                value)))
        return;

    if (fence_->GetCompletedValue() < value) {
        if (SUCCEEDED(
                fence_->SetEventOnCompletion(
                    value,
                    fenceEvent_))) {
            WaitForSingleObject(
                fenceEvent_,
                INFINITE);
        }
    }
}

#endif

std::unique_ptr<IBackend> CreateD3D12Backend()
{
    return std::make_unique<D3D12Backend>();
}

} // namespace Hamun::RHI
