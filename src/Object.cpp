#include "Object.h"

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "DebugUtils.h"
#include "Scene.h"

namespace VKRT {

Object::Object()
    : mLocalTransform(1.0f),
      mAbsoluteTranform(1.0f),
      mPosition(0.0f),
      mRotation(glm::vec3(0.0f)),
      mScale(1.0f, 1.0f, 1.0f) {}

void Object::SetTranslation(const glm::vec3& position) {
    mPosition = position;
}

void Object::Translate(const glm::vec3& delta) {
    mPosition += delta;
}

void Object::Rotate(const glm::vec3& delta) {
    mRotation *= glm::quat(glm::radians(delta));
}

void Object::SetRotation(const glm::quat& rotation) {
    mRotation = rotation;
}

void Object::Scale(const glm::vec3& delta) {
    mScale += delta;
}

void Object::SetScale(const glm::vec3& scale) {
    mScale = scale;
}

void Object::UpdateTransforms(const glm::mat4& parentTransform) {
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), mPosition);
    glm::mat4 rotate = glm::toMat4(mRotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), mScale);
    mLocalTransform = translate * rotate * scale;
    mAbsoluteTranform = parentTransform * mLocalTransform;
    for (ScopedRefPtr<Object> child : mChildren) {
        child->UpdateTransforms(mAbsoluteTranform);
    }
}

void Object::AddChild(ScopedRefPtr<Object> child) {
    mChildren.push_back(child);
}

Object::~Object() {}

}  // namespace VKRT