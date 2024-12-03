// uix_test.cpp : Defines the entry point for the application.
//
#pragma comment(lib, "d2d1.lib")
#include "d2d1.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
extern "C" int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow);

// Global Variables:
HINSTANCE hInst;                                // current instance
bool should_quit = false;
static HANDLE quit_event = NULL,app_mutex=NULL,app_thread=NULL;
// directX stuff
static ID2D1HwndRenderTarget* render_target = nullptr;
static ID2D1Factory* d2d_factory = nullptr;
static ID2D1Bitmap* render_bitmap = nullptr;

static void setup() {

}
static void loop() {

}
// this handles our main application loop
// plus rendering
static DWORD render_thread_proc(void* state) {
    setup();
    bool quit = false;
    while (!quit) {
        loop();
        if (render_target && render_bitmap) {
            if (WAIT_OBJECT_0 == WaitForSingleObject(
                app_mutex,    // handle to mutex
                INFINITE)) {  // no time-out interval)

                render_target->BeginDraw();
                D2D1_RECT_F rect_dest = {
                    0,
                    0,
                    (float)320,
                    (float)240 };
                render_target->DrawBitmap(render_bitmap,
                    rect_dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
                render_target->EndDraw();
                ReleaseMutex(app_mutex);
                //InterlockedIncrement(&frames);
            }
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            quit = true;
        }
    }
    return 0;
}

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClasses(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WindowProcDX(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

   
    // Initialize global strings
    MyRegisterClasses(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClasses(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName  = L"UIX_TEST";
    wcex.hIconSm        = NULL;

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
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable
   // for signalling when to exit
   quit_event = CreateEvent(
       NULL,              // default security attributes
       TRUE,              // manual-reset event
       FALSE,             // initial state is nonsignaled
       TEXT("QuitEvent")  // object name
   );
   // for handling our render
   app_mutex = CreateMutex(NULL, FALSE, NULL);
   assert(app_mutex != NULL);
   app_thread = CreateThread(NULL, 8000 * 4, render_thread_proc, NULL, 0, NULL);
   assert(app_thread != NULL);

   RECT r = { 0, 0, 320-1, 240-1};
   // adjust the size of the window so
   // the above is our client rect
   AdjustWindowRectEx(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_APPWINDOW);
   HWND hWndMain = CreateWindowW(L"UIX_TEST", L"UIX test", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,CW_USEDEFAULT, CW_USEDEFAULT, r.right-r.left+1, r.bottom-r.top+1, nullptr, nullptr, hInstance, nullptr);

   HWND hwnd_dx = CreateWindowW(L"UIX_DX", L"",
       WS_CHILDWINDOW | WS_VISIBLE,
       0, 0, 320,240,
       hWndMain, NULL,
       hInstance, NULL);
   // start DirectX
   HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
   assert(hr == S_OK);
   // set up our direct2d surface
   {
       RECT rc;
       GetClientRect(hwnd_dx, &rc);
       D2D1_SIZE_U size = D2D1::SizeU(
           (rc.right - rc.left+1),
           rc.bottom - rc.top+1);

       hr = d2d_factory->CreateHwndRenderTarget(
           D2D1::RenderTargetProperties(),
           D2D1::HwndRenderTargetProperties(hwnd_dx, size),
           &render_target);
       assert(hr == S_OK);
       
   }
   // initialize the render bitmap
   {
       D2D1_SIZE_U size = { 0 };
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
       size.width = 320;
       size.height = 240;

       hr = render_target->CreateBitmap(size,
           props,
           &render_bitmap);
       assert(hr == S_OK);
      
   }
   ShowWindow(hWndMain, nCmdShow);
   UpdateWindow(hWndMain);

   return TRUE;
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
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        SetEvent(quit_event);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
LRESULT CALLBACK WindowProcDX(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
