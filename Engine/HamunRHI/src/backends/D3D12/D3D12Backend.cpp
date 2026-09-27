#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

constexpr UINT kFrameCount = 2;

struct Vertex {
    float position[3];
    float color[4];
};

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
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

D3D12_RESOURCE_DESC BufferDesc(UINT64 size)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

} // namespace

#endif

namespace Hamun::RHI {

class D3D12Backend final : public IBackend {
public:
    D3D12Backend();
    ~D3D12Backend() override;

    std::string_view Name() const noexcept override { return "Direct3D 12"; }
    BackendType Type() const noexcept override { return BackendType::D3D12; }
    const Capabilities& Caps() const noexcept override { return caps_; }

    bool Initialize(const BackendCreateInfo& createInfo) override;
    bool RenderFrame() override;
    void Shutdown() override;

private:
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    bool CreateDeviceAndQueue(bool enableValidation);
    bool CreateSwapChain(HWND hwnd, std::uint32_t width, std::uint32_t height);
    bool CreateFrameResources();
    bool CreatePipeline();
    bool CreateTriangleBuffer();
    bool WaitForFrame(std::uint32_t submittedFrame);
    void WaitForGpu();

    ComPtr<IDXGIFactory6> factory_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> renderTargets_;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> allocators_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Fence> fence_;

    D3D12_VERTEX_BUFFER_VIEW vertexView_{};
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

    if (!CreatePipeline())
        return false;

    if (!CreateTriangleBuffer())
        return false;

    initialized_ = true;
    Core::Log(Core::LogLevel::Info,
        "D3D12 initialized: device, queue, swap chain, RTVs, command list, fence and triangle PSO are ready.");
    return true;
#else
    (void)createInfo;
    Core::Log(Core::LogLevel::Warning,
        "D3D12 backend is unavailable in this build.");
    return false;
#endif
}

bool D3D12Backend::RenderFrame()
{
#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)
    if (!initialized_)
        return false;

    const std::uint32_t submittedFrame = frameIndex_;
    if (Failed(allocators_[submittedFrame]->Reset(), "CommandAllocator::Reset"))
        return false;

    if (Failed(commandList_->Reset(
            allocators_[submittedFrame].Get(), pipelineState_.Get()),
            "CommandList::Reset"))
        return false;

    const auto toRenderTarget = TransitionBarrier(
        renderTargets_[submittedFrame].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(submittedFrame) * rtvDescriptorSize_;

    constexpr float clearColor[4] = {0.025f, 0.045f, 0.075f, 1.0f};
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissor_);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexView_);
    commandList_->DrawInstanced(3, 1, 0, 0);

    const auto toPresent = TransitionBarrier(
        renderTargets_[submittedFrame].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    if (Failed(commandList_->Close(), "CommandList::Close"))
        return false;

    ID3D12CommandList* lists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    const HRESULT presentResult = swapChain_->Present(1, 0);
    if (Failed(presentResult, "SwapChain::Present"))
        return false;

    return WaitForFrame(submittedFrame);
#else
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

    vertexBuffer_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    commandList_.Reset();

    for (auto& allocator : allocators_)
        allocator.Reset();
    for (auto& target : renderTargets_)
        target.Reset();

    rtvHeap_.Reset();
    swapChain_.Reset();
    queue_.Reset();
    device_.Reset();
    adapter_.Reset();
    factory_.Reset();

    initialized_ = false;
#endif
}

#if defined(HAMUN_ENABLE_D3D12) && defined(_WIN32)

bool D3D12Backend::CreateDeviceAndQueue(bool enableValidation)
{
    UINT factoryFlags = 0;

    if (enableValidation) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            Core::Log(Core::LogLevel::Info,
                "D3D12 debug layer enabled.");
        }
    }

    if (Failed(CreateDXGIFactory2(
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

        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
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
        if (Failed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp)),
                "EnumWarpAdapter"))
            return false;

        if (Failed(warp.As(&adapter_), "Query WARP adapter"))
            return false;

        Core::Log(Core::LogLevel::Warning,
            "D3D12 hardware adapter unavailable; using WARP.");
    }

    if (Failed(D3D12CreateDevice(
            adapter_.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_)),
            "D3D12CreateDevice"))
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    return !Failed(device_->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
}

bool D3D12Backend::CreateSwapChain(
    HWND hwnd, std::uint32_t width, std::uint32_t height)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kFrameCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (Failed(factory_->CreateSwapChainForHwnd(
            queue_.Get(), hwnd, &desc, nullptr, nullptr, &swapChain1),
            "CreateSwapChainForHwnd"))
        return false;

    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (Failed(swapChain1.As(&swapChain_), "Query IDXGISwapChain3"))
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
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = kFrameCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (Failed(device_->CreateDescriptorHeap(
            &heapDesc, IID_PPV_ARGS(&rtvHeap_)),
            "Create RTV heap"))
        return false;

    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kFrameCount; ++i) {
        if (Failed(swapChain_->GetBuffer(
                i, IID_PPV_ARGS(&renderTargets_[i])),
                "Get swap-chain buffer"))
            return false;

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(), nullptr, handle);
        handle.ptr += rtvDescriptorSize_;

        if (Failed(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&allocators_[i])),
                "CreateCommandAllocator"))
            return false;
    }

    if (Failed(device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocators_[frameIndex_].Get(),
            nullptr,
            IID_PPV_ARGS(&commandList_)),
            "CreateCommandList"))
        return false;

    if (Failed(commandList_->Close(), "Initial CommandList::Close"))
        return false;

    if (Failed(device_->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
            "CreateFence"))
        return false;

    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        Core::Log(Core::LogLevel::Error,
            "D3D12: CreateEventW failed for GPU fence.");
        return false;
    }

    return true;
}

bool D3D12Backend::CreatePipeline()
{
    static constexpr const char* shaderSource = R"(
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

    UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    shaderFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompile(
        shaderSource,
        std::strlen(shaderSource),
        "HamunBootstrapTriangle",
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        shaderFlags,
        0,
        &vs,
        &errors);

    if (FAILED(hr)) {
        if (errors)
            Core::Log(Core::LogLevel::Error,
                static_cast<const char*>(errors->GetBufferPointer()));
        return false;
    }

    errors.Reset();
    hr = D3DCompile(
        shaderSource,
        std::strlen(shaderSource),
        "HamunBootstrapTriangle",
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        shaderFlags,
        0,
        &ps,
        &errors);

    if (FAILED(hr)) {
        if (errors)
            Core::Log(Core::LogLevel::Error,
                static_cast<const char*>(errors->GetBufferPointer()));
        return false;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 0;
    rootDesc.pParameters = nullptr;
    rootDesc.NumStaticSamplers = 0;
    rootDesc.pStaticSamplers = nullptr;
    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRoot;
    errors.Reset();
    hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRoot,
        &errors);

    if (FAILED(hr)) {
        if (errors)
            Core::Log(Core::LogLevel::Error,
                static_cast<const char*>(errors->GetBufferPointer()));
        return false;
    }

    if (Failed(device_->CreateRootSignature(
            0,
            serializedRoot->GetBufferPointer(),
            serializedRoot->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)),
            "CreateRootSignature"))
        return false;

    const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    auto& rt = blend.RenderTarget[0];
    rt.BlendEnable = FALSE;
    rt.LogicOpEnable = FALSE;
    rt.SrcBlend = D3D12_BLEND_ONE;
    rt.DestBlend = D3D12_BLEND_ZERO;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.LogicOp = D3D12_LOGIC_OP_NOOP;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC raster{};
    raster.FillMode = D3D12_FILL_MODE_SOLID;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    raster.FrontCounterClockwise = FALSE;
    raster.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    raster.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    raster.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    raster.DepthClipEnable = TRUE;
    raster.MultisampleEnable = FALSE;
    raster.AntialiasedLineEnable = FALSE;
    raster.ForcedSampleCount = 0;
    raster.ConservativeRaster =
        D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.BlendState = blend;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = raster;
    pso.DepthStencilState = depth;
    pso.InputLayout = {inputLayout, _countof(inputLayout)};
    pso.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pso.SampleDesc.Count = 1;
    pso.SampleDesc.Quality = 0;
    pso.NodeMask = 0;
    pso.CachedPSO = {};
    pso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    return !Failed(device_->CreateGraphicsPipelineState(
        &pso, IID_PPV_ARGS(&pipelineState_)),
        "CreateGraphicsPipelineState");
}

bool D3D12Backend::CreateTriangleBuffer()
{
    const Vertex vertices[] = {
        {{ 0.0f,  0.60f, 0.0f}, {0.15f, 0.75f, 1.00f, 1.0f}},
        {{ 0.60f, -0.55f, 0.0f}, {0.95f, 0.35f, 0.20f, 1.0f}},
        {{-0.60f, -0.55f, 0.0f}, {0.25f, 0.95f, 0.45f, 1.0f}},
    };

    const UINT64 bufferSize = sizeof(vertices);
    const D3D12_HEAP_PROPERTIES heapProps = UploadHeapProperties();
    const D3D12_RESOURCE_DESC resourceDesc = BufferDesc(bufferSize);

    if (Failed(device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer_)),
            "Create triangle vertex buffer"))
        return false;

    void* mapped = nullptr;
    const D3D12_RANGE readRange{0, 0};
    if (Failed(vertexBuffer_->Map(0, &readRange, &mapped),
            "Map triangle vertex buffer"))
        return false;

    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);

    vertexView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexView_.SizeInBytes = static_cast<UINT>(sizeof(vertices));
    vertexView_.StrideInBytes = sizeof(Vertex);
    return true;
}

bool D3D12Backend::WaitForFrame(std::uint32_t submittedFrame)
{
    const std::uint64_t fenceValue = nextFenceValue_++;
    if (Failed(queue_->Signal(fence_.Get(), fenceValue),
            "CommandQueue::Signal"))
        return false;

    frameFenceValues_[submittedFrame] = fenceValue;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

    const std::uint64_t waitValue = frameFenceValues_[frameIndex_];
    if (waitValue != 0 && fence_->GetCompletedValue() < waitValue) {
        if (Failed(fence_->SetEventOnCompletion(
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
        if (SUCCEEDED(fence_->SetEventOnCompletion(value, fenceEvent_)))
            WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

#endif

std::unique_ptr<IBackend> CreateD3D12Backend()
{
    return std::make_unique<D3D12Backend>();
}

} // namespace Hamun::RHI
