#pragma once

#include "InputManager.h"
#include "RefCountPtr.h"
#include "Window.h"
#include "ViewFrustum.h"

namespace VKRT {
class Camera : public RefCountPtr, public InputEventListener {
public:
    Camera(ScopedRefPtr<Window> window);

    void Update(float deltaTime);

    void SetTranslation(const glm::vec3& position);
    void Translate(const glm::vec3& delta);

    const glm::vec3& GetForwardDir() { return mForwardDir; }
    void SetForwardDir(const glm::vec3& forwardDir) { mForwardDir = forwardDir; }

    const glm::mat4& GetViewTransform() { return mViewTransform; }
    const glm::mat4& GetProjectionTransform() { return mProjectionTransform; }
    const uint32_t GetFramesSinceMoved() const { return mFramesSinceMoved; }
    const glm::vec3& GetPosition() const { return mPosition; }

    const ViewFrustum& GetViewFrustum() const { return mViewFrustum; }

    ~Camera();

private:
    void OnKeyPressed(int key) override;
    void OnKeyReleased(int key) override;
    void OnMouseMoved(glm::vec2 newPos) override;
    void OnLeftMouseButtonPressed() override;
    void OnLeftMouseButtonReleased() override;
    void OnRightMouseButtonPressed() override;
    void OnRightMouseButtonReleased() override;

    void UpdateViewTransform();

    ScopedRefPtr<Window> mWindow;

    float mMovementSpeed, mRotationSpeed;

    glm::mat4 mViewTransform;
    glm::mat4 mProjectionTransform;

    glm::vec3 mForwardDir;
    glm::vec3 mPosition;
    float mPitch, mYaw;

    struct KeyStates {
        bool forwardPressed = false;
        bool backwardsPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;
    };
    KeyStates mKeyStates;
    glm::vec2 mCurrentMousePos;
    bool mActive;
    bool mSpeedModifierActive;
    uint32_t mFramesSinceMoved;

    bool mFreezeFrustumUpdates;
    ViewFrustum mViewFrustum;
};

}  // namespace VKRT