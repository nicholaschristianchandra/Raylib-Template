#version 330
#include "Common.glsli"

in vec2 fragTexCoord;

uniform sampler2D texture0; // the HDR scene
uniform sampler2D bloom;
uniform float strength;

out vec4 finalColor;

// The HDR scene plus its glow, tone mapped to the screen
void main()
{
    vec3 color = texture(texture0, fragTexCoord).rgb + texture(bloom, fragTexCoord).rgb * strength;
    finalColor = vec4(ToScreen(color), 1.0);
}
