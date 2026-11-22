#version 330
#include "Common.glsli"

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in mat4 instanceTransform; // for ducks drawn together: columns 0 and 1 hold the duck (see duckA and duckB)

uniform mat4 viewProjection;
uniform vec4 params; // x: 0 the ground, 1 ducks drawn together, 2 one duck per draw; y: time;
                     // w: 0 placed by the CPU, 1 spinning here, 2 animated here
uniform vec4 duckA;  // one duck per draw: x, y, z, scale
uniform vec4 duckB;  // and angle, spin, phase

out vec3 fragWorld;
out vec3 fragNormal;
out vec2 fragTexCoord;

vec3 RotateY(vec3 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

void main()
{
    vec3 world = vertexPosition;
    vec3 normal = vertexNormal;

    if (params.x > 0.5)
    {
        vec4 a = params.x > 1.5 ? duckA : instanceTransform[0];
        vec4 b = params.x > 1.5 ? duckB : instanceTransform[1];

        if (params.w > 1.5)
        {
            AnimateDuck(a, b, params.y);
        }

        float angle = b.x + (params.w > 0.5 && params.w < 1.5 ? params.y * b.y : 0.0);
        world = RotateY(world * a.w, angle) + a.xyz;
        normal = RotateY(normal, angle);
    }

    fragWorld = world;
    fragNormal = normal;
    fragTexCoord = vertexTexCoord;
    gl_Position = viewProjection * vec4(world, 1.0);
}
