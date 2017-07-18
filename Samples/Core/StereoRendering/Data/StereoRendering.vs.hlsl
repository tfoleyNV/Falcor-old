/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "VertexAttrib.h"
__import ShaderCommon;

#include "StereoRenderingCommon.h"

struct VS_IN
{
    float4 pos         : POSITION;
    float3 normal      : NORMAL;
    float3 bitangent   : BITANGENT;
#ifdef HAS_TEXCRD
    float2 texC        : TEXCOORD;
#endif
#ifdef HAS_LIGHTMAP_UV
    float2 lightmapC   : LIGHTMAP_UV;
#endif
#ifdef HAS_COLORS
    float3 color       : DIFFUSE_COLOR;
#endif
#ifdef _VERTEX_BLENDING
    float4 boneWeights : BONE_WEIGHTS;
    uint4  boneIds     : BONE_IDS;
#endif
    uint instanceID : SV_INSTANCEID;
};

float4x4 getWorldMat(VS_IN vIn)
{
#ifdef _VERTEX_BLENDING
    float4x4 worldMat = getBlendedWorldMat(vIn.boneWeights, vIn.boneIds);
#else
    float4x4 worldMat = gWorldMat[vIn.instanceID];
#endif
    return worldMat;
}

float3x3 getWorldInvTransposeMat(VS_IN vIn)
{
#ifdef _VERTEX_BLENDING
    float3x3 worldInvTransposeMat = getBlendedInvTransposeWorldMat(vIn.boneWeights, vIn.boneIds);
#else
    float3x3 worldInvTransposeMat = gWorldInvTransposeMat[vIn.instanceID];
#endif
    return worldInvTransposeMat;
}

VS_OUT main(VS_IN vIn)
{
    VS_OUT vOut;
    float4x4 worldMat = getWorldMat(vIn);
    float4 posW = mul(vIn.pos, worldMat);
    vOut.posW = posW.xyz;

#ifdef HAS_TEXCRD
    vOut.texC = vIn.texC;
#else
    vOut.texC = 0;
#endif

#ifdef HAS_COLORS
    vOut.colorV = vIn.color;
#else
    vOut.colorV = 0;
#endif

    vOut.normalW = mul(vIn.normal, getWorldInvTransposeMat(vIn)).xyz;
    vOut.bitangentW = mul(vIn.bitangent, (float3x3)worldMat).xyz;

    return vOut;
}
