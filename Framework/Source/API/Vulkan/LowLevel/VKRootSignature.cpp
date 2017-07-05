/***************************************************************************
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
#include "Framework.h"
#include "API/LowLevel/RootSignature.h"
#include "API/Device.h"
#include <set>

namespace Falcor
{
    VkDescriptorType falcorToVkDescType(DescriptorPool::Type type);

    VkShaderStageFlags getShaderVisibility(ShaderVisibility visibility)
    {
        VkShaderStageFlags flags = 0;

        if ((visibility & ShaderVisibility::Vertex) != ShaderVisibility::None)
        {
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if ((visibility & ShaderVisibility::Pixel) != ShaderVisibility::None)
        {
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if ((visibility & ShaderVisibility::Geometry) != ShaderVisibility::None)
        {
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        if ((visibility & ShaderVisibility::Domain) != ShaderVisibility::None)
        {
            flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;;
        }
        if ((visibility & ShaderVisibility::Hull) != ShaderVisibility::None)
        {
            flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }

        return flags;
    }
    
    bool RootSignature::apiInit()
    {
        std::vector<VkDescriptorSetLayout> vkSetLayouts(mDesc.mSets.size());

        for (size_t set = 0 ; set < mDesc.mSets.size() ; set++)
        {
            const auto& layout = mDesc.mSets[set];
            std::vector<VkDescriptorSetLayoutBinding> bindings(layout.getRangeCount());

            for (uint32_t r = 0; r < layout.getRangeCount(); r++)
            {
                VkDescriptorSetLayoutBinding& b = bindings[r];
                const auto& range = layout.getRange(r);
                assert(range.regSpace == 0);
                b.binding = range.baseRegIndex;
                b.descriptorCount = range.descCount;
                b.descriptorType = falcorToVkDescType(range.type);
                b.pImmutableSamplers = nullptr;
                b.stageFlags = getShaderVisibility(layout.getVisibility());
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = (uint32_t)bindings.size();
            layoutInfo.pBindings = bindings.data();
            vk_call(vkCreateDescriptorSetLayout(gpDevice->getApiHandle(), &layoutInfo, nullptr, &vkSetLayouts[set]));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pSetLayouts = vkSetLayouts.data();
        pipelineLayoutInfo.setLayoutCount = (uint32_t)vkSetLayouts.size();
        vk_call(vkCreatePipelineLayout(gpDevice->getApiHandle(), &pipelineLayoutInfo, nullptr, &mApiHandle));

        return true;
    }

    struct Range
    {
        uint32_t baseIndex;
        uint32_t count;

        bool operator<(const Range& other) const { return baseIndex < other.baseIndex; }
    };
    using SetRangeMap = std::map<RootSignature::DescType, std::set<Range>>;
    using SetMap = std::map<uint32_t, SetRangeMap>;

    struct ResData
    {
        RootSignature::DescType type;
        uint32_t regSpace;
        uint32_t regIndex;
        uint32_t count;
    };

    static ResData getResData(const ProgramReflection::Resource& resource)
    {
        ResData data;
        if (resource.type == ProgramReflection::Resource::ResourceType::Sampler)
        {
            data.type = RootSignature::DescType::Sampler;
        }
        else
        {
            switch (resource.shaderAccess)
            {
            case ProgramReflection::ShaderAccess::ReadWrite:
                data.type = RootSignature::DescType::Uav;
                break;
            case ProgramReflection::ShaderAccess::Read:
                data.type = RootSignature::DescType::Srv;
                break;
            default:
                should_not_get_here();
            }
        }

        data.count = resource.arraySize ? resource.arraySize : 1;
        data.regIndex = resource.regIndex;
        data.regSpace = resource.registerSpace;
        return data;
    }

    void insertResData(SetMap& map, const ResData& data)
    {
        if (map.find(data.regSpace) == map.end())
        {
            map[data.regSpace] = {};
        }

        SetRangeMap& rangeMap = map[data.regSpace];
        if (rangeMap.find(data.type) == rangeMap.end())
        {
            rangeMap[data.type] = {};
        }

        rangeMap[data.type].insert({ data.regIndex, data.count });
    }

    std::vector<Range> mergeRanges(std::set<Range>& ranges)
    {
        std::vector<Range> merged;
        // The ranges are already sorted, we only need to check if we can merge the last entry with the current one

        for (const auto& r : ranges)
        {
            if (merged.size())
            {
                auto& back = merged.back();
                if (back.baseIndex + back.count + 1 == r.baseIndex)
                {
                    back.count += r.count;
                    continue;
                }
            }
            merged.push_back(r);
        }
        return merged;
    }

    ProgramReflection::ShaderAccess getRequiredShaderAccess(RootSignature::DescType type);

    static void insertBuffers(const ProgramReflection* pReflector, SetMap& setMap, ProgramReflection::BufferReflection::Type bufferType, RootSignature::DescType descType)
    {
        uint32_t cost = 0;
        const auto& bufMap = pReflector->getBufferMap(bufferType);
        for (const auto& buf : bufMap)
        {
            const ProgramReflection::BufferReflection* pBuffer = buf.second.get();
            if (pBuffer->getShaderAccess() == getRequiredShaderAccess(descType))
            {
                ResData resData;
                resData.count = 1;
                resData.regIndex = pBuffer->getRegisterIndex();
                resData.regSpace = pBuffer->getRegisterSpace();
                resData.type = descType;
                insertResData(setMap, resData);
            }
        }
    }

    RootSignature::SharedPtr RootSignature::create(const ProgramReflection* pReflector)
    {
        // We'd like to create an optimized signature (minimize the number of ranges per set). First, go over all of the resources and find packed ranges
        SetMap setMap;

        const auto& resMap = pReflector->getResourceMap();
        for (const auto& res : resMap)
        {
            const ProgramReflection::Resource& resource = res.second;
            ResData resData = getResData(resource);
            insertResData(setMap, resData);
        }

        insertBuffers(pReflector, setMap, ProgramReflection::BufferReflection::Type::Constant, RootSignature::DescType::Cbv);
        insertBuffers(pReflector, setMap, ProgramReflection::BufferReflection::Type::Structured, RootSignature::DescType::Srv);
        insertBuffers(pReflector, setMap, ProgramReflection::BufferReflection::Type::Structured, RootSignature::DescType::Uav);

        std::map<uint32_t, DescriptorSetLayout> setLayouts;
        // Merge all the ranges
        for (auto& s : setMap)
        {
            for (auto& r : s.second)
            {
                auto& merged = mergeRanges(r.second);

                for (const auto& range : merged)
                {
                    if (setLayouts.find(s.first) == setLayouts.end()) setLayouts[s.first] = {};
                    setLayouts[s.first].addRange(r.first, range.baseIndex, range.count, s.first);
                }
            }
        }

        Desc d;
        for (const auto& s : setLayouts)
        {
            d.addDescriptorSet(s.second);
        }
        return create(d);
    }
}
