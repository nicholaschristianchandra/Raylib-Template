#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform float threshold;

out vec4 finalColor;

// What glows: the light above the threshold, at half size
void main()
{
    vec3 color = texture(texture0, fragTexCoord).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    float keep = clamp((brightness - threshold) / max(brightness, 0.0001), 0.0, 1.0);
    finalColor = vec4(color * keep, 1.0);
}
