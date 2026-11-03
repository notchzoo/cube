// Source GLB coordinates: nose -X, upper surface +Y, starboard +Z.
// All masks stay in mesh space, including when a rig surface is deflected.
float jetBand(float x, float lo, float hi, float aa)
{
    return smoothstep(lo-aa, lo+aa, x) * (1.0-smoothstep(hi-aa, hi+aa, x));
}
void su57Material(vec3 p, vec3 n, vec2 uv,
                  inout vec3 albedo, inout float metallic, inout float roughness)
{
    float aa = max(max(fwidth(p.x), fwidth(p.z)), 0.00012);
    if (uUseAlbedoMap == 1)
    {
        vec3 upper = pow(texture(uAlbedoTop, uv).rgb, vec3(2.2));
        vec3 lower = pow(texture(uAlbedoBottom, uv).rgb, vec3(2.2));
        float up = smoothstep(-0.25, 0.25, n.y);
        albedo = mix(lower, upper, up);
        // Side projection is confined to fuselage and fins: never wash out wing decals.
        float body = 1.0-smoothstep(0.10, 0.14, abs(p.z));
        float fin = smoothstep(0.11, 0.13, p.y);
        float side = smoothstep(0.35, 0.85, 1.0-abs(n.y))*max(body, fin);
        if (uUseAlbedoSide == 1)
        {
            vec2 sideUV = vec2(p.x*.85+.55, p.y*5.0+.05);
            albedo = mix(albedo, pow(texture(uAlbedoSide, sideUV).rgb, vec3(2.2)), side);
        }
    }
    else
        albedo = jetDigitalCamo(p,n);
    metallic = 0.03;
    roughness = 0.64;

    // Off-white dielectric radar nose with a sharp circumferential paint seam.
    float radome = 1.0-smoothstep(-.643-aa,-.643+aa,p.x);
    albedo = mix(albedo, pow(vec3(.79,.82,.79),vec3(2.2)),radome);
    roughness = mix(roughness,.48,radome);
    float seam = 1.0-smoothstep(.0003,.0003+aa,abs(p.x+.643));
    albedo *= 1.0-.26*seam;
    // Tail sting cap, distinct from the two nozzles.
    float sting = smoothstep(.365,.405,p.x)*(1.0-smoothstep(.018,.025,abs(p.z)));
    albedo = mix(albedo,pow(vec3(.47,.61,.63),vec3(2.2)),sting);

    // Cockpit glazing follows its raised shell, excluding the fuselage underneath.
    float cx = (p.x+.491)/.106;
    float canopyWidth = .027*sqrt(max(0.0,1.0-cx*cx));
    float canopy = jetBand(p.x,-.595,-.385,aa) *
                   smoothstep(.116,.126,p.y) *
                   (1.0-smoothstep(canopyWidth-aa,canopyWidth+aa,abs(p.z)));
    float frame = max(jetBand(p.x,-.552,-.548,aa),jetBand(p.x,-.397,-.393,aa));
    float glass = canopy*(1.0-frame);
    albedo = mix(albedo,vec3(.009,.027,.036),glass);
    metallic = mix(metallic,.58,glass);
    roughness = mix(roughness,.115,glass);
    albedo = mix(albedo,vec3(.105,.13,.13),canopy*frame);

    // The source mesh's nacelles lie around z=+/-0.055. Heat-stained titanium
    // sleeves lead into darker segmented exhaust petals, on both upper/lower faces.
    float engine = smoothstep(.155,.19,p.x) *
                   jetBand(abs(p.z),.024,.093,.002) *
                   (1.0-smoothstep(.128,.14,p.y));
    float exhaust = smoothstep(.317,.325,p.x);
    float ring = .5+.5*sin((p.x-.18)*960.0);
    float angle = atan(p.y-.058,abs(p.z)-.055);
    float petal = pow(.5+.5*cos(angle*18.0),14.0);
    float grain = hash21(floor(vec2(p.x*19000.0,angle*360.0)));
    vec3 titanium = mix(vec3(.19,.22,.24),vec3(.28,.235,.18),smoothstep(.20,.30,p.x));
    titanium *= .86+.14*ring;
    vec3 nozzle = mix(vec3(.075,.080,.086),vec3(.13,.12,.105),ring*.35);
    vec3 metalColor = mix(titanium,nozzle,exhaust)*(1.0-.32*petal)*(.96+.08*grain);
    albedo = mix(albedo,metalColor,engine);
    metallic = mix(metallic,.90,engine);
    roughness = mix(roughness,mix(.29,.43,exhaust)+petal*.1,engine);
    // Dark slots at intake lips under the forward wing roots.
    float intake = jetBand(p.x,-.32,-.27,.003)*jetBand(abs(p.z),.06,.10,.002)*
                   (1.0-smoothstep(.027,.044,p.y));
    albedo = mix(albedo,vec3(.009,.013,.016),intake);
    roughness = mix(roughness,.82,intake);
}
