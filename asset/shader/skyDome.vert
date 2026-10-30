#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uView;         // view matrix with translation removed
uniform mat4 uProjection;

out vec3 vDirection;

void main()
{
    // The sphere is centered on the camera, so local position is world direction.
    vDirection = aPosition;

    vec4 clipPosition = uProjection * uView * vec4(aPosition, 1.0);
    gl_Position = clipPosition.xyww;    // force far depth (z / w = 1)
}