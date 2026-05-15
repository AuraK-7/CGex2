#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
layout (location = 3) in vec3 aBitangent;
layout (location = 4) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

out VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
    mat3 tbn;
    vec4 fragPosLightSpace;
} vsOut;

void main()
{
    vec4 worldPosition = uModel * vec4(aPos, 1.0);
    vsOut.worldPos = worldPosition.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(uModel)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    B = normalize(cross(N, T));
    vsOut.normal = N;
    vsOut.tbn = mat3(T, B, N);

    vsOut.texCoord = aTexCoord;
    vsOut.fragPosLightSpace = uLightSpaceMatrix * worldPosition;

    gl_Position = uProjection * uView * worldPosition;
}
