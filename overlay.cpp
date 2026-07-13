#include "overlay.h"
#include <gdiplus.h>
#include <string>
#include <cstdio>
#include <iterator>

#pragma comment(lib, "gdiplus.lib")

namespace {

    const wchar_t* kClassName = L"cs2_external_overlay";
    constexpr float kDefaultFontSize = 13.f; // pixels

    ULONG_PTR   g_gdiToken   = 0;
    HWND        g_hwnd       = nullptr;
    HWND        g_gameHwnd   = nullptr;
    RECT        g_lastRect{};
    bool        g_shown      = false;

    HDC               g_memDC     = nullptr;
    HBITMAP           g_bitmap    = nullptr;
    HBITMAP           g_oldBitmap = nullptr;
    void*             g_bits      = nullptr;
    int               g_bmpW      = 0;
    int               g_bmpH      = 0;
    Gdiplus::Bitmap*  g_gdiBitmap = nullptr;
    Gdiplus::Graphics* g_gfx      = nullptr;

    Gdiplus::FontFamily* g_fontFamily = nullptr;

    Gdiplus::Color ToGdiColor(Draw::Color c) {
        return Gdiplus::Color(c.a, c.r, c.g, c.b);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    struct FindByPid { DWORD pid; HWND found; };

    BOOL CALLBACK EnumWindowsCb(HWND hwnd, LPARAM lParam) {
        auto* ctx = reinterpret_cast<FindByPid*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != ctx->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindowTextLengthW(hwnd) == 0) return TRUE;
        ctx->found = hwnd;
        return FALSE; // stop
    }

    HWND FindGameWindow(DWORD pid) {
        FindByPid ctx{ pid, nullptr };
        EnumWindows(EnumWindowsCb, reinterpret_cast<LPARAM>(&ctx));
        return ctx.found;
    }

    bool EnsureBackBuffer(int w, int h) {
        if (w <= 0 || h <= 0) return false;
        if (w == g_bmpW && h == g_bmpH && g_gdiBitmap) return true;

        if (g_gfx)       { delete g_gfx;       g_gfx = nullptr; }
        if (g_gdiBitmap) { delete g_gdiBitmap; g_gdiBitmap = nullptr; }
        if (g_memDC && g_oldBitmap) { SelectObject(g_memDC, g_oldBitmap); g_oldBitmap = nullptr; }
        if (g_bitmap)    { DeleteObject(g_bitmap); g_bitmap = nullptr; }
        if (!g_memDC) g_memDC = CreateCompatibleDC(nullptr);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = w;
        bmi.bmiHeader.biHeight      = -h; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        g_bitmap = CreateDIBSection(g_memDC, &bmi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
        if (!g_bitmap) return false;
        g_oldBitmap = static_cast<HBITMAP>(SelectObject(g_memDC, g_bitmap));

        // PARGB: GDI+ treats this bitmap as premultiplied-alpha, which is
        // exactly what UpdateLayeredWindow(ULW_ALPHA) expects from the DIB
        // it's handed — no manual premultiplication step needed.
        g_gdiBitmap = new Gdiplus::Bitmap(w, h, w * 4, PixelFormat32bppPARGB,
                                           static_cast<BYTE*>(g_bits));
        g_gfx = new Gdiplus::Graphics(g_gdiBitmap);
        g_gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g_gfx->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        g_bmpW = w;
        g_bmpH = h;
        return true;
    }

}

namespace Overlay {

    bool Init() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&g_gdiToken, &input, nullptr) != Gdiplus::Ok)
            return false;

        g_fontFamily = new Gdiplus::FontFamily(L"Segoe UI");

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        // WS_EX_TRANSPARENT: click-through, mouse events pass to whatever is
        // beneath. WS_EX_LAYERED: required for UpdateLayeredWindow per-pixel
        // alpha. WS_EX_NOACTIVATE + WS_EX_TOOLWINDOW: never steals focus or
        // shows in alt-tab/taskbar.
        g_hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kClassName, L"cs2_external overlay", WS_POPUP,
            0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);

        return g_hwnd != nullptr;
    }

    void Shutdown() {
        if (g_gfx)       { delete g_gfx;       g_gfx = nullptr; }
        if (g_gdiBitmap) { delete g_gdiBitmap; g_gdiBitmap = nullptr; }
        if (g_memDC && g_oldBitmap) { SelectObject(g_memDC, g_oldBitmap); g_oldBitmap = nullptr; }
        if (g_bitmap)    { DeleteObject(g_bitmap); g_bitmap = nullptr; }
        if (g_memDC)     { DeleteDC(g_memDC); g_memDC = nullptr; }
        if (g_fontFamily){ delete g_fontFamily; g_fontFamily = nullptr; }

        if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
        UnregisterClassW(kClassName, GetModuleHandleW(nullptr));

        if (g_gdiToken) { Gdiplus::GdiplusShutdown(g_gdiToken); g_gdiToken = 0; }
    }

    bool Update(DWORD gamePid) {
        if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
            HWND prev = g_gameHwnd;
            g_gameHwnd = FindGameWindow(gamePid);
            if (g_gameHwnd && g_gameHwnd != prev) {
                wchar_t title[128]{};
                GetWindowTextW(g_gameHwnd, title, static_cast<int>(std::size(title)));
                std::printf("[overlay] found game window hwnd=%p title=\"%ls\"\n",
                            static_cast<void*>(g_gameHwnd), title);
            } else if (!g_gameHwnd) {
                static int s_missLogged = 0;
                if ((s_missLogged++ % 200) == 0)
                    std::printf("[overlay] no visible top-level window owned by pid %lu yet\n", gamePid);
            }
        }
        if (!g_gameHwnd || !IsWindowVisible(g_gameHwnd) || IsIconic(g_gameHwnd)) {
            if (g_shown) { ShowWindow(g_hwnd, SW_HIDE); g_shown = false; std::printf("[overlay] hiding (game window gone/minimized)\n"); }
            return false;
        }

        RECT rect{};
        if (!GetClientRect(g_gameHwnd, &rect)) return false;
        POINT topLeft{ rect.left, rect.top };
        ClientToScreen(g_gameHwnd, &topLeft);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;
        if (w <= 0 || h <= 0) return false;

        bool moved = (topLeft.x != g_lastRect.left || topLeft.y != g_lastRect.top ||
                      w != (g_lastRect.right - g_lastRect.left) ||
                      h != (g_lastRect.bottom - g_lastRect.top));

        if (moved) {
            if (!EnsureBackBuffer(w, h)) { std::printf("[overlay] EnsureBackBuffer(%d,%d) failed\n", w, h); return false; }
            SetWindowPos(g_hwnd, HWND_TOPMOST, topLeft.x, topLeft.y, w, h, SWP_NOACTIVATE);
            g_lastRect = { topLeft.x, topLeft.y, topLeft.x + w, topLeft.y + h };
            std::printf("[overlay] positioned at (%d,%d) size %dx%d\n", topLeft.x, topLeft.y, w, h);
        }

        if (!g_shown) { ShowWindow(g_hwnd, SW_SHOWNOACTIVATE); g_shown = true; std::printf("[overlay] window shown\n"); }
        return true;
    }

    void PumpMessages() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    bool BeginFrame() {
        if (!g_shown || !g_gfx) return false;
        // Clear() is a direct fill (bypasses compositing mode), so this
        // reliably zeroes alpha every pixel regardless of what was drawn last frame.
        g_gfx->Clear(Gdiplus::Color(0, 0, 0, 0));
        return true;
    }

    void EndFrame() {
        if (!g_shown || !g_gfx) return;

        POINT ptSrc{ 0, 0 };
        SIZE size{ g_bmpW, g_bmpH };
        POINT ptDst{ g_lastRect.left, g_lastRect.top };
        BLENDFUNCTION blend{};
        blend.BlendOp             = AC_SRC_OVER;
        blend.BlendFlags          = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat         = AC_SRC_ALPHA;

        UpdateLayeredWindow(g_hwnd, nullptr, &ptDst, &size, g_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    }

    void SetInteractive(bool interactive) {
        static int s_state = -1; // -1 unknown, 0 click-through, 1 interactive
        int want = interactive ? 1 : 0;
        if (want == s_state || !g_hwnd) return;
        s_state = want;

        LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        if (interactive) ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        else             ex |=  static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);

        // Steal foreground when opening the menu so the game releases mouse
        // capture and the OS cursor becomes usable over our panel.
        if (interactive) SetForegroundWindow(g_hwnd);
        else if (g_gameHwnd) SetForegroundWindow(g_gameHwnd);
    }

    POINT Origin() {
        return { g_lastRect.left, g_lastRect.top };
    }

}

namespace Draw {

    Size ScreenSize() {
        return { static_cast<float>(g_bmpW), static_cast<float>(g_bmpH) };
    }

    void AddRect(float x0, float y0, float x1, float y1, Color color, float thickness) {
        if (!g_gfx) return;
        Gdiplus::Pen pen(ToGdiColor(color), thickness);
        g_gfx->DrawRectangle(&pen, x0, y0, x1 - x0, y1 - y0);
    }

    void AddRectFilled(float x0, float y0, float x1, float y1, Color color) {
        if (!g_gfx) return;
        Gdiplus::SolidBrush brush(ToGdiColor(color));
        g_gfx->FillRectangle(&brush, x0, y0, x1 - x0, y1 - y0);
    }

    void AddRectFilledGradientV(float x0, float y0, float x1, float y1, Color top, Color bottom) {
        if (!g_gfx || x1 <= x0 || y1 <= y0) return;
        Gdiplus::RectF rect(x0, y0, x1 - x0, y1 - y0);
        Gdiplus::LinearGradientBrush brush(rect, ToGdiColor(top), ToGdiColor(bottom),
                                            Gdiplus::LinearGradientModeVertical);
        g_gfx->FillRectangle(&brush, rect);
    }

    void AddLine(float x0, float y0, float x1, float y1, Color color, float thickness) {
        if (!g_gfx) return;
        Gdiplus::Pen pen(ToGdiColor(color), thickness);
        g_gfx->DrawLine(&pen, x0, y0, x1, y1);
    }

    void AddText(float x, float y, float fontSize, Color color, const char* text) {
        if (!g_gfx || !text || !g_fontFamily) return;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (wlen <= 0) return;
        std::wstring wtext(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext.data(), wlen);

        Gdiplus::Font font(g_fontFamily, fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(ToGdiColor(color));
        g_gfx->DrawString(wtext.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
    }

    void AddText(float x, float y, Color color, const char* text) {
        AddText(x, y, kDefaultFontSize, color, text);
    }

    Size CalcTextSize(float fontSize, const char* text) {
        if (!g_gfx || !text || !g_fontFamily) return { 0.f, 0.f };
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (wlen <= 0) return { 0.f, 0.f };
        std::wstring wtext(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext.data(), wlen);

        Gdiplus::Font font(g_fontFamily, fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::RectF bounds;
        Gdiplus::PointF origin(0.f, 0.f);
        g_gfx->MeasureString(wtext.c_str(), -1, &font, origin, &bounds);
        return { bounds.Width, bounds.Height };
    }

    Size CalcTextSize(const char* text) {
        return CalcTextSize(kDefaultFontSize, text);
    }

}
