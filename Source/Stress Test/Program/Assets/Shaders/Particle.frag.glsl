#version 330

in vec2 fragCorner;
in vec3 fragColor;

out vec4 finalColor;

// A soft round glow, added to what is behind it
void main()
{
    float glow = clamp(1.0 - length(fragCorner), 0.0, 1.0);
    finalColor = vec4(fragColor * glow * glow, 1.0);
}
