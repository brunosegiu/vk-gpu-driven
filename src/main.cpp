#include <chrono>

#include <glm/gtx/rotate_vector.hpp>

#include "DebugUtils.h"
#include "Renderer.h"

struct Timer {
    void Start() { beginTime = std::chrono::steady_clock::now(); }
    template <typename M>
    uint64_t Elapsed() {
        return std::chrono::duration_cast<M>(std::chrono::steady_clock::now() - beginTime).count();
    }
    uint64_t ElapsedMillis() { return Elapsed<std::chrono::milliseconds>(); }
    uint64_t ElapsedMicros() { return Elapsed<std::chrono::microseconds>(); }
    double ElapsedSeconds() {
        return static_cast<double>(Elapsed<std::chrono::microseconds>()) / 1000000.0;
    }

    std::chrono::steady_clock::time_point beginTime;
};

int main() {
    using namespace VKRT;
    auto [windowResult, window] = Window::Create(1920, 1080);
    VKRT_ASSERT_MSG(windowResult == Result::Success, "Couldn't create window");
    if (windowResult == Result::Success) {
        auto [contextResult, context] = window->CreateContext();
        VKRT_ASSERT_MSG(contextResult == Result::Success, "No compatible GPU found");
        if (contextResult == Result::Success) {
            ScopedRefPtr<Scene> scene = new Scene(context);

            scene->Load("./assets/Bistro.vkrt");

            ScopedRefPtr<Camera> camera = new Camera(window);
            camera->SetTranslation(glm::vec3(-14.0f, 5.0f, -1.0f));
            camera->SetForwardDir(glm::normalize(glm::vec3(0.9f, -0.2f, 0.1f)));
            ScopedRefPtr<SettingsManager> settingsManager = new SettingsManager();
            ScopedRefPtr<Renderer> renderer = new Renderer(context, scene, settingsManager);

            Timer timer;
            double elapsedSeconds = 0.0;
            double totalSeconds = 0.0;

            while (window->Update()) {
                timer.Start();
                {
                    camera->Update(elapsedSeconds);
                    renderer->Render(camera);
                }
                elapsedSeconds = timer.ElapsedSeconds();
                totalSeconds += elapsedSeconds;
            }
        }
        if (context != nullptr) {
            window->DestroyContext();
        }
    }
    return 0;
}