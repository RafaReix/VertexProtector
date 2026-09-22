#ifdef _DEBUG
#pragma once

#include <Windows.h>
#include <iostream>

namespace Framework::Utils
{
	static void InitializeConsole()
	{
		AllocConsole();

		FILE* ConsoleOut;
		FILE* ConsoleIn;

		freopen_s(&ConsoleOut, "CONOUT$", "w", stdout);
		freopen_s(&ConsoleIn,"CONIN$", "r", stdin);

		SetConsoleTitle(L"Debug Console");
	}
}
#endif