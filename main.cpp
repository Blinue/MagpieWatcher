#include "MainWindow.h"
#include <CommCtrl.h>

static void InitNativeControls() noexcept {
	INITCOMMONCONTROLSEX icce{
		.dwSize = sizeof(INITCOMMONCONTROLSEX),
		.dwICC = ICC_STANDARD_CLASSES
	};
	InitCommonControlsEx(&icce);
}

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPWSTR /*lpCmdLine*/,
	_In_ int /*nCmdShow*/
) {
	InitNativeControls();

	MainWindow mainWindow;
	if (!mainWindow.Create(hInstance)) {
		return 1;
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		// 实现 tab 导航
		if (!IsDialogMessage(mainWindow.Handle(), &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}
