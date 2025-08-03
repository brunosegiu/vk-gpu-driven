#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VKRT {
Camera::Camera(ScopedRefPtr<Window> window)
    : mWindow(window),
      mMovementSpeed(2.0f),
      mRotationSpeed(1.0f),
      mViewTransform(1.0f),
      mForwardDir(0.0f, 1.0f, 0.0f),
      mPosition(0.0f),
      mPitch(0.0f),
      mYaw(0.0f),
      mActive(false),
      mSpeedModifierActive(false),
      mCurrentMousePos(0.0, 0.0),
      mFramesSinceMoved(0),
      mFreezeFrustumUpdates(false),
      mViewFrustum(glm::mat4(1.0f)) {
    ScopedRefPtr<InputManager> inputManager = mWindow->GetInputManager();
    inputManager->Subscribe(this);
    auto windowSize = mWindow->GetSize();
    mProjectionTransform = glm::perspective(
        glm::radians(60.0),
        static_cast<double>(windowSize.width) / static_cast<double>(windowSize.height),
        0.1,
        1000.0);
    mProjectionTransform[1][1] *= -1.0f;
}

void Camera::UpdateViewTransform() {
    mViewTransform = glm::lookAt(mPosition, mPosition + mForwardDir, glm::vec3(0.0f, 1.0f, 0.0f));
    if (!mFreezeFrustumUpdates) {
        mViewFrustum.Update(mProjectionTransform * mViewTransform);
    }
}

void Camera::Update(float deltaTime) {
    mForwardDir = glm::normalize(glm::vec3(
        glm::cos(mYaw) * glm::cos(mPitch),
        glm::sin(mPitch),
        glm::sin(mYaw) * glm::cos(mPitch)));

    const float moveDelta = deltaTime * mMovementSpeed;
    mFramesSinceMoved = mActive ? 0 : mFramesSinceMoved + 1;
    if (mKeyStates.forwardPressed) {
        mPosition += mForwardDir * moveDelta;
        mFramesSinceMoved = 0;
    }
    if (mKeyStates.backwardsPressed) {
        mPosition += -mForwardDir * moveDelta;
        mFramesSinceMoved = 0;
    }
    const glm::vec3 rightDir = glm::normalize(glm::cross(mForwardDir, glm::vec3(0.0f, 1.0f, 0.0f)));
    if (mKeyStates.rightPressed) {
        mPosition += rightDir * moveDelta;
        mFramesSinceMoved = 0;
    }
    if (mKeyStates.leftPressed) {
        mPosition += -rightDir * moveDelta;
        mFramesSinceMoved = 0;
    }
    UpdateViewTransform();
}

void Camera::SetTranslation(const glm::vec3& position) {
    mPosition = position;
}

void Camera::Translate(const glm::vec3& delta) {
    mPosition += delta;
}

void Camera::OnKeyPressed(int key) {
    if (key == GLFW_KEY_W) {
        mKeyStates.forwardPressed = true;
    } else if (key == GLFW_KEY_S) {
        mKeyStates.backwardsPressed = true;
    } else if (key == GLFW_KEY_D) {
        mKeyStates.rightPressed = true;
    } else if (key == GLFW_KEY_A) {
        mKeyStates.leftPressed = true;
    } else if (key == GLFW_KEY_LEFT_SHIFT) {
        if (!mSpeedModifierActive) {
            mMovementSpeed *= 10.0f;
            mSpeedModifierActive = true;
        }
    }
}

void Camera::OnKeyReleased(int key) {
    if (key == GLFW_KEY_W) {
        mKeyStates.forwardPressed = false;
    } else if (key == GLFW_KEY_S) {
        mKeyStates.backwardsPressed = false;
    } else if (key == GLFW_KEY_D) {
        mKeyStates.rightPressed = false;
    } else if (key == GLFW_KEY_A) {
        mKeyStates.leftPressed = false;
    } else if (key == GLFW_KEY_LEFT_SHIFT) {
        if (mSpeedModifierActive) {
            mMovementSpeed /= 10.0f;
            mSpeedModifierActive = false;
        }
    } else if (key == GLFW_KEY_P) {
        mFreezeFrustumUpdates = !mFreezeFrustumUpdates;
    }
}

void Camera::OnMouseMoved(glm::vec2 newPos) {
    if (mActive) {
        glm::vec2 delta = newPos - mCurrentMousePos;
        mPitch -= delta.y * mRotationSpeed;
        mYaw += delta.x * mRotationSpeed;
    }
    mCurrentMousePos = newPos;
}

void Camera::OnLeftMouseButtonPressed() {
    mActive = !mActive;
    InputManager* inputManager = mWindow->GetInputManager();
    inputManager->SetCursorMode(
        mActive ? InputManager::CursorMode::Disabled : InputManager::CursorMode::Normal);
}

void Camera::OnLeftMouseButtonReleased() {}

void Camera::OnRightMouseButtonPressed() {}

void Camera::OnRightMouseButtonReleased() {}

Camera::~Camera() {
    InputManager* inputManager = mWindow->GetInputManager();
    inputManager->Unsuscribe(this);
}
}  // namespace VKRT
