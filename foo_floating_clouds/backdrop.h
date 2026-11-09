#pragma once

// ============================================================================
// backdrop.h - the desktop behind the window, as pixels.
//
// Why this exists
// ---------------
// An UpdateLayeredWindow surface is composited by DWM and cannot sample what is
// behind it, which is why every skin before Liquid Glass had to be near-opaque.
// There is exactly one way to lift that: capture the screen region under the
// card and bake the capture into the card's own pixels.
//
// The one problem is that the capture would contain the card itself, and a
// translucent card that samples itself is a feedback loop. The clean mechanism,
// measured on this machine (logs/glass/probe/capture/RESULTS.md):
//
//   SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)  // Win10 2004+
//   BitBlt(GetDC(NULL) -> DIB, SRCCOPY)
//
// With the affinity set, the card is absent from the capture and the capture
// holds the real composited desktop (verified: 56000/56000 px exactly match the
// true backdrop; without the affinity 56000/56000 px are the card's own colour).
//
// The cost, also measured, is what shapes this class:
//
//   * a screen-DC BitBlt blocks for exactly ONE composition period (6.94 ms at
//     144 Hz, ~16.7 ms at 60 Hz) regardless of the captured region's size - it
//     waits for the next composition boundary, it is not doing work;
//   * therefore it must never run on the UI thread: one capture would drop a
//     whole frame;
//   * and it cannot run at content frame rate: a continuous loop would keep one
//     thread permanently parked on the compositor.
//
// So: a worker thread, a modest capture interval, and latest-frame handoff. The
// renderer asks "is there a newer frame?" and never blocks.
//
// The trade-off the mechanism forces (documented, not hidden): while the
// affinity is set the card is excluded from EVERY capture consumer - Snipping
// Tool, OBS, Teams/Zoom share, Windows.Graphics.Capture. It is therefore enabled
// only while a glass skin is selected, and WDA_NONE is restored on the way out.
// ============================================================================

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif

namespace glass {

class BackdropSource
{
public:
    BackdropSource() = default;
    ~BackdropSource() { stop(); }

    BackdropSource(const BackdropSource&) = delete;
    BackdropSource& operator=(const BackdropSource&) = delete;

    // Feature-detects capture exclusion on the window and starts the worker.
    // Returns false when this OS/window cannot exclude itself (pre-Win10 2004,
    // a non-top-level window, a policy block); the caller then uses the static
    // frosted fallback. Safe to call when already started.
    bool start(HWND hwnd)
    {
        if (m_started) return m_available;
        if (!hwnd || !::IsWindow(hwnd)) return false;
        m_hwnd = hwnd;

        if (!::SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
            const DWORD err = ::GetLastError();
            m_status = "capture exclusion unavailable (err " + std::to_string((unsigned)err) + ")";
            m_available = false;
            return false;
        }
        m_available = true;
        m_status = "backdrop capture ready";

        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_quit = false;
        }
        m_thread = std::thread(&BackdropSource::thread_main, this);
        m_started = true;
        return true;
    }

    // Stops the worker and hands the window back to normal capture consumers.
    void stop()
    {
        if (!m_started) return;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_quit = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
        if (m_hwnd && ::IsWindow(m_hwnd)) ::SetWindowDisplayAffinity(m_hwnd, WDA_NONE);
        m_started = false;
        m_available = false;
        m_hwnd = nullptr;
        m_status = "stopped";
    }

    bool available() const { return m_available; }
    const std::string& status() const { return m_status; }

    // Region to capture, in the screen coordinate space of the window, plus the
    // transparent margin on every side that the lens needs as source material.
    // `interval_ms` is how often the worker refreshes; 0 means "as fast as the
    // compositor allows" (used while the window is being dragged).
    void set_region(int x, int y, int win_w, int win_h, int pad, int interval_ms)
    {
        if (win_w <= 0 || win_h <= 0) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            changed = (x != m_x || y != m_y || win_w != m_win_w || win_h != m_win_h ||
                       pad != m_pad || interval_ms != m_interval);
            m_x = x; m_y = y;
            m_win_w = win_w; m_win_h = win_h;
            m_pad = pad;
            if (interval_ms != m_interval) m_interval = interval_ms;
            if (changed) m_region_serial++;   // the worker captures immediately
            m_have_region = true;
        }
        if (changed) m_cv.notify_all();
    }

    // Ask for a fresh frame now (window moved/resized, style changed). The
    // worker still cannot beat the compositor, so this is a hint, not a sync.
    void request_now()
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_urgent = true;
        }
        m_cv.notify_all();
    }

    // Latest frame, exactly once per serial. Copies into `out` only when it is
    // newer than `serial`, so a caller holding an already-built material can
    // skip all work.
    bool fetch(std::vector<uint32_t>& out, int& win_w, int& win_h, int& pad, uint64_t& serial)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_front_serial == 0 || m_front_serial == serial) return false;
        const size_t n = (size_t)(m_front_w + 2 * m_front_pad) * (size_t)(m_front_h + 2 * m_front_pad);
        if (m_front.size() != n || m_front_w != m_win_w || m_front_h != m_win_h) return false;
        out = m_front;
        win_w = m_front_w;
        win_h = m_front_h;
        pad = m_front_pad;
        serial = m_front_serial;
        return true;
    }

    uint64_t captures() const { return m_captures.load(); }
    uint64_t failures() const { return m_failures.load(); }
    double last_capture_ms() const { return m_last_ms.load(); }

private:
    void thread_main()
    {
        std::vector<uint32_t> frame;   // worker-owned capture buffer
        HDC mem = nullptr;
        HBITMAP dib = nullptr;
        void* bits = nullptr;
        int cap_w = 0, cap_h = 0;

        for (;;) {
            int x = 0, y = 0, ww = 0, wh = 0, pad = 0;
            {
                std::unique_lock<std::mutex> lk(m_mtx);
                if (m_quit) break;
                // Wake on: quit, an urgent request, a region change, or the
                // capture interval. interval 0 means drag mode: capture as fast
                // as the compositor allows, but never spin.
                const auto has_work = [this] {
                    return m_quit || m_urgent || m_region_serial != m_captured_region_serial;
                };
                if (!has_work()) {
                    m_cv.wait_for(lk, std::chrono::milliseconds(m_interval > 0 ? m_interval : 16),
                                  [&] { return has_work(); });
                }
                if (m_quit) break;
                if (!m_have_region || m_win_w <= 0) continue;
                x = m_x; y = m_y; ww = m_win_w; wh = m_win_h; pad = m_pad;
                m_urgent = false;
                m_captured_region_serial = m_region_serial;
            }

            const int cw = ww + 2 * pad;
            const int ch = wh + 2 * pad;
            if (cw <= 0 || ch <= 0) continue;

            const ULONGLONG t0 = ::GetTickCount64();
            HDC sdc = ::GetDC(nullptr);
            if (!sdc) { m_failures++; continue; }

            if (!dib || cap_w != cw || cap_h != ch) {
                if (mem) { ::DeleteDC(mem); mem = nullptr; }
                if (dib) { ::DeleteObject(dib); dib = nullptr; }
                BITMAPINFO bi = {};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = cw;
                bi.bmiHeader.biHeight = -ch;      // top-down
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                dib = ::CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (!dib) { ::ReleaseDC(nullptr, sdc); m_failures++; continue; }
                mem = ::CreateCompatibleDC(sdc);
                if (!mem) { ::DeleteObject(dib); dib = nullptr; ::ReleaseDC(nullptr, sdc); m_failures++; continue; }
                ::SelectObject(mem, dib);
                cap_w = cw; cap_h = ch;
                frame.resize((size_t)cw * ch);
            }

            // The one blocking call in this module: it waits for the next
            // composition boundary, which is why this is a worker thread.
            const BOOL ok = ::BitBlt(mem, 0, 0, cw, ch, sdc, x - pad, y - pad, SRCCOPY);
            ::ReleaseDC(nullptr, sdc);
            if (!ok) { m_failures++; continue; }

            // Screen captures carry no alpha semantics; the material treats the
            // capture as opaque and supplies coverage itself.
            const uint32_t* src = (const uint32_t*)bits;
            const size_t n = (size_t)cw * ch;
            for (size_t i = 0; i < n; i++) frame[i] = src[i] | 0xFF000000u;

            {
                std::lock_guard<std::mutex> lk(m_mtx);
                m_front.swap(frame);
                m_front_w = ww;
                m_front_h = wh;
                m_front_pad = pad;
                m_front_serial++;
            }
            // `frame` now holds the previous front buffer; reuse it next round.
            frame.resize(n);
            m_captures++;
            m_last_ms.store((double)(::GetTickCount64() - t0));
        }

        if (mem) ::DeleteDC(mem);
        if (dib) ::DeleteObject(dib);
    }

    HWND m_hwnd = nullptr;
    std::thread m_thread;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_started = false;
    std::atomic<bool> m_available{false};

    // Requested region (guarded by m_mtx)
    int m_x = 0, m_y = 0, m_win_w = 0, m_win_h = 0, m_pad = 0, m_interval = 80;
    bool m_have_region = false;
    bool m_urgent = false;
    bool m_quit = false;
    int m_region_serial = 0;
    int m_captured_region_serial = -1;

    // Latest completed frame (guarded by m_mtx)
    std::vector<uint32_t> m_front;
    int m_front_w = 0, m_front_h = 0, m_front_pad = 0;
    uint64_t m_front_serial = 0;

    std::string m_status;
    std::atomic<uint64_t> m_captures{0};
    std::atomic<uint64_t> m_failures{0};
    std::atomic<double> m_last_ms{0.0};
};

} // namespace glass
