#include "ShaderInterop_Renderer.h"
#include "ShaderInterop_Mesh.h"

struct VS_OUT
{
    float4 pos  : SV_POSITION;
    uint targetSlice : SV_RenderTargetArrayIndex;
    
};

struct PerLightData
{
    matrix view;
    matrix proj;
    //----
    float4 position;
    float3 color;
    float cutoffAngle;
    float3 direction;
    float strength;
};

struct PerDrawData
{
    matrix world;
    uint submeshID;
    uint materialID;
    uint jointsDescriptor;
};

struct JointsData
{
    matrix joints[300];
};

struct BlendWeight
{
    int idx;
    float weight;
};

struct Blend
{
    BlendWeight iw[4];
};

struct PushConstantElement
{
    uint gdDescriptor;
    uint perFrameOffset;
    
 
    uint perDrawLight;
    uint wireframe;
    uint smIdx;
    uint jointOffset;
};
ConstantBuffer<PushConstantElement> constants : register(b0, space0);

ConstantBuffer<PerDrawData> perDrawData : register(b1, space0);


VS_OUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VS_OUT output = (VS_OUT) 0;
    
    //ConstantBuffer<PerDrawData> perDrawData = ResourceDescriptorHeap[constants.perDrawCB];
    ConstantBuffer<PerLightData> perLightData = ResourceDescriptorHeap[constants.perDrawLight];
    
    StructuredBuffer<ShaderInterop_GlobalData> gds = ResourceDescriptorHeap[constants.gdDescriptor];
    ShaderInterop_GlobalData gd = gds[0];
    
    StructuredBuffer<ShaderInterop_PerFrameData> pfDatas = ResourceDescriptorHeap[gd.perFrameTable];
    ShaderInterop_PerFrameData pfData = pfDatas[constants.perFrameOffset];
    
    StructuredBuffer<ShaderInterop_SubmeshMetadata> mds = ResourceDescriptorHeap[gd.meshTableSubmeshMD];
    StructuredBuffer<float3> positions = ResourceDescriptorHeap[gd.meshTablePos];
    StructuredBuffer<Blend> blendData = ResourceDescriptorHeap[gd.meshTableBlend];

    ShaderInterop_SubmeshMetadata md = mds[perDrawData.submeshID];
    Blend bw = blendData[vertexID + md.blendStart];
    vertexID += md.vertStart;

    float3 pos = positions[vertexID];
    
    if (md.blendCount > 0)
    {
        ConstantBuffer<JointsData> jointsData = ResourceDescriptorHeap[perDrawData.jointsDescriptor];
        
        matrix mat = jointsData.joints[bw.iw[0].idx + constants.jointOffset] * bw.iw[0].weight;
        mat += jointsData.joints[bw.iw[1].idx + constants.jointOffset] * bw.iw[1].weight;
        mat += jointsData.joints[bw.iw[2].idx + constants.jointOffset] * bw.iw[2].weight;
        mat += jointsData.joints[bw.iw[3].idx + constants.jointOffset] * bw.iw[3].weight;
        pos = (float3) mul(float4(pos, 1.0f), mat);
    }
    
    float3 wsPos = mul(perDrawData.world, float4(pos, 1.f)).xyz;
    output.pos = mul(perLightData.proj, mul(perLightData.view, float4(wsPos, 1.f)));
    output.targetSlice = constants.smIdx;
    
    return output;
}