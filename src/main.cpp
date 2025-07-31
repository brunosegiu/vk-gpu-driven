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

// From chatgpt
glm::dvec3 ComputeSunDir(double tFull, double tYear, double latDeg) {
    const double PI = glm::pi<double>();
    const double DEG2RAD = PI / 180.0;

    auto normalize = [](const glm::dvec3& v) -> glm::dvec3 {
        double L = glm::length(v);
        return (L > 0.0) ? (v / L) : glm::dvec3(0.0);
    };

    // Solar declination (simple sinusoid; good visuals)
    auto declination = [&](double tY) {
        const double eps = 23.44 * DEG2RAD;  // axial tilt
        return eps * std::sin(2.0 * PI * (tY - 0.218));
    };

    // Daylight half-span H0 (sunrise/sunset hour-angle)
    auto daylightHalfSpan = [&](double latD, double tY) {
        double phi = latD * DEG2RAD;
        double delta = declination(tY);
        double c = -std::tan(phi) * std::tan(delta);
        if (c <= -1.0)
            return PI;  // 24h daylight
        if (c >= 1.0)
            return 0.0;  // 0h daylight
        return std::acos(c);
    };

    // Direction from hour angle H for current latitude/year
    auto fromHourAngle = [&](double H) {
        double phi = latDeg * DEG2RAD;
        double delta = declination(tYear);

        double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
        double sinDel = std::sin(delta), cosDel = std::cos(delta);

        double sinAlt = sinPhi * sinDel + cosPhi * cosDel * std::cos(H);
        sinAlt = glm::clamp(sinAlt, -1.0, 1.0);
        double alt = std::asin(sinAlt);

        // Azimuth A from north, increasing eastward
        double A = std::atan2(std::sin(H), std::cos(H) * sinPhi - std::tan(delta) * cosPhi);
        double cosAlt = std::cos(alt);

        // Sun vector in ENU (east, north, up)
        glm::dvec3 sun(
            cosAlt * std::sin(A),  // x: east
            cosAlt * std::cos(A),  // y: north
            std::sin(alt)          // z: up
        );

        // Light forward points from sun toward scene
        return normalize(-sun);
    };

    // --- Skip-night mapping ---
    double H0 = daylightHalfSpan(latDeg, tYear);
    double dayFrac = H0 / PI;  // fraction of a 24h day that is daylight

    // Polar night: keep a tiny above-horizon sun toward south (customizable)
    if (dayFrac <= 1e-6) {
        double epsAlt = 0.5 * DEG2RAD;
        glm::dvec3 sun(0.0, -std::cos(epsAlt), std::sin(epsAlt));  // just above horizon, due south
        return normalize(-sun);
    }

    // 24h daylight: map full [0,1) to hour angle [-π, π)
    if (dayFrac >= 1.0 - 1e-6) {
        double u = std::fmod(std::max(0.0, tFull), 1.0);
        double H = -PI + 2.0 * PI * u;
        return fromHourAngle(H);
    }

    // Compress full day into the daylight arc and loop it (night is skipped)
    double uFull = std::fmod(std::max(0.0, tFull), 1.0);
    double tDaylight = uFull / dayFrac;
    tDaylight -= std::floor(tDaylight);       // fract
    double H = -H0 + (2.0 * H0) * tDaylight;  // sunrise -> sunset
    return fromHourAngle(H);
}

// From chatgpt
glm::vec3 SunLightColorSimple(const glm::vec3& lightForward) {
    // Altitude from forward (dir points downward toward scene)
    double alt = std::asin(glm::clamp(-lightForward.z, -1.0f, 1.0f));
    if (alt <= 0.0)
        return {0.f, 0.f, 0.f};  // sun below horizon

    // Kasten–Young airmass X (alt in degrees)
    double altDeg = alt * (180.0 / M_PI);
    double X = 1.0 / (std::sin(alt) + 0.50572 * std::pow(altDeg + 6.07995, -1.6364));

    // Fixed atmosphere knobs (kept internal to keep the API tiny)
    const double T = 3.0;      // turbidity (clear)
    const double alpha = 1.3;  // Angström exponent
    const double beta = std::max(0.0, 0.04608365822050 * T - 0.04586025928522);

    // Wavelength samples (μm)
    const double Lr = 0.680, Lg = 0.550, Lb = 0.440;

    auto trans = [&](double L_um) {
        double tauR = 0.008735 * std::pow(L_um, -4.08);  // Rayleigh
        double tauM = beta * std::pow(L_um, -alpha);     // Mie
        return std::exp(-(tauR + tauM) * X);             // Beer–Lambert
    };

    double Tr = trans(Lr), Tg = trans(Lg), Tb = trans(Lb);

    // Fade with altitude so sunrise/sunset dim naturally
    double I = std::sin(alt);  // 0 at horizon, 1 at zenith

    return {float(Tr * I), float(Tg * I), float(Tb * I)};  // linear RGB
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
                        // Example: 10 second cycle, 5‑minute elapsed
                        double dayLengthSec = 20.0;
                        double tDay = std::fmod(totalSeconds / dayLengthSec, 1.0);

                        // Slow year cycle (e.g., 20 real minutes per “year”)
                        double yearLengthSec = 1200.0;
                        double tYear = std::fmod(totalSeconds / yearLengthSec, 1.0);
                        glm::vec3 sunDirection = ComputeSunDir(tDay, tYear, 35.0);
                        light.SetDireciton(sunDirection);
                        light.SetRadiance(SunLightColorSimple(sunDirection));
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