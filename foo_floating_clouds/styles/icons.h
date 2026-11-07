#pragma once

// ============================================================================
// icons.h - the control-row glyphs, drawn as vector paths.
//
// Why not text glyphs: the previous icons were Unicode characters (U+23EE,
// U+23EF, U+1F507, U+2630) resolved through font fallback. They came from three
// different fonts, had three different weights and metrics, the mute glyph was
// a colour emoji, and every symbol shifted with the user's font choice. HIG
// asks for one consistent size, level of detail and stroke weight across all
// interface icons (icons.md > Best practices), which a font fallback chain
// cannot guarantee.
//
// Every glyph here is built in a 24x24 unit box centred on the origin, as a
// FILLED path, so:
//   * all of them share one visual weight by construction (no mixture of
//     filled and stroked shapes, which is the usual reason a small icon row
//     looks uneven);
//   * drawing is a transform plus one FillGeometry, no font metrics involved;
//   * the same unit geometry is reused at any button size.
// ============================================================================

#include <d2d1.h>
#include <d2d1helper.h>
#include <cmath>
#include <algorithm>

namespace icons {

enum class Kind {
    Prev,
    Play,
    Pause,
    Next,
    VolumeOn,
    VolumeOff,
    Queue,
    Count
};

constexpr int kKindCount = (int)Kind::Count;

namespace detail {

inline D2D1_POINT_2F polar(float cx, float cy, float r, float deg)
{
    const float a = deg * 3.14159265358979f / 180.0f;
    return D2D1::Point2F(cx + r * cosf(a), cy + r * sinf(a));
}

// A wave arc drawn as a filled crescent: outer radius R, inner radius R - t.
// Constant thickness, so it matches the weight of the filled speaker body.
inline void add_crescent(ID2D1GeometrySink* sink, float cx, float cy,
                         float r, float t, float a0, float a1)
{
    const float R = r + t * 0.5f;
    const float Ri = r - t * 0.5f;
    const D2D1_POINT_2F o0 = polar(cx, cy, R, a0);
    const D2D1_POINT_2F o1 = polar(cx, cy, R, a1);
    const D2D1_POINT_2F i1 = polar(cx, cy, Ri, a1);
    const D2D1_POINT_2F i0 = polar(cx, cy, Ri, a0);

    sink->BeginFigure(o0, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc(D2D1::ArcSegment(o1, D2D1::SizeF(R, R), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(i1);
    sink->AddArc(D2D1::ArcSegment(i0, D2D1::SizeF(Ri, Ri), 0.0f,
                                  D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

inline void add_triangle(ID2D1GeometrySink* sink, D2D1_POINT_2F a, D2D1_POINT_2F b, D2D1_POINT_2F c)
{
    sink->BeginFigure(a, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(b);
    sink->AddLine(c);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

// A bar with rounded ends, built as a closed path so it matches the weight of
// the neighbouring triangles (FilledRoundedRect would work too, but keeping one
// path per icon keeps the draw a single FillGeometry).
inline void add_bar(ID2D1GeometrySink* sink, float x0, float y0, float x1, float y1, float r)
{
    const float w = x1 - x0;
    const float h = y1 - y0;
    r = (std::min)(r, (std::min)(w, h) * 0.5f);
    sink->BeginFigure(D2D1::Point2F(x0 + r, y0), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(x1 - r, y0));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x1, y0 + r), D2D1::SizeF(r, r), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(D2D1::Point2F(x1, y1 - r));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x1 - r, y1), D2D1::SizeF(r, r), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(D2D1::Point2F(x0 + r, y1));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x0, y1 - r), D2D1::SizeF(r, r), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(D2D1::Point2F(x0, y0 + r));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x0 + r, y0), D2D1::SizeF(r, r), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

// Speaker body shared by VolumeOn / VolumeOff (already optically centred in the
// box together with the waves).
inline void add_speaker(ID2D1GeometrySink* sink)
{
    sink->BeginFigure(D2D1::Point2F(-6.5f, -3.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(-2.7f, -3.0f));
    sink->AddLine(D2D1::Point2F(1.1f, -7.6f));
    sink->AddLine(D2D1::Point2F(1.1f, 7.6f));
    sink->AddLine(D2D1::Point2F(-2.7f, 3.0f));
    sink->AddLine(D2D1::Point2F(-6.5f, 3.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

} // namespace detail

// Build the unit-box geometry for one glyph. Caller owns the geometry.
inline bool build_unit_geometry(ID2D1Factory* factory, Kind kind, ID2D1PathGeometry** out)
{
    if (!factory || !out) return false;
    *out = nullptr;

    ID2D1PathGeometry* geo = nullptr;
    if (FAILED(factory->CreatePathGeometry(&geo)) || !geo) return false;

    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(geo->Open(&sink)) || !sink) { geo->Release(); return false; }
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    using namespace detail;
    switch (kind) {
    case Kind::Play:
        // Optical centring: a triangle centred on its bounding box looks too
        // far left, so it is nudged right.
        add_triangle(sink, D2D1::Point2F(-5.9f, -7.2f), D2D1::Point2F(5.9f, 0.0f),
                     D2D1::Point2F(-5.9f, 7.2f));
        break;

    case Kind::Pause:
        add_bar(sink, -5.6f, -7.0f, -1.6f, 7.0f, 1.4f);
        add_bar(sink, 1.6f, -7.0f, 5.6f, 7.0f, 1.4f);
        break;

    case Kind::Prev:
        add_bar(sink, -6.2f, -7.0f, -4.2f, 7.0f, 1.0f);
        add_triangle(sink, D2D1::Point2F(6.2f, -7.0f), D2D1::Point2F(-2.4f, 0.0f),
                     D2D1::Point2F(6.2f, 7.0f));
        break;

    case Kind::Next:
        add_bar(sink, 4.2f, -7.0f, 6.2f, 7.0f, 1.0f);
        add_triangle(sink, D2D1::Point2F(-6.2f, -7.0f), D2D1::Point2F(2.4f, 0.0f),
                     D2D1::Point2F(-6.2f, 7.0f));
        break;

    case Kind::VolumeOn:
        add_speaker(sink);
        add_crescent(sink, 1.1f, 0.0f, 4.2f, 2.0f, -40.0f, 40.0f);
        add_crescent(sink, 1.1f, 0.0f, 7.0f, 2.0f, -40.0f, 40.0f);
        break;

    case Kind::VolumeOff: {
        add_speaker(sink);
        // Slash over the wave zone, the canonical mute mark. The four corners
        // are rotated by hand: a geometry sink has no transform, and at ~1.4px
        // the difference between flat and round ends is invisible.
        const D2D1_POINT_2F lo = D2D1::Point2F(1.5f, -7.4f);
        const D2D1_POINT_2F hi = D2D1::Point2F(8.1f, 2.6f);
        const float cx = (lo.x + hi.x) * 0.5f, cy = (lo.y + hi.y) * 0.5f;
        const float a = atan2f(hi.y - lo.y, hi.x - lo.x);
        const float ux = cosf(a), uy = sinf(a);          // along the slash
        const float nx = -uy, ny = ux;                   // across it
        const float hl = 0.5f * sqrtf((hi.x - lo.x) * (hi.x - lo.x) + (hi.y - lo.y) * (hi.y - lo.y));
        const float hw = 1.15f;
        auto corner = [&](float al, float aw) {
            return D2D1::Point2F(cx + ux * al + nx * aw, cy + uy * al + ny * aw);
        };
        sink->BeginFigure(corner(-hl, -hw), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(corner(hl, -hw));
        sink->AddLine(corner(hl, hw));
        sink->AddLine(corner(-hl, hw));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        break;
    }

    case Kind::Queue:
        // list.bullet: three rows of a dot and a rule.
        for (int i = -1; i <= 1; i++) {
            const float y = 5.6f * (float)i;
            const float dot_r = 1.35f;
            const D2D1_POINT_2F dc = D2D1::Point2F(-6.4f, y);
            sink->BeginFigure(D2D1::Point2F(dc.x + dot_r, dc.y), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(dc.x - dot_r, dc.y),
                                          D2D1::SizeF(dot_r, dot_r), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(dc.x + dot_r, dc.y),
                                          D2D1::SizeF(dot_r, dot_r), 0.0f,
                                          D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            add_bar(sink, -3.6f, y - 1.3f, 6.4f, y + 1.3f, 1.3f);
        }
        break;

    default:
        break;
    }

    if (FAILED(sink->Close())) { sink->Release(); geo->Release(); return false; }
    sink->Release();
    *out = geo;
    return true;
}

// Draw one glyph, filling `box` square pixels centred at `center`. The render
// target's current transform is preserved (the window applies a show/hide
// translation to the whole surface).
inline void draw(ID2D1RenderTarget* rt, ID2D1PathGeometry* unit_geo, ID2D1SolidColorBrush* brush,
                 D2D1_POINT_2F center, float box, const D2D1_COLOR_F& color)
{
    if (!rt || !unit_geo || !brush || box <= 0.0f) return;

    D2D1_MATRIX_3X2_F base;
    rt->GetTransform(&base);
    const float s = box / 24.0f;
    // Row-vector convention: the glyph is scaled, moved to its centre, THEN
    // carried by the caller's transform (the window's show/hide drift). Getting
    // this order wrong pre-translates the glyph by the card offset before the
    // scale and throws it out of its button.
    rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s) *
                     D2D1::Matrix3x2F::Translation(center.x, center.y) * base);
    brush->SetColor(color);
    rt->FillGeometry(unit_geo, brush);
    rt->SetTransform(base);
}

} // namespace icons
