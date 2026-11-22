#version 330

in vec2 fragTexCoord;

uniform vec4 right;        // w: tan(fov / 2) * aspect
uniform vec4 up;           // w: tan(fov / 2)
uniform vec4 forward;
uniform vec4 sunDirection; // w: time

out vec4 finalColor;

float Hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    vec2 screen = vec2(fragTexCoord.x * 2.0 - 1.0, 1.0 - fragTexCoord.y * 2.0);
    vec3 direction = normalize(forward.xyz + screen.x * right.xyz * right.w + screen.y * up.xyz * up.w);
    vec3 sun = sunDirection.xyz;
    float day = smoothstep(-0.15, 0.25, sun.y);
    float sunset = clamp(1.0 - abs(sun.y) * 4.0, 0.0, 1.0);
    float height = clamp(direction.y, 0.0, 1.0);

    vec3 zenith = mix(vec3(0.004, 0.008, 0.03), vec3(0.15, 0.35, 0.8), day);
    vec3 horizon = mix(vec3(0.02, 0.03, 0.07), vec3(0.62, 0.75, 0.9), day);
    float towardSun = clamp(dot(normalize(vec3(direction.x, 0.0, direction.z)), normalize(vec3(sun.x, 0.0, sun.z))), 0.0, 1.0);
    horizon += vec3(0.55, 0.22, 0.06) * sunset * (0.4 + 0.6 * towardSun);
    vec3 color = mix(horizon, zenith, pow(height, 0.5));

    // Sun disk and glow, moon, stars twinkling at night
    float facing = clamp(dot(direction, sun), 0.0, 1.0);
    color += vec3(1.0, 0.9, 0.7) * (pow(facing, 3000.0) * 40.0 + pow(facing, 12.0) * 0.4 * (day + sunset));
    color += vec3(0.8, 0.85, 1.0) * smoothstep(0.9993, 0.9996, dot(direction, -sun)) * 2.0 * (1.0 - day);

    vec3 cell = floor(direction * 220.0);
    float star = step(0.996, Hash(cell)) * (0.6 + 0.4 * sin(sunDirection.w * 3.0 + Hash(cell + 7.0) * 40.0));
    color += star * (1.0 - day) * smoothstep(0.0, 0.15, direction.y) * 1.5;

    color = mix(color, horizon * 0.35, clamp(-direction.y * 4.0, 0.0, 1.0));
    finalColor = vec4(color, 1.0);
}
