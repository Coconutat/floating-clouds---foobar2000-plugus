#include "stdafx.h"
#include "floating_window.h"

// ============================================================================
// Floating Clouds - Application entry point via initquit
// ============================================================================

namespace {
    class FloatingCloudsApp : public initquit {
    public:
        void on_init() {
            console::print("Floating Clouds: initializing...");
            
            // Create the floating window on the main thread
            m_window = std::make_unique<FloatingCloudsWindow>();
            m_window->initialize_window(NULL);
            
            console::print("Floating Clouds: initialized successfully");
        }
        
        void on_quit() {
            console::print("Floating Clouds: shutting down...");
            
            // Save window position
            if (m_window && m_window->IsWindow()) {
                CRect rect;
                m_window->GetWindowRect(&rect);
                cfg_var_modern::cfg_int cfg_x(cfg_guids::window_x, 0);
                cfg_var_modern::cfg_int cfg_y(cfg_guids::window_y, 0);
                cfg_x = rect.left;
                cfg_y = rect.top;
            }
            
            m_window.reset();
            
            console::print("Floating Clouds: shut down");
        }
        
    private:
        std::unique_ptr<FloatingCloudsWindow> m_window;
    };
    
    FB2K_SERVICE_FACTORY(FloatingCloudsApp);
    
} // namespace