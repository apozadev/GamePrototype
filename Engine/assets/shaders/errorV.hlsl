float4 main(float3 aPosition : POSITION) : SV_Position
{
    return float4(aPosition, 1.0);
}
