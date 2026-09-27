#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

constexpr UINT kFrameCount = 2;

bool Failed(HRESULT hr, const char* operation)
{
    if (SUCCEEDED(hr))
        return false;

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Error,
        std::string("D3D12: ") + operation + " failed.");
    return true;
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

D3D12_RESOURCE_DESC DepthResourceDesc(UINT width, UINT height)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
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

UINT64 AlignConstantBufferSize(UINT64 size)
{
    return (size + 255ull) & ~255ull;
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

private:
    ComPtr<ID3D12Resource> resource_;
    std::uint64_t size_ = 0;
    BufferUsage usage_ = BufferUsage::Vertex;
};

class D3D12Shader final : public IShader {
public:
    D3D12Shader(ShaderStage stage, ComPtr<ID3DBlob> bytecode)
        : stage_(stage)
        , bytecode_(std::move(bytecode))
    {
    }

    ShaderStage Stage() const noexcept override { return stage_; }
    ID3DBlob* Bytecode() const noexcept { return bytecode_.Get(); }

private:
    ShaderStage stage_;
    ComPtr<ID3DBlob> bytecode_;
};

class D3D12Pipeline final : public IPipeline {
public:
    D3D12Pipeline(
        ComPtr<ID3D12RootSignature> rootSignature,
        ComPtr<ID3D12PipelineState> pipelineState)
        : rootSignature_(std::move(rootSignature))
        , pipelineState_(std::move(pipelineState))
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

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
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
    void Prepare(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* renderTarget,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor)
    {
        commandList_ = commandList;
        renderTarget_ = renderTarget;
        rtv_ = rtv;
        dsv_ = dsv;
        viewport_ = viewport;
        scissor_ = scissor;
        renderPassOpen_ = false;
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

    void SetPipeline(IPipeline& pipeline) override
    {
        auto* native = dynamic_cast<D3D12Pipeline*>(&pipeline);
        if (!native || !commandList_)
            return;

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
        if (!native || !commandList_)
            return;

        commandList_->SetGraphicsRootConstantBufferView(
            slot,
            native->Native()->GetGPUVirtualAddress());
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
        if (!commandList_ || !renderTarget_ || !renderPassOpen_)
            return;

        const auto barrier = TransitionBarrier(
            renderTarget_,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        commandList_->ResourceBarrier(1, &barrier);
        renderPassOpen_ = false;
    }

private:
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    ID3D12Resource* renderTarget_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_{};
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

    bool Initialize(const BackendCreateInfo& createInfo) override;
    void Shutdown() override;

    std::unique_ptr<IBuffer> CreateBuffer(
        const BufferDesc& desc) override;
    std::unique_ptr<IShader> CreateShader(
        const ShaderDesc& desc) override;
    std::unique_ptr<IPipeline> CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc) override;

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
    bool WaitForFrame(std::uint32_t submittedFrame);
    void WaitForGpu();

    ComPtr<IDXGIFactory6> factory_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
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
    bool initialized_ = false;
#endif

    Capabilities caps_{true, true, true, true, false, false};
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
        Core::Log(Core::LogLevel::Error,
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

    swapChainView_.Bind(
        swapChain_.Get(), createInfo.width, createInfo.height);
    fenceView_.Bind(fence_.Get());

    initialized_ = true;
    Core::Log(Core::LogLevel::Info,
        "D3D12 RHI initialized with depth, indexed drawing and constant buffers.");
    return true;
#else
    (void)createInfo;
    Core::Log(Core::LogLevel::Warning,
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
    dsvHeap_.Reset();
    fence_.Reset();
    rtvHeap_.Reset();
    swapChain_.Reset();
    queue_.Reset();
    device_.Reset();
    adapter_.Reset();
    factory_.Reset();

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

    const auto heapProps = UploadHeapProperties();
    const auto resourceDesc = BufferResourceDesc(allocationSize);

    ComPtr<ID3D12Resource> resource;
    if (Failed(device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
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

std::unique_ptr<IShader> D3D12Backend::CreateShader(
    const ShaderDesc& desc)
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_ || desc.source.empty() || desc.entryPoint.empty())
        return {};

    const char* target =
        desc.stage == ShaderStage::Vertex ? "vs_5_1" : "ps_5_1";

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> bytecode;
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
        &bytecode,
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

    return std::make_unique<D3D12Shader>(
        desc.stage, std::move(bytecode));
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

    if (!initialized_ || !vs || !ps ||
        vs->Stage() != ShaderStage::Vertex ||
        ps->Stage() != ShaderStage::Pixel ||
        desc.vertexStride == 0)
        return {};

    std::vector<D3D12_ROOT_PARAMETER> rootParameters(
        desc.constantBufferCount);

    for (std::uint32_t i = 0; i < desc.constantBufferCount; ++i) {
        rootParameters[i].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[i].Descriptor.ShaderRegister = i;
        rootParameters[i].Descriptor.RegisterSpace = 0;
        rootParameters[i].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters =
        static_cast<UINT>(rootParameters.size());
    rootDesc.pParameters =
        rootParameters.empty() ? nullptr : rootParameters.data();
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
    if (Failed(device_->CreateRootSignature(
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
    raster.CullMode = D3D12_CULL_MODE_BACK;
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
    pso.VS = {
        vs->Bytecode()->GetBufferPointer(),
        vs->Bytecode()->GetBufferSize()};
    pso.PS = {
        ps->Bytecode()->GetBufferPointer(),
        ps->Bytecode()->GetBufferSize()};
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
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat =
        desc.depthTest ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
    pso.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipelineState;
    if (Failed(device_->CreateGraphicsPipelineState(
            &pso, IID_PPV_ARGS(&pipelineState)),
            "CreateGraphicsPipelineState"))
        return {};

    return std::make_unique<D3D12Pipeline>(
        std::move(rootSignature),
        std::move(pipelineState));
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
                allocators_[frameIndex_].Get(), nullptr),
            "CommandList::Reset"))
        return nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr +=
        static_cast<SIZE_T>(frameIndex_) * rtvDescriptorSize_;

    const D3D12_CPU_DESCRIPTOR_HANDLE dsv =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandListView_.Prepare(
        commandList_.Get(),
        renderTargets_[frameIndex_].Get(),
        rtv,
        dsv,
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

    if (Failed(commandList_->Close(), "CommandList::Close"))
        return false;

    ID3D12CommandList* lists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    if (Failed(swapChain_->Present(1, 0), "SwapChain::Present"))
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
                D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            Core::Log(
                Core::LogLevel::Info,
                "D3D12 debug layer enabled.");
        }
    }

    if (Failed(
            CreateDXGIFactory2(
                factoryFlags, IID_PPV_ARGS(&factory_)),
            "CreateDXGIFactory2"))
        return false;

    for (UINT index = 0; ; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory_->EnumAdapterByGpuPreference(
                index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC1 adapterDesc{};
        candidate->GetDesc1(&adapterDesc);
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
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
                factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp)),
                "EnumWarpAdapter"))
            return false;

        if (Failed(warp.As(&adapter_), "Query WARP adapter"))
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

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    return !Failed(
        device_->CreateCommandQueue(
            &queueDesc, IID_PPV_ARGS(&queue_)),
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
        hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (Failed(
            swapChain1.As(&swapChain_),
            "Query IDXGISwapChain3"))
        return false;

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

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
    rtvHeapDesc.NumDescriptors = kFrameCount;

    if (Failed(
            device_->CreateDescriptorHeap(
                &rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)),
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
                    i, IID_PPV_ARGS(&renderTargets_[i])),
                "Get swap-chain buffer"))
            return false;

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(), nullptr, rtv);
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
    dsvHeapDesc.NumDescriptors = 1;

    if (Failed(
            device_->CreateDescriptorHeap(
                &dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_)),
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
    depthClear.DepthStencil.Stencil = 0;

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

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

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
        CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        Core::Log(
            Core::LogLevel::Error,
            "D3D12: CreateEventW failed.");
        return false;
    }

    return true;
}

bool D3D12Backend::WaitForFrame(std::uint32_t submittedFrame)
{
    const std::uint64_t fenceValue = nextFenceValue_++;

    if (Failed(
            queue_->Signal(fence_.Get(), fenceValue),
            "CommandQueue::Signal"))
        return false;

    frameFenceValues_[submittedFrame] = fenceValue;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

    const std::uint64_t waitValue =
        frameFenceValues_[frameIndex_];

    if (waitValue != 0 &&
        fence_->GetCompletedValue() < waitValue) {
        if (Failed(
                fence_->SetEventOnCompletion(
                    waitValue, fenceEvent_),
                "Fence::SetEventOnCompletion"))
            return false;

        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    return true;
}

void D3D12Backend::WaitForGpu()
{
    if (!queue_ || !fence_ || !fenceEvent_)
        return;

    const std::uint64_t value = nextFenceValue_++;

    if (FAILED(queue_->Signal(fence_.Get(), value)))
        return;

    if (fence_->GetCompletedValue() < value) {
        if (SUCCEEDED(
                fence_->SetEventOnCompletion(
                    value, fenceEvent_))) {
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }
}

#endif

std::unique_ptr<IBackend> CreateD3D12Backend()
{
    return std::make_unique<D3D12Backend>();
}

} // namespace Hamun::RHI
