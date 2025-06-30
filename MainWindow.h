#pragma once
#include "WindowBase.h"
#include <cstdint>

class MainWindow : public WindowBaseT<MainWindow> {
	using base_type = WindowBaseT<MainWindow>;
	friend base_type;

public:
	bool Create(HINSTANCE hInst) noexcept;

protected:
	LRESULT _MessageHandler(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

private:
	void _UpdateScalingInfo() noexcept;

	void _UpdateSizeButtons() const noexcept;

	void _UpdateDpi(uint32_t dpi) noexcept;

	double _dpiScale = 1.0;
	HFONT _hUIFont = NULL;

	HWND _hwndSizeUpBtn = NULL;
	HWND _hwndSizeDownBtn = NULL;

	HWND _hwndScaling = NULL;
	HWND _hwndSrc = NULL;
	RECT _srcRect{};
	RECT _destRect{};
	bool _isWindowedScaling = false;
};
