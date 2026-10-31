#version 330 core
layout(location=0) in vec2 aCorner;
uniform mat4 uTransform;
out vec2 vUV;
void main()
{
    vUV=aCorner;
    gl_Position=uTransform*vec4(aCorner,0.0,1.0);
}
