#pragma once

#if _DEBUG

#include "API/DX11/DX11Exception.h"

#define DX11_CHECK(expr) { HRESULT __hResult = expr; if(FAILED(__hResult)){ throw api::DX11Exception(__LINE__,__FILE__, __hResult);}}

#else

#define DX11_CHECK(expr) expr;

#endif