#include <windows.h>
#include <string>
#include <format>

static UINT WM_MAGPIE_SCALINGCHANGED = RegisterWindowMessage(L"MagpieScalingChanged");
static UINT TIMER_ID = 1;

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

	// Retrieve scaling information from window properties
	// 从窗口属性中检索缩放信息

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

	if (hUIFont) {
		DeleteObject(hUIFont);
	}

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
			KillTimer(hWnd, TIMER_ID);

			if (lParam == 0) {
				// Scaling ended
				// 缩放已结束
				UpdateHwndScaling(NULL);
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			break;
		case 1:
		case 2:
			// Scaling started
			// 缩放已开始
			if (wParam == 2) {
				KillTimer(hWnd, TIMER_ID);
			}

			UpdateHwndScaling(wParam == 1 ? (HWND)lParam : hwndScaling);
			InvalidateRect(hWnd, nullptr, FALSE);

			// Once scaling begins, place our window above magpie scaling window
			// 缩放开始后将本窗口置于缩放窗口上面
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			break;
		case 3:
			// 用户开始调整缩放窗口大小或移动缩放窗口，在此期间没有新事件。
			// 使用定时器定期更新缩放信息。
			SetTimer(hWnd, TIMER_ID, 50, nullptr);
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
	case WM_TIMER:
	{
		if (wParam == TIMER_ID) {
			UpdateHwndScaling(hwndScaling);
			InvalidateRect(hWnd, nullptr, FALSE);
		}
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
				return L"Not scaling";
			}

			std::wstring srcTitle(GetWindowTextLength(hwndSrc), 0);
			srcTitle.resize(GetWindowText(hwndSrc, srcTitle.data(), (int)srcTitle.size() + 1));

			double factor = double(destRect.right - destRect.left) / (srcRect.right - srcRect.left);
			factor = int(factor * 1000) / 1000.0;

			return std::format(L"Scaling\n\nSource window: {}\nScale factor: {}", srcTitle, factor);
		}();

		RECT clientRect;
		GetClientRect(hWnd, &clientRect);

		const int padding = std::lround(10 * dpiScale);
		clientRect.left += padding;
		clientRect.top += padding;
		clientRect.right -= padding;
		clientRect.bottom -= padding;
		
		DrawText(hdc, text.c_str(), (int)text.size(), &clientRect, DT_TOP | DT_LEFT | DT_WORDBREAK);

		SelectObject(hdc, hOldFont);

		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_DESTROY:
	{
		if (hUIFont) {
			DeleteObject(hUIFont);
		}

		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPWSTR /*lpCmdLine*/,
	_In_ int /*nCmdShow*/
) {
	// Ensure the MagpieScalingChanged message won't be filtered by UIPI
	// 使 MagpieScalingChanged 消息不会被 UIPI 过滤
	ChangeWindowMessageFilter(WM_MAGPIE_SCALINGCHANGED, MSGFLT_ADD);

	WNDCLASSEXW wcex{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WndProc,
		.hInstance = hInstance,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
		.lpszClassName = L"MagpieWatcher"
	};
	RegisterClassExW(&wcex);

	// Make our window top-most to prevent it from being covered by magpie scaling window
	// 创建置顶窗口以避免被缩放窗口遮挡
	HWND hWnd = CreateWindow(L"MagpieWatcher", L"MagpieWatcher", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) {
		return FALSE;
	}

	// Use class name to retrieve the handle of magpie scaling window.
	// Make sure to check the visibility of the scaling window.
	// 使用窗口类名检索缩放窗口句柄，注意应检查缩放窗口是否可见
	{
		HWND hwndFound = FindWindow(L"Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22", nullptr);
		if (IsWindowVisible(hwndFound)) {
			UpdateHwndScaling(hwndFound);
		}
	}

	SetWindowPos(hWnd, hwndScaling ? HWND_TOPMOST : NULL, 0, 0, std::lround(400 * dpiScale),
		std::lround(300 * dpiScale), SWP_NOMOVE | SWP_SHOWWINDOW);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
