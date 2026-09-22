#pragma once

// Windows Libraries
#include <Windows.h>

// Utils
#ifdef _DEBUG
#include "Utils/Console.hpp"
#endif

#ifdef _DEBUG
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif