#version 330 core

out vec4 FragColor;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;

uniform int uPointLightsOn;

uniform sampler2D shadowMap;
uniform int uUseDirectionalShadow;
uniform vec2 uShadowTexelSize;
uniform float uExposure;
uniform vec3 uSceneMin;
uniform vec3 uSceneMax;
uniform int uApplyWindowFalloff;

in VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
    mat3 tbn;
    vec4 fragPosLightSpace;
} fsIn;

uniform bool uHasTexture;
uniform bool uHasNormalMap;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;
uniform vec3 uMaterialDiffuse;
uniform vec3 uViewPosition;

const float kShininess = 48.0;
const vec3 kSpecularAlbedo = vec3(0.055);

float attenuationPoly(float distance, float c, float kl, float kq)
{
    return 1.0 / (c + kl * distance + kq * (distance * distance));
}

float hash12(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float wallSpecAttenuation(vec3 n)
{
    float v = 1.0 - abs(n.y);
    v = smoothstep(0.1, 0.92, v);
    return mix(1.0, 0.18, v);
}

float directionalShadow(vec4 fragPosLightSpace, vec3 norm, vec3 lightDirToSun)
{
    if (uUseDirectionalShadow == 0) {
        return 1.0;
    }

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 1.0;
    }
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float ndotl = abs(dot(norm, lightDirToSun));
    float bias = max(0.0045 * (1.0 - ndotl), 0.0025);

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(float(x), float(y)) * uShadowTexelSize).r;
            shadow += projCoords.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return 1.0 - shadow;
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 baseColor, float shadow, float wallSpec)
{
    vec3 lightDir = normalize(-light.direction);
    float raw = max(dot(normal, lightDir), 0.0);
    float diff = pow(mix(0.12, 1.0, raw), 0.92);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), kShininess);

    vec3 ambient = light.ambient * baseColor;
    vec3 diffuse = light.diffuse * diff * baseColor;
    vec3 specular = light.specular * spec * kSpecularAlbedo * wallSpec;
    return ambient + shadow * (diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor, float wallSpec)
{
    vec3 toLight = light.position - fragPos;
    float distance = length(toLight);
    vec3 lightDir = normalize(toLight);

    float attenuation = attenuationPoly(distance, light.constant, light.linear, light.quadratic);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), kShininess);

    vec3 ambient = light.ambient * baseColor;
    vec3 diffuse = light.diffuse * diff * baseColor * attenuation;
    vec3 specular = light.specular * spec * kSpecularAlbedo * wallSpec * attenuation;
    return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor, float wallSpec)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), kShininess);

    float distance = length(light.position - fragPos);
    float attenuation = attenuationPoly(distance, light.constant, light.linear, light.quadratic);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient * baseColor;
    vec3 diffuse = light.diffuse * diff * baseColor * attenuation * intensity;
    vec3 specular = light.specular * spec * kSpecularAlbedo * wallSpec * attenuation * intensity;
    return ambient + diffuse + specular;
}

void main()
{
    vec3 baseColor = uMaterialDiffuse;
    if (uHasTexture) {
        baseColor = texture(texture_diffuse1, fsIn.texCoord).rgb;
    }

    vec3 norm = normalize(fsIn.normal);
    if (uHasNormalMap) {
        vec3 tangentNormal = texture(texture_normal1, fsIn.texCoord).xyz * 2.0 - 1.0;
        norm = normalize(fsIn.tbn * tangentNormal);
    }
    vec3 viewDir = normalize(uViewPosition - fsIn.worldPos);
    float wallSpec = wallSpecAttenuation(norm);

    float h = hash12(fsIn.worldPos.xz * 3.17 + fsIn.worldPos.y);
    float vertWall = smoothstep(0.22, 0.96, 1.0 - abs(norm.y));
    baseColor *= mix(vec3(1.0), vec3(0.97, 0.98, 1.0), vertWall * (0.5 + 0.5 * h) * 0.07);

    float lum = dot(baseColor, vec3(0.299, 0.587, 0.114));
    if (lum < 0.14 && vertWall > 0.25) {
        baseColor += (h - 0.5) * 0.018;
    }

    if (baseColor.r > 0.42 && baseColor.g < 0.22 && baseColor.b < 0.22) {
        baseColor = pow(max(baseColor, vec3(0.001)), vec3(0.9)) * vec3(0.9, 0.88, 0.88);
    }

    if (lum > 0.93 && vertWall > 0.2) {
        baseColor *= 0.92 + 0.06 * h;
    }

    vec3 towardSun = normalize(dirLight.direction);
    float dirShadow = directionalShadow(fsIn.fragPosLightSpace, norm, towardSun);

    vec3 windowSkylight = vec3(0.0);
    if (uApplyWindowFalloff == 1) {
        vec3 inc = -towardSun;
        float ext = max(abs(dot(uSceneMax - uSceneMin, inc)), 0.02);
        float tDep = dot(fsIn.worldPos - uSceneMin, inc) / ext;
        tDep = clamp(tDep, 0.0, 1.0);
        float nearBeam = clamp(1.0 - smoothstep(0.0, 0.55, tDep), 0.0, 1.0);
        float facing = max(dot(norm, inc), 0.0);
        float vert = smoothstep(0.18, 0.94, 1.0 - abs(norm.y));
        windowSkylight = vec3(0.42, 0.55, 0.68) * facing * vert * (nearBeam * nearBeam) * 0.22;
    }

    vec3 result = CalcDirLight(dirLight, norm, viewDir, baseColor, dirShadow, wallSpec);
    result += windowSkylight * baseColor;

    if (uApplyWindowFalloff == 1) {
        vec3 inc = -normalize(dirLight.direction);
        float ndotv = clamp(dot(norm, viewDir), 0.0, 1.0);
        float rim = pow(1.0 - ndotv, 2.4) * max(dot(norm, inc), 0.0);
        result += rim * baseColor * vec3(0.12, 0.13, 0.15);
    }

    if (uPointLightsOn == 1) {
        for (int i = 0; i < NR_POINT_LIGHTS; ++i) {
            result += CalcPointLight(pointLights[i], norm, fsIn.worldPos, viewDir, baseColor, wallSpec);
        }
    }

    result += CalcSpotLight(spotLight, norm, fsIn.worldPos, viewDir, baseColor, wallSpec);

    vec3 color = max(result * uExposure, vec3(0.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
