#pragma once
// Diagnostic overlay for Hamun's native Windows Vulkan runtime.
// Metrics are measurements or clearly labelled "n/a", never guessed.
#include <windows.h>
#include <psapi.h>
#include <vulkan/vulkan.h>
#include <chrono>
#include <algorithm>
#include <cwchar>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

class HamunDiagnostics {
    HWND label_{};
    HWND parent_{};
    static constexpr int PanelWidth=460, PanelHeight=290;
    HFONT font_{};
    std::wstring gpu_, driver_, presentMode_=L"FIFO";
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
    void setPresentMode(const std::wstring& name){presentMode_=name;}
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
        parent_=parent;
        // An owned top-level layered popup is composed by DWM, not painted into
        // the Vulkan swapchain. A child EDIT control flickers on Vulkan surfaces.
        label_=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
            L"STATIC",L"",WS_POPUP,0,0,PanelWidth,PanelHeight,
            parent,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!label_) throw std::runtime_error("Cannot create diagnostics overlay");
        font_=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FF_DONTCARE,L"Consolas");
        ShowWindow(label_,SW_SHOWNOACTIVATE);
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
           <<L"Present: "<<presentMode_<<L"\r\n"
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
        // Render readable white text onto a dark, translucent layered popup.
        POINT origin{12,12};
        ClientToScreen(parent_,&origin);
        HDC screen=GetDC(nullptr);
        HDC memory=CreateCompatibleDC(screen);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=PanelWidth;
        bmi.bmiHeader.biHeight=-PanelHeight;
        bmi.bmiHeader.biPlanes=1;
        bmi.bmiHeader.biBitCount=32;
        bmi.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;
        HBITMAP bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(bitmap){
            HGDIOBJ old=SelectObject(memory,bitmap);
            RECT area{0,0,PanelWidth,PanelHeight};
            // Draw on black, then convert brightness to per-pixel alpha.
            // Fully black pixels become transparent; only glyph pixels remain.
            PatBlt(memory,0,0,PanelWidth,PanelHeight,BLACKNESS);
            SetBkMode(memory,TRANSPARENT);
            SetTextColor(memory,RGB(255,255,255));
            HGDIOBJ priorFont=font_?SelectObject(memory,font_):nullptr;
            RECT body{14,12,PanelWidth-10,PanelHeight-10};
            const auto lines=out.str();
            DrawTextW(memory,lines.c_str(),-1,&body,DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);
            if(priorFont)SelectObject(memory,priorFont);
            // GDI does not populate the DIB alpha channel. Build premultiplied
            // BGRA using the glyph luminance for proper per-pixel transparency.
            auto* rgba=static_cast<unsigned char*>(pixels);
            for(int i=0;i<PanelWidth*PanelHeight;++i){
                unsigned char* pixel=rgba+4*i;
                const unsigned char alpha=std::max(pixel[0],std::max(pixel[1],pixel[2]));
                pixel[0]=alpha;pixel[1]=alpha;pixel[2]=alpha;pixel[3]=alpha;
            }
            SIZE size{PanelWidth,PanelHeight};
            POINT src{0,0};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            UpdateLayeredWindow(label_,screen,&origin,&size,memory,&src,0,&blend,ULW_ALPHA);
            SelectObject(memory,old);
            DeleteObject(bitmap);
        }
        DeleteDC(memory);
        ReleaseDC(nullptr,screen);
    }
    void shutdown(){
        if(label_ && IsWindow(label_))DestroyWindow(label_);
        label_=nullptr;
        if(font_)DeleteObject(font_);
        font_=nullptr;
    }
};
