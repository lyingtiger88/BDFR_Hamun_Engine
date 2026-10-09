#include <windowsx.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

namespace Hamun::Editor {
namespace {
struct Node { int id; int kind; int x; int y; };
struct Edge { int source; int target; };
struct GraphWindow {
    std::vector<Node> nodes{{1,0,80,110},{2,1,360,110},{3,2,640,110}};
    std::vector<Edge> edges{{1,2},{2,3}};
    int nextId=4, dragId=0, dx=0,dy=0, pending=0;
    std::filesystem::path file;
    HWND hwnd=nullptr;
};
GraphWindow* state=nullptr;
constexpr int cardW=174,cardH=104;
constexpr COLORREF background=RGB(22,25,31),panel=RGB(38,43,53),accent=RGB(51,158,211);
const wchar_t* labels[]={L"EVENT BEGIN PLAY",L"PRINT STRING",L"RETURN",L"BRANCH",L"ADD FLOAT"};
Node* find(GraphWindow& g,int id) {
    for(auto& n:g.nodes) if(n.id==id) return &n;
    return nullptr;
}
POINT input(const Node& n){return {n.x,n.y+73};}
POINT output(const Node& n){return {n.x+cardW,n.y+73};}
bool save(GraphWindow& g) {
    if(g.file.empty())return false;
    std::ofstream f(g.file,std::ios::trunc);
    if(!f)return false;
    f<<"hamunblueprint=1\n";
    for(auto& n:g.nodes)f<<"node "<<n.id<<" "<<n.kind<<" "<<n.x<<" "<<n.y<<"\n";
    for(auto& e:g.edges)f<<"edge "<<e.source<<" "<<e.target<<"\n";
    return f.good();
}
bool load(GraphWindow& g,const std::filesystem::path& path) {
    std::ifstream f(path);
    std::string header;
    if(!std::getline(f,header)||header!="hamunblueprint=1")return false;
    std::vector<Node> nodes;std::vector<Edge> edges;
    std::string line;
    while(std::getline(f,line)){
        std::istringstream ss(line);std::string type;ss>>type;
        if(type=="node"){Node n{};if(!(ss>>n.id>>n.kind>>n.x>>n.y)||n.kind<0||n.kind>4)return false;nodes.push_back(n);}
        else if(type=="edge"){Edge e{};if(!(ss>>e.source>>e.target))return false;edges.push_back(e);}
        else if(!line.empty())return false;
    }
    if(nodes.size()>1000||edges.size()>4000)return false;
    for(std::size_t i=0;i<nodes.size();++i)
        for(std::size_t j=i+1;j<nodes.size();++j)if(nodes[i].id==nodes[j].id)return false;
    for(auto& e:edges) {
        bool source=false,target=false;
        for(auto& n:nodes){source|=n.id==e.source;target|=n.id==e.target;}
        if(!source||!target||e.source==e.target)return false;
    }
    g.nodes=std::move(nodes);g.edges=std::move(edges);g.nextId=1;
    for(auto& n:g.nodes)g.nextId=std::max(g.nextId,n.id+1);
    g.file=path;return true;
}
void draw(HDC dc,GraphWindow& g,RECT bounds) {
    HBRUSH bg=CreateSolidBrush(background);FillRect(dc,&bounds,bg);DeleteObject(bg);
    HPEN grid=CreatePen(PS_SOLID,1,RGB(32,37,46));
    HPEN old=(HPEN)SelectObject(dc,grid);
    for(int x=0;x<bounds.right;x+=32){MoveToEx(dc,x,0,nullptr);LineTo(dc,x,bounds.bottom);}
    for(int y=0;y<bounds.bottom;y+=32){MoveToEx(dc,0,y,nullptr);LineTo(dc,bounds.right,y);}
    SelectObject(dc,old);DeleteObject(grid);
    HPEN wire=CreatePen(PS_SOLID,3,accent);old=(HPEN)SelectObject(dc,wire);
    for(auto& e:g.edges){
        Node* a=find(g,e.source);Node* b=find(g,e.target);
        if(!a||!b)continue;
        POINT p=output(*a),q=input(*b);int middle=(p.x+q.x)/2;
        POINT path[]={{p.x,p.y},{middle,p.y},{middle,q.y},{q.x,q.y}};
        Polyline(dc,path,4);
    }
    SelectObject(dc,old);DeleteObject(wire);
    SetBkMode(dc,TRANSPARENT);
    for(auto& n:g.nodes){
        RECT card{n.x,n.y,n.x+cardW,n.y+cardH};
        HBRUSH fill=CreateSolidBrush(panel);FillRect(dc,&card,fill);DeleteObject(fill);
        HBRUSH bar=CreateSolidBrush(n.kind==0?RGB(115,65,165):RGB(35,103,146));
        RECT header{n.x,n.y,n.x+cardW,n.y+34};FillRect(dc,&header,bar);DeleteObject(bar);
        SetTextColor(dc,RGB(242,245,249));RECT label{n.x+10,n.y+9,n.x+cardW-5,n.y+32};
        DrawTextW(dc,labels[n.kind],-1,&label,DT_LEFT|DT_SINGLELINE);
        SetTextColor(dc,RGB(173,186,201));RECT subtitle{n.x+16,n.y+47,n.x+cardW-8,n.y+67};
        DrawTextW(dc,L"Execution", -1,&subtitle,DT_LEFT|DT_SINGLELINE);
        HBRUSH pin=CreateSolidBrush(accent);
        POINT a=input(n),b=output(n);
        RECT left{a.x-5,a.y-5,a.x+5,a.y+5},right{b.x-5,b.y-5,b.x+5,b.y+5};
        FillRect(dc,&left,pin);FillRect(dc,&right,pin);DeleteObject(pin);
    }
    SetTextColor(dc,RGB(175,189,204));
    RECT help{12,8,bounds.right-12,45};
    DrawTextW(dc,L"1 Event   2 Print   3 Return   4 Branch   5 Add   |   Drag nodes   |   Click output then input to connect   |   Ctrl+S save   Ctrl+O open", -1,&help,DT_WORDBREAK);
}
bool near(POINT a,int x,int y){return std::abs(a.x-x)<13&&std::abs(a.y-y)<13;}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    GraphWindow* g=reinterpret_cast<GraphWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){
        auto* init=reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(init->lpCreateParams));
        return TRUE;
    }
    if(!g)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);draw(dc,*g,r);EndPaint(hwnd,&ps);return 0;}
    case WM_LBUTTONDOWN:{
        int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        for(auto it=g->nodes.rbegin();it!=g->nodes.rend();++it){
            if(near(output(*it),x,y)){g->pending=it->id;InvalidateRect(hwnd,nullptr,FALSE);return 0;}
            if(near(input(*it),x,y)&&g->pending){
                if(g->pending!=it->id){
                    g->edges.erase(std::remove_if(g->edges.begin(),g->edges.end(),[&](const Edge& e){return e.target==it->id;}),g->edges.end());
                    g->edges.push_back({g->pending,it->id});
                }
                g->pending=0;InvalidateRect(hwnd,nullptr,FALSE);return 0;
            }
            if(x>=it->x&&x<it->x+cardW&&y>=it->y&&y<it->y+cardH){
                g->dragId=it->id;g->dx=x-it->x;g->dy=y-it->y;SetCapture(hwnd);return 0;
            }
        }return 0;
    }
    case WM_MOUSEMOVE:
        if(g->dragId&&(wp&MK_LBUTTON)){
            if(Node* n=find(*g,g->dragId)){n->x=std::max(0,GET_X_LPARAM(lp)-g->dx);n->y=std::max(48,GET_Y_LPARAM(lp)-g->dy);}
            InvalidateRect(hwnd,nullptr,FALSE);
        }return 0;
    case WM_LBUTTONUP:g->dragId=0;if(GetCapture()==hwnd)ReleaseCapture();return 0;
    case WM_KEYDOWN:{
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if(ctrl&&(wp=='S'||wp=='O')){
            wchar_t filename[MAX_PATH]=L"";
            if(!g->file.empty())wcsncpy_s(filename,g->file.c_str(),_TRUNCATE);
            OPENFILENAMEW dlg{};dlg.lStructSize=sizeof(dlg);dlg.hwndOwner=hwnd;
            dlg.lpstrFilter=L"Hamun Blueprint (*.hamunblueprint)\0*.hamunblueprint\0All files\0*.*\0";
            dlg.lpstrFile=filename;dlg.nMaxFile=MAX_PATH;dlg.lpstrDefExt=L"hamunblueprint";
            if(wp=='S'){
                if(g->file.empty()&&!GetSaveFileNameW(&dlg))return 0;
                if(g->file.empty())g->file=filename;
                if(!save(*g))MessageBoxW(hwnd,L"Could not save blueprint.",L"Hamun",MB_ICONERROR);
            }else if(GetOpenFileNameW(&dlg)){
                if(!load(*g,filename))MessageBoxW(hwnd,L"Invalid blueprint graph.",L"Hamun",MB_ICONERROR);
            }
            InvalidateRect(hwnd,nullptr,FALSE);return 0;
        }
        if(wp>='1'&&wp<='5'){
            RECT r;GetClientRect(hwnd,&r);
            g->nodes.push_back({g->nextId++,int(wp-'1'),std::max(40,(r.right-cardW)/2),std::max(60,(r.bottom-cardH)/2)});
            InvalidateRect(hwnd,nullptr,FALSE);return 0;
        }
        return 0;
    }
    case WM_DESTROY:delete g;return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
void OpenBlueprintEditor(HWND owner){
    static bool registered=false;
    if(!registered){
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"HamunBlueprintGraphEditor";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassW(&wc))return;
        registered=true;
    }
    auto* g=new GraphWindow;
    HWND hwnd=CreateWindowExW(0,L"HamunBlueprintGraphEditor",
        L"Hamun Blueprint | Event Graph",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,1100,730,owner,nullptr,GetModuleHandleW(nullptr),g);
    if(!hwnd){delete g;return;}
    g->hwnd=hwnd;ShowWindow(hwnd,SW_SHOW);
}
} // namespace Hamun::Editor
