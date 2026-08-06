#pragma once

#include <helpers/foobar2000+atl.h>

// ATL/WTL
#include <atlbase.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctrls.h>
#include <windowsx.h>

// Direct2D
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// C++ standard
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>