#version 330

in vec3 fragWorld;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D texture0;  // albedo
uniform sampler2D texture2;  // normal map
uniform sampler2D shadowMap; // depth, seen from the light

uniform mat4 lightViewProjection; // the shadow map's
uniform vec4 shadowParams;        // x: shadows on, y: one texel, z: which light casts them (0 sun, 1 moon)
uniform vec4 cameraPosition;
uniform vec4 sunDirection;
uniform vec4 sunColor;
uniform vec4 moonDirection;
uniform vec4 moonColor;
uniform vec4 ambient;
uniform vec4 fog;                 // rgb: the horizon's color, w: density
uniform vec4 lightPositions[4];   // w: range
uniform vec4 lightColors[4];      // w: intensity
uniform vec4 material;            // x: normal map on, y: specular strength, z: shininess, w: 1 for the ground

out vec4 finalColor;

vec3 Shade(vec3 albedo, vec3 normal, vec3 view, vec3 toLight, vec3 color)
{
    vec3 halfway = normalize(toLight + view);
    float diffuse = clamp(dot(normal, toLight), 0.0, 1.0);
    float specular = pow(clamp(dot(normal, halfway), 0.0, 1.0), material.z) * material.y * (diffuse > 0.0 ? 1.0 : 0.0);
    return (albedo * diffuse + specular) * color;
}

// 1 lit, 0 in shadow, softened over 3 x 3 texels
float Shadow(vec3 world)
{
    if (shadowParams.x < 0.5)
    {
        return 1.0;
    }

    vec4 clip = lightViewProjection * vec4(world, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float depth = ndc.z * 0.5 + 0.5 - 0.0015;

    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0 || depth > 1.0)
    {
        return 1.0;
    }

    float lit = 0.0;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            lit += depth <= texture(shadowMap, uv + vec2(float(x), float(y)) * shadowParams.y).r ? 1.0 : 0.0;
        }
    }

    return lit / 9.0;
}

void main()
{
    // The ground's texture follows the world, 2.5 meters per repeat
    vec2 uv = material.w > 0.5 ? fragWorld.xz * 0.4 : fragTexCoord;
    vec3 albedo = pow(texture(texture0, uv).rgb, vec3(2.2));
    vec3 normal = normalize(fragNormal);

    if (material.x > 0.5)
    {
        vec3 bumped = texture(texture2, uv).xyz * 2.0 - 1.0;
        normal = normalize(vec3(1.0, 0.0, 0.0) * bumped.x + vec3(0.0, 0.0, 1.0) * bumped.y + normal * bumped.z);
    }

    vec3 toCamera = cameraPosition.xyz - fragWorld;
    vec3 view = normalize(toCamera);
    float shadow = Shadow(fragWorld);
    vec3 color = albedo * ambient.rgb * (0.6 + 0.4 * normal.y);
    color += Shade(albedo, normal, view, sunDirection.xyz, sunColor.rgb) * (shadowParams.z < 0.5 ? shadow : 1.0);
    color += Shade(albedo, normal, view, moonDirection.xyz, moonColor.rgb) * (shadowParams.z > 0.5 ? shadow : 1.0);

    for (int i = 0; i < 4; i++)
    {
        vec3 toLight = lightPositions[i].xyz - fragWorld;
        float range = length(toLight);
        float window = clamp(1.0 - pow(range / lightPositions[i].w, 4.0), 0.0, 1.0);
        float attenuation = window * window / (1.0 + range * range);
        color += Shade(albedo, normal, view, toLight / range, lightColors[i].rgb * lightColors[i].w * attenuation);
    }

    float haze = 1.0 - exp(-length(toCamera) * fog.w);
    finalColor = vec4(mix(color, fog.rgb, haze), 1.0);
}
