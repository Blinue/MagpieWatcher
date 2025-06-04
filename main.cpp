#include <windows.h>
#include <string>
#include <format>

static UINT WM_MAGPIE_SCALINGCHANGED = RegisterWindowMessage(L"MagpieScalingChanged");
static UINT TIMER_ID = 1;

double dpiScale = 1.0;
HFONT hUIFont = NULL;

HWND hwndScaling = NULL;
HWND hwndSrc = NULL;
bool isWindowed;
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
	isWindowed = (bool)GetProp(hwndScaling, L"Magpie.Windowed");

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
		if (wParam == 3) {
			// User has started resizing or moving the scaled window. A timer is
			// used to periodically update scaling information.
			// 用户开始调整缩放窗口大小或移动缩放窗口。使用定时器定期更新缩放信息。
			SetTimer(hWnd, TIMER_ID, 50, nullptr);
			return 0;
		} else {
			KillTimer(hWnd, TIMER_ID);
		}
		
		switch (wParam) {
		case 0:
			// Scaling has ended or the source window has lost focus
			// 缩放已结束或源窗口失去焦点
			if (lParam == 0) {
				// Scaling ended
				// 缩放结束
				UpdateHwndScaling(NULL);
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			
			// Remove top-most status
			// 取消置顶
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			break;
		case 1:
			// Scaling has started or the source window has regained focus
			// 缩放已开始或源窗口回到前台
			if (!hwndScaling) {
				// Scaling started
				// 缩放开始
				UpdateHwndScaling((HWND)lParam);
				InvalidateRect(hWnd, nullptr, TRUE);
			}

			// Once scaling begins, place our window above the scaled window
			// 缩放开始后将本窗口置于缩放窗口上面
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
			break;
		case 2:
			// The position or size of the scaled window has changed
			// 缩放窗口位置或大小改变
			UpdateHwndScaling(hwndScaling);
			InvalidateRect(hWnd, nullptr, TRUE);
			// Update the Z-order to ensure our window stays above the scaled window
			// 更新 Z 轴顺序确保本窗口在缩放窗口上面
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
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
			InvalidateRect(hWnd, nullptr, TRUE);
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
	case WM_GETMINMAXINFO:
	{
		MINMAXINFO* mmi = (MINMAXINFO*)lParam;
		mmi->ptMinTrackSize = { std::lround(400 * dpiScale), std::lround(300 * dpiScale) };
		return 0;
	}
	case WM_ERASEBKGND:
	{
		// No need to erase the background since WM_PAINT redraws the entire client area
		// 无需擦除背景，因为 WM_PAINT 绘制整个客户区
		return TRUE;
	}
	case WM_PAINT:
	{
		// Use double buffering to prevent flickering
		// 双缓冲绘图以防止闪烁
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		int clientWidth = clientRect.right - clientRect.left;
		int clientHeight = clientRect.bottom - clientRect.top;

		HDC hdcMem = CreateCompatibleDC(hdc);
		HBITMAP hbmMem = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
		HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
		HFONT hOldFont = (HFONT)SelectObject(hdcMem, hUIFont);

		FillRect(hdcMem, &clientRect, GetSysColorBrush(COLOR_WINDOW));

		std::wstring text = []() -> std::wstring {
			if (!hwndScaling) {
				return L"Not scaling";
			}

			std::wstring srcTitle(GetWindowTextLength(hwndSrc), 0);
			srcTitle.resize(GetWindowText(hwndSrc, srcTitle.data(), (int)srcTitle.size() + 1));

			double factor = double(destRect.right - destRect.left) / (srcRect.right - srcRect.left);
			factor = int(factor * 1000) / 1000.0;

			return std::format(L"{} scaling\n\nSource window: {}\nScale factor: {}",
				isWindowed ? L"Windowed" : L"Fullscreen", srcTitle, factor);
		}();

		const int padding = std::lround(10 * dpiScale);
		clientRect.left += padding;
		clientRect.top += padding;
		clientRect.right -= padding;
		clientRect.bottom -= padding;
		
		DrawText(hdcMem, text.c_str(), (int)text.size(), &clientRect, DT_TOP | DT_LEFT | DT_WORDBREAK);

		BitBlt(hdc, 0, 0, clientWidth, clientHeight, hdcMem, 0, 0, SRCCOPY);

		SelectObject(hdcMem, hOldFont);
		SelectObject(hdcMem, hbmOld);
		DeleteObject(hbmMem);
		DeleteDC(hdcMem);

		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == BN_CLICKED && hwndScaling) {
			RECT targetRect;
			GetWindowRect(hwndScaling, &targetRect);

			const LONG delta = lround((LOWORD(wParam) == 1 ? 25 : -25) * dpiScale);
			SetWindowPos(
				hwndScaling,
				NULL,
				targetRect.left - delta,
				targetRect.top - delta,
				targetRect.right - targetRect.left + 2 * delta,
				targetRect.bottom - targetRect.top + 2 * delta,
				SWP_NOZORDER | SWP_NOACTIVATE
			);
		}
		break;
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
		.lpszClassName = L"MagpieWatcher"
	};
	RegisterClassExW(&wcex);

	// Make our window top-most to prevent it from being covered by magpie scaling window
	// 创建置顶窗口以避免被缩放窗口遮挡
	HWND hWnd = CreateWindow(L"MagpieWatcher", L"MagpieWatcher",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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

	SetWindowPos(
		hWnd,
		hwndScaling && (GetForegroundWindow() == hwndSrc) ? HWND_TOPMOST : NULL,
		0, 0, std::lround(400 * dpiScale), std::lround(300 * dpiScale),
		SWP_NOMOVE | SWP_SHOWWINDOW
	);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
