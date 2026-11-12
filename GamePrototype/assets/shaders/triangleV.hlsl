cbuffer PushConstants : register(b0)
{
    float4x4 mvp;
};

struct VSInput
{
    float3 aPosition : POSITION;
};

struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // En tu GLSL realmente NO estás usando la mvp,
    // solo haces vec4(aPosition, 1.0)
    output.position = float4(input.aPosition, 1.0);

    return output;
}
