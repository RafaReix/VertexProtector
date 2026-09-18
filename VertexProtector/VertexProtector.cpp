#include <Framework/Framework.hpp>
#include <Framework/Utils/Window.hpp>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
	Framework::Utils::InitializeConsole();
#endif

	Framework::Utils::Window window;

	if (!window.Initialize())
		return -1;

	window.Run();

#ifdef _DEBUG
	FreeConsole();
#endif

	return 0;
}
