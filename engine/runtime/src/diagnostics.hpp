#pragma once
// Diagnostic overlay for Hamun's native Windows Vulkan runtime.
// Metrics are measurements or clearly labelled "n/a", never guessed.
#include <windows.h>
#include <psapi.h>
#include <vulkan/vulkan.h>
#include <chrono>
#include <cwchar>
#include <string>
#include <sstream>
#include <iomanip>

class HamunDiagnostics {
    HWND label_{};
    HFONT font_{};
    std::wstring gpu_, driver_;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point previous_=started_;
    unsigned frames_=0;
    double avgFrameMs_=0, lastCpuFrameMs_=0, lastGpuWaitMs_=0;
    uint64_t ramBytes_=0;
    ULONGLONG prevIdle_=0, prevKernel_=0, prevUser_=0;
    bool hasCpuSample_=false;
    double cpuPercent_=-1;
    static ULONGLONG ticks(FILETIME f) {
        ULARGE_INTEGER n{}; n.LowPart=f.dwLowDateTime; n.HighPart=f.dwHighDateTime; return n.QuadPart;
    }
    void sampleCpu(){
        FILETIME idle{},kernel{},user{};
        if(!GetSystemTimes(&idle,&kernel,&user)) return;
        auto i=ticks(idle),k=ticks(kernel),u=ticks(user);
        if(hasCpuSample_){
            auto total=(k-prevKernel_)+(u-prevUser_);
            auto idleDelta=i-prevIdle_;
            if(total>0 && idleDelta<=total)
                cpuPercent_=100.0*double(total-idleDelta)/double(total);
        }
        prevIdle_=i;prevKernel_=k;prevUser_=u;hasCpuSample_=true;
    }
public:
    void create(HWND parent, VkPhysicalDevice gpu){
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(gpu,&properties);
        int needed=MultiByteToWideChar(CP_UTF8,0,properties.deviceName,-1,nullptr,0);
        if(needed>0){gpu_.resize(size_t(needed));MultiByteToWideChar(CP_UTF8,0,properties.deviceName,-1,gpu_.data(),needed);gpu_.resize(size_t(needed-1));}
        else gpu_=L"Unknown Vulkan GPU";
        std::wostringstream api;
        api<<VK_VERSION_MAJOR(properties.apiVersion)<<L"."<<VK_VERSION_MINOR(properties.apiVersion)<<L"."<<VK_VERSION_PATCH(properties.apiVersion);
        driver_=api.str();
        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(gpu,&memory);
        for(uint32_t i=0;i<memory.memoryHeapCount;++i)
            if(memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ramBytes_+=memory.memoryHeaps[i].size;
        label_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"Initializing diagnostics...",
            WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,
            12,12,322,224,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
        font_=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
              CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FF_DONTCARE,L"Consolas");
        if(label_&&font_)SendMessageW(label_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
        previous_=started_=std::chrono::steady_clock::now();
    }
    void frame(double cpuFrameMs,double waitMs){
        lastCpuFrameMs_=cpuFrameMs;lastGpuWaitMs_=waitMs;
        ++frames_;
        auto now=std::chrono::steady_clock::now();
        double seconds=std::chrono::duration<double>(now-previous_).count();
        if(seconds<0.75||!label_)return;
        avgFrameMs_=(1000.0*seconds)/double(frames_);
        double fps=frames_/seconds;
        frames_=0;previous_=now;
        sampleCpu();
        MEMORYSTATUSEX mem{};mem.dwLength=sizeof(mem);GlobalMemoryStatusEx(&mem);
        PROCESS_MEMORY_COUNTERS counters{};counters.cb=sizeof(counters);
        GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters));
        auto gib=[](unsigned long long n){return double(n)/(1024.0*1024.0*1024.0);};
        std::wostringstream out;out<<std::fixed<<std::setprecision(1);
        out<<L"HAMUN  |  RUNTIME DIAGNOSTICS\r\n"
           <<L"Render API: Vulkan  "<<driver_<<L"\r\n"
           <<L"GPU: "<<gpu_<<L"\r\n"
           <<L"FPS: "<<fps<<L"  |  Frame: "<<avgFrameMs_<<L" ms\r\n"
           <<L"CPU frame: "<<lastCpuFrameMs_<<L" ms\r\n"
           <<L"GPU/Present wait*: "<<lastGpuWaitMs_<<L" ms\r\n"
           <<L"System CPU: ";
        if(cpuPercent_>=0)out<<cpuPercent_<<L"%";else out<<L"sampling";
        out<<L"\r\nProcess RAM: "<<gib(counters.WorkingSetSize)<<L" GiB"
           <<L"\r\nSystem RAM used: "<<gib(mem.ullTotalPhys-mem.ullAvailPhys)
           <<L" / "<<gib(mem.ullTotalPhys)<<L" GiB"
           <<L"\r\nGPU local memory total: "<<gib(ramBytes_)<<L" GiB"
           <<L"\r\nGPU utilization: n/a (not measured)"
           <<L"\r\n*CPU-side wait, NOT GPU latency.";
        SetWindowTextW(label_,out.str().c_str());
    }
    void shutdown(){
        if(label_ && IsWindow(label_))DestroyWindow(label_);
        label_=nullptr;
        if(font_)DeleteObject(font_);
        font_=nullptr;
    }
};
