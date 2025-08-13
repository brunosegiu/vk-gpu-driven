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

void LoadBasicScene(
    VKRT::ScopedRefPtr<VKRT::Context> context,
    VKRT::ScopedRefPtr<VKRT::Scene> scene) {
    using namespace VKRT;

    {
        ScopedRefPtr<Object> dragon =
            Object::Load(context, scene, "./assets/DragonAttenuation.glb");
        dragon->SetTranslation(glm::vec3(0.0f, 0.0f, 0.5f));
        dragon->SetScale(glm::vec3(6.0f, 7.5f, 4.0f));
        dragon->Rotate(glm::vec3(0.0f, 90.0f, 0.0f));
    }

    {
        ScopedRefPtr<Object> object = Object::Load(context, scene, "./assets/venus.gltf");
        object->SetTranslation(glm::vec3(10.0f, 3.5f, 15.0f));
        object->SetScale(glm::vec3(10.0f));
        object->Rotate(glm::vec3(0.0f, 90.0f, 0.0f));
        object->GetChildren().front()->GetMeshes().front()->GetMaterial()->SetAlphaMode(
            Material::AlphaMode::Blended);
    }

    {
        ScopedRefPtr<Object> teapot = Object::Load(context, scene, "./assets/utahTeapot.glb");
        teapot->SetTranslation(glm::vec3(10.0f, 2.5f, -15.5f));
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

// From chatgpt
struct SunLight {
    glm::vec3 direction;  // Unit vector pointing from scene origin toward the sun
    glm::vec3 radiance;   // RGB color/intensity of the sunlight
};

SunLight GetSunDirectionAndRadiance(
    float elapsedSeconds,
    float dayDurationSeconds = 60.0f,
    float latitudeDegrees = 35.0f) {
    // Wrap to [0, dayDuration)
    float t = fmod(elapsedSeconds, dayDurationSeconds);

    constexpr float pi = glm::pi<float>();
    float angle =
        pi * (0.75 * (t / dayDurationSeconds));  // θ = pi/4 at sunrise, 3 * pi / 4 at sunset

    // Sun arc in XZ plane (Y = up): arc from east (−Z), through +Y, to west (+Z)
    glm::vec3 sunDir =
        glm::normalize(glm::vec3(0.0f, sin(angle), -cos(angle)));  // at θ=0: horizon, +Z

    // Apply tilt by latitude (optional, rotate around X axis)
    float tiltRad = glm::radians(latitudeDegrees);
    sunDir = glm::rotateX(sunDir, tiltRad);

    // Compute elevation from Y component
    float elevation = glm::clamp(sunDir.y, 0.0f, 1.0f);
    float t_elev = elevation;

    // Color gradient: warm at horizon → white at zenith
    glm::vec3 horizonColor = glm::vec3(1.0f, 0.5f, 0.2f);  // sunrise/sunset
    glm::vec3 zenithColor = glm::vec3(1.0f, 1.0f, 0.9f);   // noon
    glm::vec3 color = glm::mix(horizonColor, zenithColor, t_elev);

    // Intensity: dim at horizon, bright at zenith
    float intensity = glm::mix(0.1f, 1.0f, t_elev);
    glm::vec3 radiance = color * intensity;

    return {sunDir, radiance};
}

int main() {
    using namespace VKRT;
    auto [windowResult, window] = Window::Create(1920, 1080);
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

            DirectionalLight& light = scene->GetLight();

            Timer timer;
            double elapsedSeconds = 0.0;
            double totalSeconds = 0.0;
            while (window->Update()) {
                timer.Start();
                {
                    camera->Update(elapsedSeconds);
                    {
                        SunLight lightProperties = GetSunDirectionAndRadiance(totalSeconds);
                        light.SetDirection(-lightProperties.direction);
                        light.SetRadiance(lightProperties.radiance);
                    }
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