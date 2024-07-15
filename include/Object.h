#pragma once

#include <string>

#include "glm/glm.hpp"

#include "Mesh.h"

namespace VKRT {

class Object : public RefCountPtr {
public:
    static ScopedRefPtr<Object> Load(ScopedRefPtr<Context> context, const std::string& path);

    Object();

    const std::vector<ScopedRefPtr<Mesh>>& GetMeshes() const { return mMeshes; }
    void AddMesh(ScopedRefPtr<Mesh> mesh) { mMeshes.push_back(mesh); }
    const glm::mat4& GetTransform() const { return mLocalTransform; }
    const glm::mat4& GetAbsoluteTransform() const { return mAbsoluteTranform; }
    const std::vector<ScopedRefPtr<Object>>& GetChildren() const { return mChildren; }

    void SetTranslation(const glm::vec3& position);
    void Translate(const glm::vec3& delta);

    void Rotate(const glm::vec3& delta);
    void SetRotation(const glm::vec3& rotation);

    void Scale(const glm::vec3& delta);
    void SetScale(const glm::vec3& scale);

    void AddChild(ScopedRefPtr<Object> child);

    void UpdateTransforms(const glm::mat4& parentTransform);

    ~Object();

private:
    std::vector<ScopedRefPtr<Object>> mChildren;
    glm::mat4 mLocalTransform;
    glm::mat4 mAbsoluteTranform;

    std::vector<ScopedRefPtr<Mesh>> mMeshes;
    glm::vec3 mEulerRotation;
    glm::vec3 mScale;
    glm::vec3 mPosition;
};

}  // namespace VKRT