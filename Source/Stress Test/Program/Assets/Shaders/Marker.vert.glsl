#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 viewProjection;
uniform vec4 lightColors[4];

out vec3 fragColor;

// A small glowing sphere at each light
void main()
{
    fragColor = lightColors[gl_InstanceID].rgb * (3.0 + 3.0 * clamp(vertexNormal.y, 0.0, 1.0));
    gl_Position = viewProjection * (instanceTransform * vec4(vertexPosition, 1.0));
}
