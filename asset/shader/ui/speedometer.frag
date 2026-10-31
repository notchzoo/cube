#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uText;
uniform vec3 uPanel;
void main()
{
    vec2 q=abs((vUV-0.5)*uPanel.xy)-(uPanel.xy*0.5-vec2(uPanel.z));
    float distance=length(max(q,0.0))+min(max(q.x,q.y),0.0)-uPanel.z;
    float edge=max(fwidth(distance),0.5);
    float mask=1.0-smoothstep(-edge,edge,distance);
    float border=mask-(1.0-smoothstep(-edge,edge,distance+1.0));
    float text=texture(uText,vUV).r;
    vec3 textColor=(vUV.y>0.33 && vUV.x<0.70)?vec3(0.38,0.91,0.96):vec3(0.83,0.89,0.93);
    vec3 background=mix(vec3(0.025,0.045,0.065),vec3(0.18,0.40,0.44),border*0.6);
    float alpha=text+0.82*(1.0-text);
    vec3 color=(textColor*text+background*0.82*(1.0-text))/max(alpha,0.001);
    fragColor=vec4(color,alpha*mask);
}
