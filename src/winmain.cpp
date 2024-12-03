#include "app_main.h"
#include <stdint.h>
#include <stddef.h>
// uix_test.cpp : Defines the entry point for the application.
//
#pragma comment(lib, "d2d1.lib")
#include "d2d1.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
static LARGE_INTEGER start_time;
extern "C" int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                                 _In_opt_ HINSTANCE hPrevInstance,
                                 _In_ LPWSTR lpCmdLine, _In_ int nCmdShow);

// Global Variables:
static HINSTANCE hInst;  // current instance
static bool should_quit = false;
static HANDLE quit_event = NULL, app_mutex = NULL, app_thread = NULL;
// directX stuff
static HWND hwnd_dx=NULL;
static ID2D1HwndRenderTarget* render_target = nullptr;
static ID2D1Factory* d2d_factory = nullptr;
static ID2D1Bitmap* render_bitmap = nullptr;
// mouse mess
static struct {
    int x;
    int y;
} mouse_loc;
static int mouse_state = 0;  // 0 = released, 1 = pressed
static int old_mouse_state = 0;
static int mouse_req = 0;
// this handles our main application loop
// plus rendering
static DWORD CALLBACK render_thread_proc(void* state) {
    setup();
    bool quit = false;
    while (!quit) {
        loop();
        if (render_target && render_bitmap) {
            if (WAIT_OBJECT_0 ==
                WaitForSingleObject(app_mutex,    // handle to mutex
                                    INFINITE)) {  // no time-out interval)

                render_target->BeginDraw();
                D2D1_RECT_F rect_dest = {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
                render_target->DrawBitmap(
                    render_bitmap, rect_dest, 1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
                render_target->EndDraw();
                ReleaseMutex(app_mutex);
                // InterlockedIncrement(&frames);
            }
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            quit = true;
        }
    }
    return 0;
}

// Forward declarations of functions included in this code module:
ATOM MyRegisterClasses(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WindowProcDX(HWND hWnd, UINT uMsg, WPARAM wParam,
                              LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    CoInitialize(NULL);
    // Initialize global strings
    MyRegisterClasses(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }
    QueryPerformanceCounter(&start_time);
    MSG msg = {0};
    while (!should_quit) {
        DWORD result = 0;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                should_quit = true;
                break;
            }
            // handle our out of band messages
            if (msg.message == WM_LBUTTONDOWN && msg.hwnd == hwnd_dx) {
                if (LOWORD(msg.lParam) < SCREEN_WIDTH &&
                    HIWORD(msg.lParam) < SCREEN_HEIGHT) {
                    SetCapture(hwnd_dx);

                    if (WAIT_OBJECT_0 == WaitForSingleObject(
                                             app_mutex,    // handle to mutex
                                             INFINITE)) {  // no time-out interval)
                        old_mouse_state = mouse_state;
                        mouse_state = 1;
                        mouse_loc.x = LOWORD(msg.lParam);
                        mouse_loc.y = HIWORD(msg.lParam);
                        mouse_req = 1;
                        ReleaseMutex(app_mutex);
                    }
                }
            }
            if (msg.message == WM_MOUSEMOVE &&
                msg.hwnd == hwnd_dx) {
                if (WAIT_OBJECT_0 == WaitForSingleObject(
                                         app_mutex,    // handle to mutex
                                         INFINITE)) {  // no time-out interval)
                    if (mouse_state == 1 && MK_LBUTTON == msg.wParam) {
                        mouse_req = 1;
                        mouse_loc.x = (int16_t)LOWORD(msg.lParam);
                        mouse_loc.y = (int16_t)HIWORD(msg.lParam);
                    }
                    ReleaseMutex(app_mutex);
                }
            }
            if (msg.message == WM_LBUTTONUP &&
                msg.hwnd == hwnd_dx) {
                ReleaseCapture();
                if (WAIT_OBJECT_0 == WaitForSingleObject(
                                         app_mutex,    // handle to mutex
                                         INFINITE)) {  // no time-out interval)

                    old_mouse_state = mouse_state;
                    mouse_req = 1;
                    mouse_state = 0;
                    mouse_loc.x = (int16_t)LOWORD(msg.lParam);
                    mouse_loc.y = (int16_t)HIWORD(msg.lParam);
                    ReleaseMutex(app_mutex);
                }
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            should_quit = true;
        }
    }
    CoUninitialize();

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClasses(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"UIX_TEST";
    wcex.hIconSm = NULL;

    RegisterClassExW(&wcex);

    wcex.lpfnWndProc = WindowProcDX;
    wcex.lpszClassName = L"UIX_DX";
    wcex.hIcon = NULL;
    wcex.hIconSm = NULL;
    RegisterClassExW(&wcex);
    return TRUE;
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;  // Store instance handle in our global variable
    // for signalling when to exit
    quit_event = CreateEvent(NULL,              // default security attributes
                             TRUE,              // manual-reset event
                             FALSE,             // initial state is nonsignaled
                             TEXT("QuitEvent")  // object name
    );
    // for handling our render
    app_mutex = CreateMutex(NULL, FALSE, NULL);
    assert(app_mutex != NULL);

    RECT r = {0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1};
    // adjust the size of the window so
    // the above is our client rect
    AdjustWindowRectEx(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_APPWINDOW);
    HWND hWndMain = CreateWindowW(
        L"UIX_TEST", L"UIX test", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left ,
        r.bottom - r.top , nullptr, nullptr, hInstance, nullptr);

    hwnd_dx = CreateWindowW(L"UIX_DX", L"", WS_CHILDWINDOW | WS_VISIBLE, 0,
                                 0, SCREEN_WIDTH, SCREEN_HEIGHT, hWndMain, NULL, hInstance, NULL);
    // start DirectX
    HRESULT hr =
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
    assert(hr == S_OK);
    // set up our direct2d surface
    {
        RECT rc;
        GetClientRect(hwnd_dx, &rc);
        D2D1_SIZE_U size =
            D2D1::SizeU((rc.right - rc.left + 1), rc.bottom - rc.top + 1);

        hr = d2d_factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd_dx, size), &render_target);
        assert(hr == S_OK);
    }
    // initialize the render bitmap
    {
        D2D1_SIZE_U size = {0};
        D2D1_BITMAP_PROPERTIES props;
        render_target->GetDpi(&props.dpiX, &props.dpiY);
        D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
#ifdef USE_RGB
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
#else
            DXGI_FORMAT_B8G8R8A8_UNORM,
#endif
            D2D1_ALPHA_MODE_IGNORE);
        props.pixelFormat = pixelFormat;
        size.width = SCREEN_WIDTH;
        size.height = SCREEN_HEIGHT;

        hr = render_target->CreateBitmap(size, props, &render_bitmap);
        assert(hr == S_OK);
    }
    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);
    app_thread =
        CreateThread(NULL, 8000 * 4, render_thread_proc, NULL, 0, NULL);
    assert(app_thread != NULL);

    return TRUE;
}
void flush_bitmap(int x1, int y1, int w, int h, const void* bmp) {
    if (render_bitmap != NULL) {
        D2D1_RECT_U b;
        b.top = y1;
        b.left = x1;
        b.bottom = y1 + h;
        b.right = x1 + w;
#ifndef SCREEN_RGB565
        render_bitmap->CopyFromMemory(&b, bmp, w * 4);
#else
        uint32_t* bmp32=(uint32_t*)malloc(w*h*4);
        if(bmp32==NULL) return;
        uint32_t* pd = bmp32;
        const uint16_t* ps = (uint16_t*)bmp;
        
        size_t pix_left = (size_t)(w*h);
        while(pix_left-->0) {
            uint16_t bs = (((*ps)>>8)&0xFF)|(((*ps++)&0xFF)<<8);
            uint8_t r = (bs>>0)&31;
            uint8_t g = (bs>>5)&63;
            uint8_t b = (bs>>11)&31;
            uint32_t px = ((r*255)/31)<<0;
            px |= (((g*255)/63)<<8);
            px |= (((b*255)/31)<<16);
            *(pd++)=px;
        }
        render_bitmap->CopyFromMemory(&b,bmp32 , w * 4);
        free(bmp32);
#endif   
    }
}
bool read_mouse(int* out_x, int* out_y) {
    if (WAIT_OBJECT_0 == WaitForSingleObject(
                             app_mutex,    // handle to mutex
                             INFINITE)) {  // no time-out interval)

        if (mouse_state) {
            *out_x = mouse_loc.x;
            *out_y = mouse_loc.y;
        }
        mouse_req = 0;
        ReleaseMutex(app_mutex);
        return mouse_state;
    }
    return false;
}

uint32_t millis() {
    LARGE_INTEGER counter_freq;
    LARGE_INTEGER end_time;
    QueryPerformanceFrequency(&counter_freq);
    QueryPerformanceCounter(&end_time);
    return uint32_t(((double)(end_time.QuadPart - start_time.QuadPart) / counter_freq.QuadPart) * 1000);
}
uint32_t micros() {
    LARGE_INTEGER counter_freq;
    LARGE_INTEGER end_time;
    QueryPerformanceFrequency(&counter_freq);
    QueryPerformanceCounter(&end_time);
    return uint32_t(((double)(end_time.QuadPart - start_time.QuadPart) / counter_freq.QuadPart) * 1000000.0);
}
void delay(uint32_t ms) {
    uint32_t end = ms + millis();
    while (millis() < end)
        ;
}
void delayMicroseconds(uint32_t us) {
    uint32_t end = us + micros();
    while (micros() < end)
        ;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        } break;
        case WM_DESTROY:
            SetEvent(quit_event);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
LRESULT CALLBACK WindowProcDX(HWND hWnd, UINT uMsg, WPARAM wParam,
                              LPARAM lParam) {
    // shouldn't get this, but handle anyway
    if (uMsg == WM_SIZE) {
        if (render_target) {
            D2D1_SIZE_U size = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));
            render_target->Resize(size);
        }
    }
    // in case we receive the close event
    if (uMsg == WM_CLOSE) {
        SetEvent(quit_event);
        should_quit = true;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
