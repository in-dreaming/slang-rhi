StructuredBuffer<float> firstBuffer : register(t0, space3);
StructuredBuffer<float> secondBuffer : register(t0, space6);
RWStructuredBuffer<float> resultBuffer : register(u0, space7);

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    resultBuffer[dispatchThreadID.x] =
        firstBuffer[dispatchThreadID.x] + secondBuffer[dispatchThreadID.x];
}
