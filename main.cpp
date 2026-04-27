#include "imgui.h"
#include "imgui_custom_widgets.hpp"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "vm_wrapper.hpp"
#include <d3d11.h>
#include <shobjidl_core.h>
#include <tchar.h>
#define STB_IMAGE_IMPLEMENTATION
#include "resource.h"
#include "stb_image.h"
#include <cmath>
#include <cstring>
#include <shellapi.h>
#include <unordered_map>
#include <chrono>
#include <thread>


static ID3D11Device *g_pd3dDevice = nullptr;

static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;

static IDXGISwapChain *g_pSwapChain = nullptr;

static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

static HWND g_hwnd = nullptr;

bool CreateDeviceD3D(HWND);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

struct ImageTexture
{
    ID3D11ShaderResourceView *view = nullptr;
    int width = 0, height = 0;
};

bool LoadTextureFromFile(const char *fn, ImageTexture *o)
{
    int w = 0, h = 0;
    unsigned char *d = stbi_load(fn, &w, &h, NULL, 4);
    if (!d)
        return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;

    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D *pT = NULL;
    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = d;
    sr.SysMemPitch = w * 4;
    sr.SysMemSlicePitch = 0;

    g_pd3dDevice->CreateTexture2D(&desc, &sr, &pT);

    D3D11_SHADER_RESOURCE_VIEW_DESC sv;
    ZeroMemory(&sv, sizeof(sv));
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = 1;

    g_pd3dDevice->CreateShaderResourceView(pT, &sv, &o->view);
    pT->Release();
    o->width = w;
    o->height = h;
    stbi_image_free(d);
    return true;
}

VMWrapper g_VM;
bool isStartup = false;
int intellipan_mode[3] = {1, 1, 1};

std::unordered_map<std::string, bool> g_Locks;
std::unordered_map<std::string, float> g_LocalCache;

float GetVal(const std::string &n)
{
    static std::unordered_map<std::string, double> last_poll_times;
    if (!g_Locks[n])
    {
        double current_time = ImGui::GetTime();
        if (current_time - last_poll_times[n] >= 0.033)
        {
            g_LocalCache[n] = g_VM.GetParameter(n);
            last_poll_times[n] = current_time;
        }
    }
    return g_LocalCache[n];
}
void SetVal(const std::string &n, float v, bool a)
{
    g_LocalCache[n] = v;
    g_VM.SetParameter(n, v);
    g_Locks[n] = a;
}
static int GetLevelCh(const char *p)
{
    if (strcmp(p, "Strip[0]") == 0)
        return 0;
    if (strcmp(p, "Strip[3]") == 0)
        return 6;
    return 8;
}

// Draw 8 buttons with exact spacing to fill faderH=260
static void DrawBtnCol(const char *vm, bool has_mc, bool is_input)
{
    std::string pre(vm);

    // Positions: 8 btns*20=160, gap budget=100, g=10 lg=20
    // A1@0 A2@30 A3@60 | B1@100 B2@130 | mono@170 | solo@210 Mute@240
    float y0 = ImGui::GetCursorPosY();

    float pos[] = {0, 30, 60, 100, 130, 170, 210, 240};

    const char *rt[] = {"A1", "A2", "A3", "B1", "B2"};

    for (int i = 0; i < 5; i++)
    {
        ImGui::SetCursorPosY(y0 + pos[i]);
        bool r = GetVal(pre + "." + rt[i]) > .5f;
        if (UI::ToggleButton(rt[i], &r, true))
            SetVal(pre + "." + rt[i], r ? 1 : 0, false);
    }
    ImGui::SetCursorPosY(y0 + pos[5]);

    if (is_input)
    {
        bool mono = GetVal(pre + ".Mono") > .5f;
        if (UI::ToggleButton("mono", &mono, false))
            SetVal(pre + ".Mono", mono ? 1 : 0, false);
    }
    else if (has_mc)
    {
        bool mc = GetVal(pre + ".MC") > .5f;
        if (UI::ToggleButton("M.C", &mc, false))
            SetVal(pre + ".MC", mc ? 1 : 0, false);
    }
    else
    {
        float kv = GetVal(pre + ".K");
        if (UI::KButton("K", &kv))
            SetVal(pre + ".K", kv, false);
    }
    ImGui::SetCursorPosY(y0 + pos[6]);
    bool solo = GetVal(pre + ".Solo") > .5f;
    if (UI::ToggleButton("solo", &solo, false))
        SetVal(pre + ".Solo", solo ? 1 : 0, false);

    ImGui::SetCursorPosY(y0 + pos[7]);
    bool mute = GetVal(pre + ".Mute") > .5f;
    if (UI::ToggleButton("Mute", &mute, false))
        SetVal(pre + ".Mute", mute ? 1 : 0, false);

    ImGui::SetCursorPosY(y0 + 260);
}

void DrawInputStrip(const char *vm)
{
    std::string pre(vm);

    float cx = GetVal(pre + ".Color_x"), cy2 = GetVal(pre + ".Color_y");

    float fx_x = GetVal(pre + ".fx_x"), fx_y = GetVal(pre + ".fx_y");

    float px = GetVal(pre + ".Pan_x"), py = GetVal(pre + ".Pan_y");

    float *tx = &cx, *ty = &cy2;

    if (intellipan_mode[0] == 1)
    {
        tx = &fx_x;
        ty = &fx_y;
    }
    if (intellipan_mode[0] == 2)
    {
        tx = &px;
        ty = &py;
    }
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 130) * .5f);

    if (UI::Intellipan("IP##0", tx, ty, &intellipan_mode[0]))
    {
        if (intellipan_mode[0] == 0)
        {
            SetVal(pre + ".Color_x", *tx, ImGui::IsItemActive());
            SetVal(pre + ".Color_y", *ty, ImGui::IsItemActive());
        }
        else if (intellipan_mode[0] == 1)
        {
            SetVal(pre + ".fx_x", *tx, ImGui::IsItemActive());
            SetVal(pre + ".fx_y", *ty, ImGui::IsItemActive());
        }
        else
        {
            SetVal(pre + ".Pan_x", *tx, ImGui::IsItemActive());
            SetVal(pre + ".Pan_y", *ty, ImGui::IsItemActive());
        }
    }
    else
    {
        g_Locks[pre + ".Color_x"] = g_Locks[pre + ".Color_y"] = g_Locks[pre + ".Pan_x"] = g_Locks[pre + ".Pan_y"] =
            ImGui::IsItemActive();
    }
    ImGui::Dummy(ImVec2(0, 10));

    float comp = GetVal(pre + ".Comp"), gate = GetVal(pre + ".Gate");

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) * .5f);

    if (UI::CircularKnob("Comp", &comp, 0, 10))
        SetVal(pre + ".Comp", comp, ImGui::IsItemActive());
    else
        g_Locks[pre + ".Comp"] = ImGui::IsItemActive();

    ImGui::SameLine(0, 12);

    if (UI::CircularKnob("Gate", &gate, 0, 10))
        SetVal(pre + ".Gate", gate, ImGui::IsItemActive());
    else
        g_Locks[pre + ".Gate"] = ImGui::IsItemActive();

    // Aligned fader group at bottom
    ImGui::SetCursorPosY(300);

    float grpW = 12 + 3 + 50 + 6 + 48;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - grpW) * .5f);

    ImGui::BeginGroup();
    int lch = GetLevelCh(vm);
    float lL = g_VM.GetLevel(1, lch), lR = g_VM.GetLevel(1, lch + 1);
    UI::PeakMeter("Pk##0", (lL > lR) ? lL : lR, 260);
    ImGui::EndGroup();

    ImGui::SameLine(0, 3);
    ImGui::BeginGroup();
    float gain = GetVal(pre + ".Gain");

    if (UI::CustomGainSlider("G##0", &gain, -60, 12, "MIKE"))
        SetVal(pre + ".Gain", gain, ImGui::IsItemActive());
    else
        g_Locks[pre + ".Gain"] = ImGui::IsItemActive();

    ImGui::EndGroup();
    ImGui::SameLine(0, 6);
    ImGui::BeginGroup();
    DrawBtnCol(vm, false, true);
    ImGui::EndGroup();
}

void DrawVirtualStrip(int idx, const char *name, const char *vm, bool has_mc)
{
    std::string pre(vm);

    if (g_HeaderFont)
        ImGui::PushFont(g_HeaderFont);

    else if (g_BoldFont)
        ImGui::PushFont(g_BoldFont);

    ImVec2 es = ImGui::CalcTextSize("EQUALIZER");
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - es.x) * .5f);
    ImGui::Text("EQUALIZER");

    if (g_HeaderFont || g_BoldFont)
        ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 10));

    float treble = GetVal(pre + ".EQGain3"), bass = GetVal(pre + ".EQGain1");

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) * .5f);

    if (UI::CircularKnob("Treble", &treble, -12, 12))
        SetVal(pre + ".EQGain3", treble, ImGui::IsItemActive());
    else
        g_Locks[pre + ".EQGain3"] = ImGui::IsItemActive();

    ImGui::SameLine(0, 12);

    if (UI::CircularKnob("Bass", &bass, -12, 12))
        SetVal(pre + ".EQGain1", bass, ImGui::IsItemActive());
    else
        g_Locks[pre + ".EQGain1"] = ImGui::IsItemActive();

    ImGui::Dummy(ImVec2(0, 10));

    float panx = GetVal(pre + ".Pan_x"), pany = GetVal(pre + ".Pan_y");

    float panW = 120 + 3 + 12;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - panW) * .5f);

    if (UI::DrawPanBox("Pan", &panx, &pany))
    {
        SetVal(pre + ".Pan_x", panx, ImGui::IsItemActive());
        SetVal(pre + ".Pan_y", pany, ImGui::IsItemActive());
    }
    else
    {
        g_Locks[pre + ".Pan_x"] = g_Locks[pre + ".Pan_y"] = ImGui::IsItemActive();
    }
    ImGui::SameLine(0, 3);
    int lch = GetLevelCh(vm);
    float lL = g_VM.GetLevel(1, lch), lR = g_VM.GetLevel(1, lch + 1);

    UI::PeakMeter("Pk", (lL > lR) ? lL : lR, 120);

    // Aligned fader group at bottom (matching InputStrip)
    ImGui::SetCursorPosY(300);

    float grpW = 50 + 6 + 48;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - grpW) * .5f);

    ImGui::BeginGroup();
    float gain = GetVal(pre + ".Gain");

    if (UI::CustomGainSlider("G", &gain, -60, 12, name))
        SetVal(pre + ".Gain", gain, ImGui::IsItemActive());
    else
        g_Locks[pre + ".Gain"] = ImGui::IsItemActive();

    ImGui::EndGroup();
    ImGui::SameLine(0, 6);
    ImGui::BeginGroup();
    DrawBtnCol(vm, has_mc, false);
    ImGui::EndGroup();
}

void DrawStrip(int idx, const char *name, const char *vm, bool virt, bool has_mc = false)
{
    ImGui::PushID(idx);
    ImGui::BeginChild(name, ImVec2(200, 570), true, ImGuiWindowFlags_NoScrollbar);

    if (g_HeaderFont)
        ImGui::PushFont(g_HeaderFont);

    else if (g_BoldFont)
        ImGui::PushFont(g_BoldFont);

    ImVec2 ts = ImGui::CalcTextSize(name);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ts.x) * .5f);

    ImGui::TextColored(ImVec4(.9f, .9f, .9f, 1), "%s", name);

    if (g_HeaderFont || g_BoldFont)
        ImGui::PopFont();

    ImGui::Separator();
    ImGui::Spacing();

    if (!virt)
        DrawInputStrip(vm);
    else
        DrawVirtualStrip(idx, name, vm, has_mc);

    ImGui::EndChild();
    ImGui::PopID();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)1 << (sysInfo.dwNumberOfProcessors - 1));

    SetCurrentProcessExplicitAppUserModelID(L"turky.whothefuckismike.1");

    WNDCLASSEX wc = {sizeof(WNDCLASSEX),     CS_CLASSDC, WndProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                     _T("WhoTheFuckIsMike"), NULL};

    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    ::RegisterClassEx(&wc);

    // Window Width 950 to fit Logo in center
    g_hwnd = ::CreateWindowEx(0,

                              wc.lpszClassName, _T("Who The Fuck Is Mike"),
                              WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU, 100, 100, 950,
                              614, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(g_hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15);

    g_BoldFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 15);

    g_HeaderFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 24);

    if (!g_BoldFont)
        g_BoldFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 15);

    if (!g_HeaderFont)
        g_HeaderFont = g_BoldFont;

    UI::ApplyBrutalistTheme();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_VM.Initialize();

    ImageTexture logoTex;
    bool hasLogo = LoadTextureFromFile("logo.png", &logoTex);

    HKEY hKey;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS)
    {
        char val[MAX_PATH];
        DWORD sz = sizeof(val);

        if (RegQueryValueExA(hKey, "WhoTheFuckIsMike", NULL, NULL, (LPBYTE)val, &sz) == ERROR_SUCCESS)
            isStartup = true;
        RegCloseKey(hKey);
    }
    bool done = false;

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("Main", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Header (Reduced height, no logo)
        ImGui::SetCursorPosY(4);

        ImGui::SameLine(io.DisplaySize.x - 185);

        if (ImGui::Button("Windows Mixer", ImVec2(105, 22)))
            system("start ms-settings:apps-volume");

        ImGui::SameLine(0, 6);
        if (ImGui::Button("_", ImVec2(20, 22)))
            ShowWindow(g_hwnd, SW_MINIMIZE);

        ImGui::SameLine(0, 2);
        if (ImGui::Button("[]", ImVec2(20, 22)))
        {
            WINDOWPLACEMENT wp;
            wp.length = sizeof(wp);
            GetWindowPlacement(g_hwnd, &wp);
            ShowWindow(g_hwnd, wp.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
        }
        ImGui::SameLine(0, 2);
        if (ImGui::Button("X", ImVec2(20, 22)))
            done = true;

        ImVec2 mp = ImGui::GetMousePos();
        if (mp.y < 30 && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0))
        {
            ReleaseCapture();
            SendMessage(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        ImGui::SetCursorPosY(30);
        ImGui::Separator();

        if (ImGui::BeginPopupContextWindow("BgMenu"))
        {
            if (ImGui::MenuItem("Run on Startup", nullptr, &isStartup))
            {
                if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                                  KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
                {
                    if (isStartup)
                    {
                        char p2[MAX_PATH];
                        GetModuleFileNameA(NULL, p2, MAX_PATH);
                        RegSetValueExA(hKey, "WhoTheFuckIsMike", 0, REG_SZ, (BYTE *)p2, (DWORD)strlen(p2) + 1);
                    }
                    else
                        RegDeleteValueA(hKey, "WhoTheFuckIsMike");
                    RegCloseKey(hKey);
                }
            }
            ImGui::EndPopup();
        }

        // Main Layout: [mike] [Large Logo] [stream] [mine]
        ImGui::SetCursorPosY(35);

        DrawStrip(0, "MIKE", "Strip[0]", false);

        ImGui::SameLine(0, 10);

        // Large Center Logo (Vertically Centered in Window)
        ImGui::BeginGroup();

        float cAreaW = io.DisplaySize.x - 610;

        if (hasLogo)
        {
            float lLogoW = cAreaW - 2;
            if (lLogoW > 450)
                lLogoW = 450;

            float lLogoH = lLogoW * ((float)logoTex.height / logoTex.width);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cAreaW - lLogoW) * .5f);

            ImGui::SetCursorPosY((io.DisplaySize.y - lLogoH) * .5f);

            ImGui::Image((void *)logoTex.view, ImVec2(lLogoW, lLogoH));
        }
        else
        {
            ImGui::Dummy(ImVec2(cAreaW, 570));
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, 10);

        ImGui::SetCursorPosY(35);

        DrawStrip(1, "STREAM", "Strip[3]", true, true);

        ImGui::SameLine(0, 4);

        DrawStrip(2, "MINE", "Strip[4]", true, false);

        ImGui::End();

        ImGui::Render();
        const float cc[4] = {.1f, .1f, .1f, 1};

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);

        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    g_VM.Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (hasLogo && logoTex.view)
    {
        logoTex.view->Release();
    }
    CleanupDeviceD3D();
    ::DestroyWindow(g_hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCCALCSIZE:
        if (wParam)
            return 0;
        break;

    case WM_NCHITTEST: {
        POINT pt;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ScreenToClient(hWnd, &pt);

        RECT rc;
        GetClientRect(hWnd, &rc);
        int b = 6;

        if (pt.y < b && pt.x < b)
            return HTTOPLEFT;
        if (pt.y < b && pt.x > rc.right - b)
            return HTTOPRIGHT;

        if (pt.y > rc.bottom - b && pt.x < b)
            return HTBOTTOMLEFT;
        if (pt.y > rc.bottom - b && pt.x > rc.right - b)
            return HTBOTTOMRIGHT;

        if (pt.y < b)
            return HTTOP;
        if (pt.y > rc.bottom - b)
            return HTBOTTOM;
        if (pt.x < b)
            return HTLEFT;
        if (pt.x > rc.right - b)
            return HTRIGHT;

        return HTCLIENT;
    }

    case WM_SIZE:

        if (g_pd3dDevice && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl, fla[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, fla, 2, D3D11_SDK_VERSION, &sd,
                                      &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = NULL;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
}
void CreateRenderTarget()
{
    ID3D11Texture2D *b;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&b));
    g_pd3dDevice->CreateRenderTargetView(b, NULL, &g_mainRenderTargetView);
    b->Release();
}
void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}
