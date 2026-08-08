#pragma once

#include <helpers/foobar2000+atl.h>

// ATL/WTL
#include <atlbase.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctrls.h>
#include <windowsx.h>

// Direct2D / Direct3D / DirectComposition
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi.h>
#include <dwrite.h>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// C++ standard
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

// MD3 (Material 3) design tokens
#include "md3_theme.h"