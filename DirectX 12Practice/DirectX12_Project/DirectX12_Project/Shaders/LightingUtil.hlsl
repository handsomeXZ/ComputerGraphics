// Defaults for number of lights.


#define MAX_LIGHTS 10

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float CalcAttenuation(float d, float falloffstart, float falloffend) {
	return saturate((falloffend - d)/(falloffend - falloffstart));
}

float3 SchlickFresnel(float3 FresnelR0, float3 normal, float3 lightvec) {
	float invAngle = 1.0f - saturate(dot(normal, lightvec));
    float3 reflectPercent = FresnelR0 + (1.0f - FresnelR0) * (pow(invAngle, 5.0f));
	return reflectPercent;
}

float3 BlinnPhong(float3 lightstrength, Material mat, float3 normal, float3 toEye, float3 lightvec) {
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightvec);

    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightvec);
    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, lightvec), 0.0f), m) / 8.0f;

    float3 specAlbedo = fresnelFactor * roughnessFactor;
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (specAlbedo + mat.DiffuseAlbedo.rgb) * lightstrength;
}

float3 ComputeDirectLight(Light L, Material Mat, float3 toEye, float3 normal) {
    float3 lightvec = -L.Direction;
    float ndotl = max(dot(lightvec, normal), 0.0f);

    float3 lightstrength = L.Strength * ndotl;

    return BlinnPhong(lightstrength, Mat, normal, toEye, lightvec);
}

float3 ComputePointLight(Light L, Material Mat, float3 pos, float3 toEye, float3 normal) {
    float3 lightvec = L.Position - pos;
    
    float d = length(lightvec);

    if (d > L.FalloffEnd) {
        return 0.0f;
    }

    lightvec /= d;
    float ndotl = max(dot(lightvec, normal), 0.0f);
    float3 lightstrength = ndotl * L.Strength;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightstrength *= att;
    return BlinnPhong(lightstrength, Mat, normal, toEye, lightvec);
}

float3 ComputeSpotLight(Light L, Material Mat, float3 pos, float3 toEye, float3 normal) {
    float3 lightvec = L.Position - pos;

    float d = length(lightvec);

    if (d > L.FalloffEnd) {
        return 0.0f;
    }

    lightvec /= d;
    float ndotl = max(dot(lightvec, normal), 0.0f);
    float3 lightstrength = ndotl * L.Strength;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightstrength *= att;

    lightstrength *= pow(max(dot(-lightvec, L.Direction), 0.0f), L.SpotPower);

    return BlinnPhong(lightstrength, Mat, normal, toEye, lightvec);
}

float4 ComputeLight(Light gLights[MAX_LIGHTS], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor) {
    
    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for (i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectLight(gLights[i], mat, toEye, normal);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS; i < NUM_POINT_LIGHTS + NUM_DIR_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, toEye, normal);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (i = NUM_POINT_LIGHTS + NUM_DIR_LIGHTS; i < NUM_POINT_LIGHTS + NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, toEye, normal);
    }
#endif

    return float4(result, 0.0f);
}