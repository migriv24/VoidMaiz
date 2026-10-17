/* src/shell/frame.hpp — the part of every shell that is the same on every
 * platform: the order of a frame, and the app's lifecycle around it. Internal
 * to voidmaiz_app; each platform's shell (desktop, Android, iOS) owns the
 * window, the GPU and the events, and hands the rest to a Runner. */
#pragma once

#include "voidmaiz/app.hpp"
#include "voidmaiz/textinputview.hpp"

#include <memory>
#include <vector>

namespace maiz::shell {

class Runner {
  public:
    explicit Runner(Device& device);

    /* Make the app (maiz::create_app) and give it its fonts. Call once, after
     * ImGui::CreateContext and before the renderer builds the font atlas. */
    void create();
    /* Its one-time start. Call before the first frame. */
    void start();
    bool started() const { return started_; }

    /* Between ImGui::NewFrame and ImGui::Render: the keyboard, the safe area,
     * the touch gate, the app. Returns true when Back was asked for and nobody
     * claimed it, so the shell does the platform's default. */
    bool frame();

    void event(AppEvent e);
    /* Queued, and delivered at the start of the next frame (never mid-frame:
     * an arriving document may replace the whole state). */
    void document(DocumentResult r);

    App* app() { return app_.get(); }

  private:
    Device& device_;
    std::unique_ptr<App> app_;
    bool started_ = false;
    TextInputSession text_;
    std::vector<DocumentResult> pending_;
};

} // namespace maiz::shell
