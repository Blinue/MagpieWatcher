#include "MainWindow.h"
#include <string>
#include <format>

static const UINT WM_MAGPIE_SCALINGCHANGED = RegisterWindowMessage(L"MagpieScalingChanged");
static constexpr UINT TIMER_ID = 1;

bool MainWindow::Create(HINSTANCE hInst) noexcept {
	WNDCLASSEXW wcex{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = _WndProc,
		.hInstance = hInst,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.lpszClassName = L"MagpieWatcher"
	};
	if (!RegisterClassEx(&wcex)) {
		return false;
	}

	CreateWindow(L"MagpieWatcher", L"MagpieWatcher",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, hInst, this);
	if (!Handle()) {
		return FALSE;
	}

	SetWindowPos(
		Handle(),
		_hwndScaling && (GetForegroundWindow() == _hwndSrc) ? HWND_TOPMOST : NULL,
		0, 0, std::lround(400 * _dpiScale), std::lround(300 * _dpiScale),
		SWP_NOMOVE | SWP_SHOWWINDOW
	);
	_UpdateSizeButtons();

	return true;
}

LRESULT MainWindow::_MessageHandler(UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	if (msg == WM_MAGPIE_SCALINGCHANGED) {
		_HandleScalingChangedMessage(wParam, lParam);
		return 0;
	}

	switch (msg) {
	case WM_CREATE:
	{
		// Ensure the MagpieScalingChanged message won't be filtered by UIPI
		// 使 MagpieScalingChanged 消息不会被 UIPI 过滤
		ChangeWindowMessageFilterEx(Handle(), WM_MAGPIE_SCALINGCHANGED, MSGFLT_ADD, nullptr);

		const HMODULE hInst = GetModuleHandle(nullptr);
		_hwndSizeUpBtn = CreateWindow(L"BUTTON", L"Size+",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, Handle(), (HMENU)1, hInst, 0);
		_hwndSizeDownBtn = CreateWindow(L"BUTTON", L"Size-",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, Handle(), (HMENU)2, hInst, 0);

		_UpdateDpi(GetDpiForWindow(Handle()));

		// Use class name to retrieve the handle of magpie scaling window.
		// Make sure to check the visibility of the scaling window.
		// 使用窗口类名检索缩放窗口句柄，注意应检查缩放窗口是否可见
		HWND hwndFound = FindWindow(L"Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22", nullptr);
		if (IsWindowVisible(hwndFound)) {
			_hwndScaling = hwndFound;
			_UpdateScalingInfo();
		}
		
		break;
	}
	case WM_TIMER:
	{
		if (wParam == TIMER_ID) {
			_UpdateScalingInfo();
			InvalidateRect(Handle(), nullptr, TRUE);
		}
		return 0;
	}
	case WM_DPICHANGED:
	{
		_UpdateDpi(HIWORD(wParam));

		RECT* newRect = (RECT*)lParam;
		SetWindowPos(
			Handle(),
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
		mmi->ptMinTrackSize = { std::lround(400 * _dpiScale), std::lround(300 * _dpiScale) };
		return 0;
	}
	case WM_SIZE:
	{
		_UpdateSizeButtons();
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
		HDC hdc = BeginPaint(Handle(), &ps);

		RECT clientRect;
		GetClientRect(Handle(), &clientRect);
		int clientWidth = clientRect.right - clientRect.left;
		int clientHeight = clientRect.bottom - clientRect.top;

		HDC hdcMem = CreateCompatibleDC(hdc);
		HBITMAP hbmMem = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
		HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
		HFONT hOldFont = (HFONT)SelectObject(hdcMem, _hUIFont);

		FillRect(hdcMem, &clientRect, GetSysColorBrush(COLOR_WINDOW));

		std::wstring text = [this]() -> std::wstring {
			if (!_hwndScaling) {
				return L"Not scaling";
			}

			std::wstring srcTitle(GetWindowTextLength(_hwndSrc), 0);
			srcTitle.resize(GetWindowText(_hwndSrc, srcTitle.data(), (int)srcTitle.size() + 1));

			double factor = double(_destRect.right - _destRect.left) / (_srcRect.right - _srcRect.left);
			factor = int(factor * 1000) / 1000.0;

			return std::format(L"{} scaling\n\nSource window: {}\nScale factor: {}",
				_isWindowedScaling ? L"Windowed" : L"Fullscreen", srcTitle, factor);
		}();

		const int padding = std::lround(10 * _dpiScale);
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

		EndPaint(Handle(), &ps);
		return 0;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == BN_CLICKED && _hwndScaling) {
			RECT targetRect;
			GetWindowRect(_hwndScaling, &targetRect);

			const LONG delta = lround((LOWORD(wParam) == 1 ? 50 : -50) * _dpiScale);
			SetWindowPos(
				_hwndScaling, NULL, 0, 0,
				targetRect.right - targetRect.left + delta,
				targetRect.bottom - targetRect.top + delta,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
			);
		}
		break;
	}
	case WM_DESTROY:
	{
		if (_hUIFont) {
			DeleteObject(_hUIFont);
		}

		PostQuitMessage(0);
		return 0;
	}
	}

	return base_type::_MessageHandler(msg, wParam, lParam);
}

void MainWindow::_UpdateScalingInfo() noexcept {
	if (!_hwndScaling) {
		return;
	}

	// Retrieve scaling information from window properties
	// 从窗口属性中检索缩放信息

	_hwndSrc = (HWND)GetProp(_hwndScaling, L"Magpie.SrcHWND");
	_isWindowedScaling = (bool)GetProp(_hwndScaling, L"Magpie.Windowed");

	_srcRect.left = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.SrcLeft");
	_srcRect.top = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.SrcTop");
	_srcRect.right = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.SrcRight");
	_srcRect.bottom = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.SrcBottom");

	_destRect.left = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.DestLeft");
	_destRect.top = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.DestTop");
	_destRect.right = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.DestRight");
	_destRect.bottom = (LONG)(INT_PTR)GetProp(_hwndScaling, L"Magpie.DestBottom");
}

void MainWindow::_HandleScalingChangedMessage(WPARAM wParam, LPARAM lParam) noexcept {
	OutputDebugString(std::format(
		L"WM_MAGPIE_SCALINGCHANGED(wParam={},lParam={})\n", wParam, lParam).c_str());

	if (wParam == 3) {
		// User has started resizing or moving the scaled window. A timer is
		// used to periodically update scaling information.
		// 用户开始调整缩放窗口大小或移动缩放窗口。使用定时器定期更新缩放信息。
		SetTimer(Handle(), TIMER_ID, 20, nullptr);
		return;
	}

	KillTimer(Handle(), TIMER_ID);

	if (wParam == 0) {
		// Scaling has ended or the source window has lost focus
		// 缩放已结束或源窗口失去焦点
		if (lParam == 0) {
			// Scaling ended
			// 缩放结束
			_hwndScaling = NULL;
			_UpdateSizeButtons();
			InvalidateRect(Handle(), nullptr, TRUE);
		}

		SetWindowPos(Handle(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
	} else if (wParam == 1) {
		// Scaling has started or the source window has regained focus
		// 缩放已开始或源窗口回到前台
		if (!_hwndScaling) {
			// Scaling started
			// 缩放开始
			_hwndScaling = (HWND)lParam;
			_UpdateScalingInfo();
			_UpdateSizeButtons();
			InvalidateRect(Handle(), nullptr, TRUE);
		}

		SetWindowPos(Handle(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
		BringWindowToTop(Handle());
	} else if (wParam == 2) {
		// The position or size of the scaled window has changed
		// 缩放窗口位置或大小改变
		_UpdateScalingInfo();
		InvalidateRect(Handle(), nullptr, TRUE);
	}
}

void MainWindow::_UpdateSizeButtons() const noexcept {
	RECT clientRect;
	GetClientRect(Handle(), &clientRect);

	const int clientHeight = clientRect.bottom - clientRect.top;
	const int padding = std::lround(10 * _dpiScale);
	const int btnWidth = std::lround(56 * _dpiScale);
	const int btnHeight = std::lround(30 * _dpiScale);
	SetWindowPos(_hwndSizeUpBtn, NULL,
		padding, clientHeight - padding - btnHeight, btnWidth, btnHeight, SWP_NOACTIVATE);
	SetWindowPos(_hwndSizeDownBtn, NULL,
		padding + btnWidth + std::lround(4 * _dpiScale), clientHeight - padding - btnHeight, btnWidth, btnHeight, SWP_NOACTIVATE);

	EnableWindow(_hwndSizeUpBtn, (BOOL)(INT_PTR)_hwndScaling);
	EnableWindow(_hwndSizeDownBtn, (BOOL)(INT_PTR)_hwndScaling);
}

void MainWindow::_UpdateDpi(uint32_t dpi) noexcept {
	_dpiScale = dpi / double(USER_DEFAULT_SCREEN_DPI);

	if (_hUIFont) {
		DeleteObject(_hUIFont);
	}

	_hUIFont = CreateFont(
		std::lround(20 * _dpiScale),
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

	SendMessage(_hwndSizeUpBtn, WM_SETFONT, (WPARAM)_hUIFont, TRUE);
	SendMessage(_hwndSizeDownBtn, WM_SETFONT, (WPARAM)_hUIFont, TRUE);
}
