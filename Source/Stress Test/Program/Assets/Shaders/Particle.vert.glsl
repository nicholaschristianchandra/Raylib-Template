#version 330

uniform mat4 viewProjection;
uniform vec4 cameraRight;
uniform vec4 cameraUp;
uniform vec4 params; // x: time, y: brightness
uniform vec4 lightColors[4];

out vec2 fragCorner;
out vec3 fragColor;

float Hash(uint n, uint salt)
{
    n = n * 747796405u + salt * 2891336453u;
    n = ((n >> ((n >> 28u) + 4u)) ^ n) * 277803737u;
    return float((n >> 22u) ^ n) / 4294967295.0;
}

// Fireflies, made from nothing but their number: each drifts on its own circle, in the colors of the four lights
void main()
{
    const vec2 corners[6] = vec2[6](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
    uint instance = uint(gl_InstanceID);
    vec2 corner = corners[gl_VertexID];
    float time = params.x;
    float radius = 1.0 + 13.0 * Hash(instance, 1u);
    float direction = Hash(instance, 2u) > 0.5 ? 1.0 : -1.0;
    float angle = Hash(instance, 3u) * 6.2831853 + time * (0.1 + 0.4 * Hash(instance, 4u)) * direction;
    float height = 0.2 + 3.5 * Hash(instance, 5u) + 0.3 * sin(time * 1.3 + Hash(instance, 6u) * 6.2831853);
    float size = 0.03 + 0.05 * Hash(instance, 7u);
    vec3 world = vec3(cos(angle) * radius, height, sin(angle) * radius);
    world += (corner.x * cameraRight.xyz + corner.y * cameraUp.xyz) * size;

    fragCorner = corner;
    fragColor = lightColors[gl_InstanceID % 4].rgb * params.y * (0.6 + 0.4 * sin(time * 4.0 + Hash(instance, 8u) * 40.0));
    gl_Position = viewProjection * vec4(world, 1.0);
}
