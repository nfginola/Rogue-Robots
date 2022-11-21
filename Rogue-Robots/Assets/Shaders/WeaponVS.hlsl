#include "ShaderInterop_Renderer.h"
#include "ShaderInterop_Mesh.h"

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 nor : NORMAL;
    float3 tan : TANGENT;
    float3 bitan : BITANGENT;
    float3 wsPos : WS_POSITION;
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

struct PerLightData
{
    matrix view;
    matrix proj;
    float4 position;
    float3 color;
    float cutoffAngle;
    float3 direction;
    float strength;
};

struct PushConstantElement
{
    uint gdDescriptor;
    uint perFrameOffset;
    
    uint spotlightArrayStructureIndex;
    uint shadowMapDepthIndex;
    uint wireframe;
    uint isLit;
    uint jointOffset;
};
ConstantBuffer<PushConstantElement> constants : register(b0, space0);

ConstantBuffer<PerDrawData> perDrawData : register(b1, space0);



VS_OUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VS_OUT output = (VS_OUT) 0;
    
    //ConstantBuffer<PerDrawData> perDrawData = ResourceDescriptorHeap[constants.perDrawCB];
    
    StructuredBuffer<ShaderInterop_GlobalData> gds = ResourceDescriptorHeap[constants.gdDescriptor];
    ShaderInterop_GlobalData gd = gds[0];
    
    StructuredBuffer<ShaderInterop_PerFrameData> pfDatas = ResourceDescriptorHeap[gd.perFrameTable];
    ShaderInterop_PerFrameData pfData = pfDatas[constants.perFrameOffset];
    
    StructuredBuffer<ShaderInterop_SubmeshMetadata> mds = ResourceDescriptorHeap[gd.meshTableSubmeshMD];
    StructuredBuffer<float3> positions = ResourceDescriptorHeap[gd.meshTablePos];
    StructuredBuffer<float2> uvs = ResourceDescriptorHeap[gd.meshTableUV];
    StructuredBuffer<float3> normals = ResourceDescriptorHeap[gd.meshTableNor];
    StructuredBuffer<float3> tangents = ResourceDescriptorHeap[gd.meshTableTan];
    StructuredBuffer<Blend> blendData = ResourceDescriptorHeap[gd.meshTableBlend];

    ShaderInterop_SubmeshMetadata md = mds[perDrawData.submeshID];
    //int offset = 65;
    Blend bw = blendData[vertexID + md.blendStart];
    vertexID += md.vertStart;

    float3 pos = positions[vertexID];
    float2 uv = uvs[vertexID];
    float3 nor = normals[vertexID];
    float3 tan = tangents[vertexID];
    
    int offset = constants.jointOffset;
    if (md.blendCount > 0)
    {
        ConstantBuffer<JointsData> jointsData = ResourceDescriptorHeap[perDrawData.jointsDescriptor];
        matrix mat = jointsData.joints[bw.iw[0].idx + offset] * bw.iw[0].weight;
        mat += jointsData.joints[bw.iw[1].idx + offset] * bw.iw[1].weight;
        mat += jointsData.joints[bw.iw[2].idx + offset] * bw.iw[2].weight;
        mat += jointsData.joints[bw.iw[3].idx + offset] * bw.iw[3].weight;
        pos = (float3) mul(float4(pos, 1.0f), mat);
    }
    
    ConstantBuffer<PerLightData> perLightData = ResourceDescriptorHeap[constants.spotlightArrayStructureIndex];
    
    output.wsPos = mul(perDrawData.world, float4(pos, 1.f)).xyz;
    output.pos = mul(pfData.projMatrix, mul(pfData.viewMatrix, float4(output.wsPos, 1.f)));
    
    // Closest
    // Remeber that we have reverse-z: Guaranteeing that the final NDC Z is == 1 is equal to putting it as close as possible
    output.pos.z = output.pos.w;
    
    output.nor = mul(perDrawData.world, float4(nor, 0.f)).xyz;
    output.tan = mul(perDrawData.world, float4(tan, 0.f)).xyz;
    output.bitan = normalize(cross(output.tan, output.nor));
    output.uv = uv;
 
    return output;
}