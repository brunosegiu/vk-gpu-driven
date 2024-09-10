#include <chrono>

#include "Camera.h"
#include "Context.h"
#include "DebugUtils.h"
#include "Renderer.h"
#include "Scene.h"
#include "Window.h"

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

void LoadBasicScene(
    VKRT::ScopedRefPtr<VKRT::Context> context,
    VKRT::ScopedRefPtr<VKRT::Scene> scene) {
    using namespace VKRT;
    {
        ScopedRefPtr<Object> boxMesh = Object::Load(context, scene, "./assets/box.glb");
        boxMesh->SetTranslation(glm::vec3(0.0f, 5.0f, 0.0f));
        boxMesh->SetScale(glm::vec3(7.5f, 5.0f, 7.5f));
        boxMesh->Rotate(glm::vec3(0.0f, 90.0f, 0.0f));
    }

    {
        ScopedRefPtr<Object> dragon =
            Object::Load(context, scene, "./assets/DragonAttenuation.glb");
        dragon->SetTranslation(glm::vec3(0.0f, 0.0f, 0.5f));
        dragon->SetScale(glm::vec3(6.0f, 7.5f, 4.0f));
        dragon->Rotate(glm::vec3(0.0f, 90.0f, 0.0f));
    }

    {
        ScopedRefPtr<Object> object = Object::Load(context, scene, "./assets/venus.gltf");
        object->SetTranslation(glm::vec3(3.0f, 3.5f, 0.0f));
        object->SetScale(glm::vec3(0.6f));
        object->Rotate(glm::vec3(90.0f, 90.0f, 0.0f));
    }

    {
        ScopedRefPtr<Object> object = Object::Load(context, scene, "./assets/bunny.glb");
        object->SetTranslation(glm::vec3(2.0f, 0.0f, 3.5f));
        object->SetScale(glm::vec3(0.05f));
        object->SetRotation(glm::vec3(270.0f, 90.0f, 0.0f));
    }

    {
        ScopedRefPtr<Object> teapot = Object::Load(context, scene, "./assets/utahTeapot.glb");
        teapot->SetTranslation(glm::vec3(2.5f, 2.5f, -3.5f));
        teapot->SetScale(glm::vec3(1.025f));
        teapot->Rotate(glm::vec3(0.0f, -90.0f, 0.0f));
    }
}

void LoadBistro(VKRT::ScopedRefPtr<VKRT::Context> context, VKRT::ScopedRefPtr<VKRT::Scene> scene) {
    using namespace VKRT;
    ScopedRefPtr<Object> bistro = Object::Load(context, scene, "./assets/Bistro.glb");
    bistro->SetTranslation(glm::vec3(0.0f, 0.0f, 0.0f));
    bistro->SetScale(glm::vec3(1.0f));
}

int main() {
    using namespace VKRT;
    auto [windowResult, window] = Window::Create(1280, 720);
    VKRT_ASSERT_MSG(windowResult == Result::Success, "Couldn't create window");
    if (windowResult == Result::Success) {
        auto [contextResult, context] = window->CreateContext();
        VKRT_ASSERT_MSG(contextResult == Result::Success, "No compatible GPU found");
        if (contextResult == Result::Success) {
            ScopedRefPtr<Scene> scene = new Scene(context);

            LoadBistro(context, scene);

            ScopedRefPtr<Camera> camera = new Camera(window);
            camera->SetTranslation(glm::vec3(-14.0f, 5.0f, -1.0f));
            camera->SetForwardDir(glm::normalize(glm::vec3(0.9f, -0.2f, 0.1f)));

            ScopedRefPtr<Renderer> renderer = new Renderer(context, scene);
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