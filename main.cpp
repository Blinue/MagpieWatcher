#include <windows.h>
#include <CommCtrl.h>
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

HWND hwndSizeUpBtn = NULL;
HWND hwndSizeDownBtn = NULL;

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

	SendMessage(hwndSizeUpBtn, WM_SETFONT, (WPARAM)hUIFont, TRUE);
	SendMessage(hwndSizeDownBtn, WM_SETFONT, (WPARAM)hUIFont, TRUE);
}

static void UpdateSizeButtons(HWND hwndParent) {
	RECT clientRect;
	GetClientRect(hwndParent, &clientRect);

	const int clientHeight = clientRect.bottom - clientRect.top;
	const int padding = std::lround(10 * dpiScale);
	const int btnWidth = std::lround(56 * dpiScale);
	const int btnHeight = std::lround(30 * dpiScale);
	SetWindowPos(hwndSizeUpBtn, NULL,
		padding, clientHeight - padding - btnHeight, btnWidth, btnHeight, SWP_NOACTIVATE);
	SetWindowPos(hwndSizeDownBtn, NULL,
		padding + btnWidth + std::lround(4 * dpiScale), clientHeight - padding - btnHeight, btnWidth, btnHeight, SWP_NOACTIVATE);

	EnableWindow(hwndSizeUpBtn, (BOOL)(INT_PTR)hwndScaling);
	EnableWindow(hwndSizeDownBtn, (BOOL)(INT_PTR)hwndScaling);
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
				UpdateSizeButtons(hWnd);
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			
			if (!isWindowed) {
				// Remove topmost status
				// 取消置顶
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
			}

			break;
		case 1:
			// Scaling has started or the source window has regained focus
			// 缩放已开始或源窗口回到前台
			if (!hwndScaling) {
				// Scaling started
				// 缩放开始
				UpdateHwndScaling((HWND)lParam);
				UpdateSizeButtons(hWnd);
				InvalidateRect(hWnd, nullptr, TRUE);
			}

			// Once scaling begins, place our window above the scaled window
			// 缩放开始后将本窗口置于缩放窗口上面
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
			if (isWindowed) {
				// Topmost is not required in windowed scaling. We briefly set our window
				// topmost and then revert it to ensure it's layered above the scaled window.
				// 窗口模式缩放时无需置顶。这里通过先置顶再取消置顶来将本窗口置于缩放窗口上面
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			}

			break;
		case 2:
			// The position or size of the scaled window has changed
			// 缩放窗口位置或大小改变
			UpdateHwndScaling(hwndScaling);
			InvalidateRect(hWnd, nullptr, TRUE);
			break;
		default:
			break;
		}
		return 0;
	}

	switch (message) {
	case WM_CREATE:
	{
		const HMODULE hInst = GetModuleHandle(nullptr);
		hwndSizeUpBtn = CreateWindow(L"BUTTON", L"Size+",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hWnd, (HMENU)1, hInst, 0);
		hwndSizeDownBtn = CreateWindow(L"BUTTON", L"Size-",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hWnd, (HMENU)2, hInst, 0);

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
	case WM_SIZE:
	{
		UpdateSizeButtons(hWnd);
		return 0;
	}
	case WM_ERASEBKGND:
	{
		// No need to erase the background since WM_PAINT redraws the entire client area
		// 无需擦除背景，因为 WM_PAINT 绘制整个客户区
		return TRUE;
	}
	case WM_CTLCOLORBTN:
	{
		// 使原生按钮控件边框外的背景透明
		return NULL;
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

			const LONG delta = lround((LOWORD(wParam) == 1 ? 50 : -50) * dpiScale);
			SetWindowPos(
				hwndScaling, NULL, 0, 0,
				targetRect.right - targetRect.left + delta,
				targetRect.bottom - targetRect.top + delta,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
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

	{
		INITCOMMONCONTROLSEX icce{
			.dwSize = sizeof(INITCOMMONCONTROLSEX),
			.dwICC = ICC_STANDARD_CLASSES
		};
		InitCommonControlsEx(&icce);
	}

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
	UpdateSizeButtons(hWnd);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		// 实现 tab 导航
		if (!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}
