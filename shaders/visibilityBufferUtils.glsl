#ifndef VISIBILITY_BUFFER_UTILS_GLSL
#define VISIBILITY_BUFFER_UTILS_GLSL

#define DRAW_ID_BITS 24

uint encodeVBData(uint drawId, uint primitiveId) {
    return primitiveId << DRAW_ID_BITS | drawId;
}

uvec2 decodeVBData(uint encoded) {
    return uvec2(encoded & ((1 << DRAW_ID_BITS) - 1), encoded >> DRAW_ID_BITS);
}

// Barycentric coordinate and derivatives computation from: https://github.com/expenses/lighthugger
struct BarycentricDeriv {
    vec3 barycentricCoords;
    vec3 ddx;
    vec3 ddy;
};

BarycentricDeriv CalcFullBary(
    vec4 pt0,
    vec4 pt1,
    vec4 pt2,
    vec2 pixelNdc,
    vec2 winSize
) {
    BarycentricDeriv ret;

    vec3 invW = 1.0 / vec3(pt0.w, pt1.w, pt2.w);

    vec2 ndc0 = pt0.xy * invW.x;
    vec2 ndc1 = pt1.xy * invW.y;
    vec2 ndc2 = pt2.xy * invW.z;

    float invDet = 1.0 / (determinant(mat2x2(ndc2 - ndc1, ndc0 - ndc1)));
    ret.ddx = vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y)
        * invDet * invW;
    ret.ddy = vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x)
        * invDet * invW;
    float ddxSum = dot(ret.ddx, vec3(1, 1, 1));
    float ddySum = dot(ret.ddy, vec3(1, 1, 1));

    vec2 deltaVec = pixelNdc - ndc0;
    float interpInvW = invW.x + deltaVec.x * ddxSum + deltaVec.y * ddySum;
    float interpW = 1.0 / interpInvW;

    ret.barycentricCoords.x = interpW
        * (invW[0] + deltaVec.x * ret.ddx.x + deltaVec.y * ret.ddy.x);
    ret.barycentricCoords.y =
        interpW * (0.0f + deltaVec.x * ret.ddx.y + deltaVec.y * ret.ddy.y);
    ret.barycentricCoords.z =
        interpW * (0.0f + deltaVec.x * ret.ddx.z + deltaVec.y * ret.ddy.z);

    ret.ddx *= (2.0f / winSize.x);
    ret.ddy *= (2.0f / winSize.y);
    ddxSum *= (2.0f / winSize.x);
    ddySum *= (2.0f / winSize.y);

    ret.ddy *= -1.0f;
    ddySum *= -1.0f;

    float interpW_ddx = 1.0f / (interpInvW + ddxSum);
    float interpW_ddy = 1.0f / (interpInvW + ddySum);

    ret.ddx =
        interpW_ddx * (ret.barycentricCoords * interpInvW + ret.ddx) - ret.barycentricCoords;
    ret.ddy =
        interpW_ddy * (ret.barycentricCoords * interpInvW + ret.ddy) - ret.barycentricCoords;

    return ret;
}

vec2 interpolate(BarycentricDeriv deriv, vec2 v0, vec2 v1, vec2 v2) {
    return deriv.barycentricCoords.x * v0 +
           deriv.barycentricCoords.y * v1 +
           deriv.barycentricCoords.z * v2;
}

struct InterpolatedWithDerivsVec2 {
    vec2 value;
    vec2 ddx;
    vec2 ddy;
};

InterpolatedWithDerivsVec2 interpolateWithDerivs(BarycentricDeriv deriv, vec2 v0, vec2 v1, vec2 v2) {
    InterpolatedWithDerivsVec2 ret;
    ret.value = deriv.barycentricCoords.x * v0 + deriv.barycentricCoords.y * v1 + deriv.barycentricCoords.z * v2;
    ret.ddx   = deriv.ddx.x * v0 + deriv.ddx.y * v1 + deriv.ddx.z * v2;
    ret.ddy   = deriv.ddy.x * v0 + deriv.ddy.y * v1 + deriv.ddy.z * v2;
    return ret;
}

vec3 interpolate(BarycentricDeriv deriv, vec3 v0, vec3 v1, vec3 v2) {
    return deriv.barycentricCoords.x * v0 +
           deriv.barycentricCoords.y * v1 +
           deriv.barycentricCoords.z * v2;
}

struct InterpolatedWithDerivsVec3 {
    vec3 value;
    vec3 ddx;
    vec3 ddy;
};

InterpolatedWithDerivsVec3 interpolateWithDerivs(BarycentricDeriv deriv, vec3 v0, vec3 v1, vec3 v2) {
    InterpolatedWithDerivsVec3 ret;
    ret.value = deriv.barycentricCoords.x * v0 + deriv.barycentricCoords.y * v1 + deriv.barycentricCoords.z * v2;
    ret.ddx   = deriv.ddx.x * v0 + deriv.ddx.y * v1 + deriv.ddx.z * v2;
    ret.ddy   = deriv.ddy.x * v0 + deriv.ddy.y * v1 + deriv.ddy.z * v2;
    return ret;
}

vec4 interpolate(BarycentricDeriv deriv, vec4 v0, vec4 v1, vec4 v2) {
    return deriv.barycentricCoords.x * v0 +
           deriv.barycentricCoords.y * v1 +
           deriv.barycentricCoords.z * v2;
}

struct InterpolatedWithDerivsVec4 {
    vec4 value;
    vec4 ddx;
    vec4 ddy;
};

InterpolatedWithDerivsVec4 interpolateWithDerivs(BarycentricDeriv deriv, vec4 v0, vec4 v1, vec4 v2) {
    InterpolatedWithDerivsVec4 ret;
    ret.value = deriv.barycentricCoords.x * v0 + deriv.barycentricCoords.y * v1 + deriv.barycentricCoords.z * v2;
    ret.ddx   = deriv.ddx.x * v0 + deriv.ddx.y * v1 + deriv.ddx.z * v2;
    ret.ddy   = deriv.ddy.x * v0 + deriv.ddy.y * v1 + deriv.ddy.z * v2;
    return ret;
}

#endif