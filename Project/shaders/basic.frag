#version 410 core
in vec3 fNormal;
in vec4 fPosEye;
in vec2 fTexCoords;
in vec4 fragPosLightSpace; 
in vec3 fWorldPos;

out vec4 fColor;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;
uniform samplerCube torchShadowMap;

uniform vec3 lightColor; 
uniform vec3 lightPos;   
uniform vec3 lightDir;   
uniform vec3 materialColor;
uniform int useTexture;
uniform float fogDensity = 0.1;
uniform vec3 fogColor = vec3(0.001, 0.001, 0.001);
uniform vec3 torchPos;
uniform vec3 torchColor;
uniform float far_plane = 150.0;  

float compute360Shadow(vec3 fragPos) {
    vec3 fragToLight = fWorldPos - torchPos;
    float closestDepth = texture(torchShadowMap, fragToLight).r;
    closestDepth *= far_plane;
    
    float currentDepth = length(fragToLight);
    float bias = 0.05; 
    return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

float computeFlashlightShadow()
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float bias = max(0.00005 * (1.0 - dot(normalize(fNormal), normalize(lightPos - fWorldPos))), 0.00001);

    return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

void main()
{
    vec3 objectColor = (useTexture == 1) ? texture(diffuseTexture, fTexCoords).rgb : materialColor;
    vec3 N = normalize(fNormal);
    
    // ------Flashlight------
    vec3 L_flash = normalize(lightPos - fWorldPos);
    float distF = length(lightPos - fWorldPos);
    vec3 lightToFrag = normalize(fWorldPos - lightPos);
    float theta = dot(lightToFrag, normalize(lightDir));
    float epsilon = cos(radians(15.0)) - cos(radians(17.0));
    float intensity = clamp((theta - cos(radians(17.0))) / epsilon, 0.0, 1.0);
    
    float diffF = max(dot(N, L_flash), 0.0);
    float attenF = 1.0 / (1.0 + 0.022 * distF + 0.0019 * distF * distF);
    float shadowF = computeFlashlightShadow(); // Use the fixed function
    
    vec3 flashlightResult = (diffF * (1.0 - shadowF)) * lightColor * intensity * attenF;
    
    // ------Torch------
    vec3 L_torch = normalize(torchPos - fWorldPos);
    float distT = length(torchPos - fWorldPos);
    float attenT = 1.0 / (1.0 + 0.045 * distT + 0.0075 * distT * distT);
    float diffT = max(dot(N, L_torch), 0.0);
    float shadowT = compute360Shadow(fWorldPos);
    
    vec3 torchResult = (diffT * (1.0 - shadowT)) * torchColor * attenT;
    
    // ------Flashlight + Torch------
    vec3 finalRGB = (flashlightResult + torchResult + vec3(0.007)) * objectColor;
    
    // ------Fog------
    float eyeDist = length(fPosEye.xyz); 
    float fogFactor = exp(-pow(eyeDist * fogDensity, 2.0));
    
    fColor = vec4(mix(fogColor, finalRGB, clamp(fogFactor, 0.0, 1.0)), 1.0);
}