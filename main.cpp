#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <format>

static UINT WM_MAGPIE_SCALINGCHANGED = RegisterWindowMessage(L"MagpieScalingChanged");

double dpiScale = 1.0;
HFONT hUIFont = NULL;

HWND hwndScaling = NULL;
HWND hwndSrc = NULL;
RECT srcRect;
RECT destRect;

static void UpdateHwndScaling(HWND hWnd) {
	hwndScaling = hWnd;

	if (!hWnd) {
		return;
	}

	hwndSrc = (HWND)GetProp(hwndScaling, L"Magpie.SrcHWND");

	srcRect.left = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.SrcLeft");
	srcRect.top = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.SrcTop");
	srcRect.right = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.SrcRight");
	srcRect.bottom = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.SrcBottom");

	destRect.left = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.DestLeft");
	destRect.top = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.DestTop");
	destRect.right = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.DestRight");
	destRect.bottom = (LONG)(INT_PTR)GetProp(hwndScaling, L"Magpie.DestBottom");
}

static void UpdateDpi(uint32_t dpi) {
	dpiScale = dpi / double(USER_DEFAULT_SCREEN_DPI);

	hUIFont = CreateFont(
		std::lround(20 * dpiScale),
		0,
		0,
		0,
		FW_NORMAL,
		FALSE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		L"Microsoft YaHei UI"
	);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_MAGPIE_SCALINGCHANGED) {
		switch (wParam) {
		case 0:
			// 缩放已停止
			UpdateHwndScaling(NULL);
			InvalidateRect(hWnd, nullptr, TRUE);
			break;
		case 1:
			// 缩放已开始
			UpdateHwndScaling((HWND)lParam);
			InvalidateRect(hWnd, nullptr, TRUE);

			// 缩放开始后将本窗口置于缩放窗口上面
			BringWindowToTop(hWnd);
			break;
		default:
			break;
		}
		return 0;
	}

	switch (message) {
	case WM_CREATE:
	{
		UpdateDpi(GetDpiForWindow(hWnd));
		break;
	}
	case WM_DPICHANGED:
	{
		UpdateDpi(HIWORD(wParam));

		RECT* newRect = (RECT*)lParam;
		SetWindowPos(hWnd,
			NULL,
			newRect->left,
			newRect->top,
			newRect->right - newRect->left,
			newRect->bottom - newRect->top,
			SWP_NOZORDER | SWP_NOACTIVATE
		);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		HGDIOBJ hOldFont = SelectObject(hdc, hUIFont);

		std::wstring text = []() -> std::wstring {
			if (!hwndScaling) {
				return L"未缩放";
			}

			std::wstring srcTitle(GetWindowTextLength(hwndSrc), 0);
			srcTitle.resize(GetWindowText(hwndSrc, srcTitle.data(), (int)srcTitle.size() + 1));

			double factor = double(destRect.right - destRect.left) / (srcRect.right - srcRect.left);
			factor = int(factor * 1000) / 1000.0;

			return std::format(L"缩放中\n\n源窗口: {}\n缩放比例: {}", srcTitle, factor);
		}();

		RECT clientRect;
		GetClientRect(hWnd, &clientRect);

		const int margin = std::lround(10 * dpiScale);
		clientRect.left += margin;
		clientRect.top += margin;
		clientRect.right -= margin;
		clientRect.bottom -= margin;
		
		DrawText(hdc, text.c_str(), (int)text.size(), &clientRect, DT_TOP | DT_LEFT | DT_WORDBREAK);

		SelectObject(hdc, hOldFont);

		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow
) {
	WNDCLASSEXW wcex{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
		.lpszClassName = L"MagpieWatcher"
	};
	RegisterClassExW(&wcex);

	HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, L"MagpieWatcher", L"MagpieWatcher", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) {
		return FALSE;
	}

	SetProp(hWnd, L"Magpie.ToolWindow", (HANDLE)TRUE);

	UpdateHwndScaling(FindWindow(L"Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22", nullptr));

	SetWindowPos(hWnd, NULL, 0, 0, std::lround(400 * dpiScale),
		std::lround(300 * dpiScale), SWP_NOMOVE | SWP_SHOWWINDOW);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
