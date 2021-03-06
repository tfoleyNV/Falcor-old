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
#pragma once
#include "Framework.h"
#include "ProgramReflection.h"
#include "Utils/StringUtils.h"

using namespace spire;

namespace Falcor
{
    ProgramReflection::SharedPtr ProgramReflection::create(ShaderReflection* pSpireReflector, std::string& log)
    {
        SharedPtr pReflection = SharedPtr(new ProgramReflection);
        return pReflection->init(pSpireReflector, log) ? pReflection : nullptr;
    }

    ProgramReflection::BindLocation ProgramReflection::getBufferBinding(const std::string& name) const
    {
        // Names are unique regardless of buffer type. Search in each map
        for (const auto& desc : mBuffers)
        {
            auto& it = desc.nameMap.find(name);
            if (it != desc.nameMap.end())
            {
                return it->second;
            }
        }

        static const BindLocation invalidBind(kInvalidLocation, ShaderAccess::Undefined);
        return invalidBind;
    }

    bool ProgramReflection::init(ShaderReflection* pSpireReflector, std::string& log)
    {
        bool b = true;
        b = b && reflectResources(          pSpireReflector, log);
        b = b && reflectVertexAttributes(   pSpireReflector, log);
        b = b && reflectPixelShaderOutputs( pSpireReflector, log);
        return b;
    }

    const ProgramReflection::Variable* ProgramReflection::BufferReflection::getVariableData(const std::string& name, size_t& offset) const
    {
        const std::string msg = "Error when getting variable data \"" + name + "\" from buffer \"" + mName + "\".\n";
        uint32_t arrayIndex = 0;
        offset = kInvalidLocation;

        // Look for the variable
        auto& var = mVariables.find(name);

#ifdef FALCOR_DX11
        if (var == mVariables.end())
        {
            // Textures might come from our struct. Try again.
            std::string texName = name + ".t";
            var = mVariables.find(texName);
        }
#endif
        if (var == mVariables.end())
        {
            // The name might contain an array index. Remove the last array index and search again
            std::string nameV2 = removeLastArrayIndex(name);
            var = mVariables.find(nameV2);

            if (var == mVariables.end())
            {
                logWarning(msg + "Variable not found.");
                return nullptr;
            }

            const auto& data = var->second;
            if (data.arraySize == 0)
            {
                // Not an array, so can't have an array index
                logError(msg + "Variable is not an array, so name can't include an array index.");
                return nullptr;
            }

            // We know we have an array index. Make sure it's in range
            std::string indexStr = name.substr(nameV2.length() + 1);
            char* pEndPtr;
            arrayIndex = strtol(indexStr.c_str(), &pEndPtr, 0);
            if (*pEndPtr != ']')
            {
                logError(msg + "Array index must be a literal number (no whitespace are allowed)");
                return nullptr;
            }

            if (arrayIndex >= data.arraySize)
            {
                logError(msg + "Array index (" + std::to_string(arrayIndex) + ") out-of-range. Array size == " + std::to_string(data.arraySize) + ".");
                return nullptr;
            }
        }

        const auto* pData = &var->second;
        offset = pData->location + pData->arrayStride * arrayIndex;
        return pData;
    }

    const ProgramReflection::Variable* ProgramReflection::BufferReflection::getVariableData(const std::string& name) const
    {
        size_t t;
        return getVariableData(name, t);
    }

    ProgramReflection::BufferReflection::SharedConstPtr ProgramReflection::getBufferDesc(uint32_t bindLocation, ShaderAccess shaderAccess, BufferReflection::Type bufferType) const
    {
        const auto& descMap = mBuffers[uint32_t(bufferType)].descMap;
        auto& desc = descMap.find({ bindLocation, shaderAccess });
        if (desc == descMap.end())
        {
            return nullptr;
        }
        return desc->second;
    }

    ProgramReflection::BufferReflection::SharedConstPtr ProgramReflection::getBufferDesc(const std::string& name, BufferReflection::Type bufferType) const
    {
        BindLocation bindLoc = getBufferBinding(name);
        if (bindLoc.regIndex != kInvalidLocation)
        {
            return getBufferDesc(bindLoc.regIndex, bindLoc.shaderAccess, bufferType);
        }
        return nullptr;
    }

    const ProgramReflection::Resource* ProgramReflection::BufferReflection::getResourceData(const std::string& name) const
    {
        auto& it = mResources.find(name);
        return it == mResources.end() ? nullptr : &(it->second);
    }

    ProgramReflection::BufferReflection::BufferReflection(const std::string& name, uint32_t registerIndex, uint32_t regSpace, Type type, StructuredType structuredType, size_t size, const VariableMap& varMap, const ResourceMap& resourceMap, ShaderAccess shaderAccess) :
        mName(name),
        mType(type),
        mStructuredType(structuredType),
        mSizeInBytes(size),
        mVariables(varMap),
        mResources(resourceMap),
        mRegIndex(registerIndex),
        mShaderAccess(shaderAccess)
    {
    }

    ProgramReflection::BufferReflection::SharedPtr ProgramReflection::BufferReflection::create(const std::string& name, uint32_t regIndex, uint32_t regSpace, Type type, StructuredType structuredType, size_t size, const VariableMap& varMap, const ResourceMap& resourceMap, ShaderAccess shaderAccess)
    {
        assert(regSpace == 0);
        return SharedPtr(new BufferReflection(name, regIndex, regSpace, type, structuredType, size, varMap, resourceMap, shaderAccess));
    }

    const ProgramReflection::Variable* ProgramReflection::getVertexAttribute(const std::string& name) const
    {
        const auto& it = mVertAttr.find(name);
        return (it == mVertAttr.end()) ? nullptr : &(it->second);
    }

    const ProgramReflection::Variable* ProgramReflection::getFragmentOutput(const std::string& name) const
    {
        const auto& it = mFragOut.find(name);
        return (it == mFragOut.end()) ? nullptr : &(it->second);
    }

    const ProgramReflection::Resource* ProgramReflection::getResourceDesc(const std::string& name) const
    {
        const auto& it = mResources.find(name);
        const ProgramReflection::Resource* pRes = (it == mResources.end()) ? nullptr : &(it->second);

        if (pRes == nullptr)
        {
            // Check if this is the internal struct
#ifdef FALCOR_D3D
            const auto& it = mResources.find(name + ".t");
            pRes = (it == mResources.end()) ? nullptr : &(it->second);
#endif
            if(pRes == nullptr)
            {
                logWarning("Can't find resource '" + name + "' in program");
            }
        }
        return pRes;
    }

    void ProgramReflection::getThreadGroupSize(
        uint32_t* outX,
        uint32_t* outY,
        uint32_t* outZ) const
    {
        if(outX) *outX = mThreadGroupSizeX;
        if(outX) *outY = mThreadGroupSizeY;
        if(outX) *outZ = mThreadGroupSizeZ;
    }

    /************************************************************************/
    /*  SPIRE Reflection                                                    */
    /************************************************************************/
    ProgramReflection::Variable::Type getVariableType(TypeReflection::ScalarType spireScalarType, uint32_t rows, uint32_t columns)
    {
        switch (spireScalarType)
        {
        case TypeReflection::ScalarType::None:
            // This isn't a scalar/matrix/vector, so it can't
            // be encoded in the `enum` that Falcor provides.
            return ProgramReflection::Variable::Type::Unknown;

        case TypeReflection::ScalarType::Bool:
            assert(rows == 1);
            switch (columns)
            {
            case 1:
                return ProgramReflection::Variable::Type::Bool;
            case 2:
                return ProgramReflection::Variable::Type::Bool2;
            case 3:
                return ProgramReflection::Variable::Type::Bool3;
            case 4:
                return ProgramReflection::Variable::Type::Bool4;
            }
        case TypeReflection::ScalarType::UInt32:
            assert(rows == 1);
            switch (columns)
            {
            case 1:
                return ProgramReflection::Variable::Type::Uint;
            case 2:
                return ProgramReflection::Variable::Type::Uint2;
            case 3:
                return ProgramReflection::Variable::Type::Uint3;
            case 4:
                return ProgramReflection::Variable::Type::Uint4;
            }
        case TypeReflection::ScalarType::Int32:
            assert(rows == 1);
            switch (columns)
            {
            case 1:
                return ProgramReflection::Variable::Type::Int;
            case 2:
                return ProgramReflection::Variable::Type::Int2;
            case 3:
                return ProgramReflection::Variable::Type::Int3;
            case 4:
                return ProgramReflection::Variable::Type::Int4;
            }
        case TypeReflection::ScalarType::Float32:
            switch (rows)
            {
            case 1:
                switch (columns)
                {
                case 1:
                    return ProgramReflection::Variable::Type::Float;
                case 2:
                    return ProgramReflection::Variable::Type::Float2;
                case 3:
                    return ProgramReflection::Variable::Type::Float3;
                case 4:
                    return ProgramReflection::Variable::Type::Float4;
                }
                break;
            case 2:
                switch (columns)
                {
                case 2:
                    return ProgramReflection::Variable::Type::Float2x2;
                case 3:
                    return ProgramReflection::Variable::Type::Float2x3;
                case 4:
                    return ProgramReflection::Variable::Type::Float2x4;
                }
                break;
            case 3:
                switch (columns)
                {
                case 2:
                    return ProgramReflection::Variable::Type::Float3x2;
                case 3:
                    return ProgramReflection::Variable::Type::Float3x3;
                case 4:
                    return ProgramReflection::Variable::Type::Float3x4;
                }
                break;
            case 4:
                switch (columns)
                {
                case 2:
                    return ProgramReflection::Variable::Type::Float4x2;
                case 3:
                    return ProgramReflection::Variable::Type::Float4x3;
                case 4:
                    return ProgramReflection::Variable::Type::Float4x4;
                }
                break;
            }
        }

        should_not_get_here();
        return ProgramReflection::Variable::Type::Unknown;
    }

    size_t getRowCountFromType(ProgramReflection::Variable::Type type)
    {
        switch (type)
        {
        case ProgramReflection::Variable::Type::Unknown:
            return 0;
        case ProgramReflection::Variable::Type::Bool:
        case ProgramReflection::Variable::Type::Bool2:
        case ProgramReflection::Variable::Type::Bool3:
        case ProgramReflection::Variable::Type::Bool4:
        case ProgramReflection::Variable::Type::Uint:
        case ProgramReflection::Variable::Type::Uint2:
        case ProgramReflection::Variable::Type::Uint3:
        case ProgramReflection::Variable::Type::Uint4:
        case ProgramReflection::Variable::Type::Int:
        case ProgramReflection::Variable::Type::Int2:
        case ProgramReflection::Variable::Type::Int3:
        case ProgramReflection::Variable::Type::Int4:
        case ProgramReflection::Variable::Type::Float:
        case ProgramReflection::Variable::Type::Float2:
        case ProgramReflection::Variable::Type::Float3:
        case ProgramReflection::Variable::Type::Float4:
            return 1;
        case ProgramReflection::Variable::Type::Float2x2:
        case ProgramReflection::Variable::Type::Float2x3:
        case ProgramReflection::Variable::Type::Float2x4:
            return 2;
        case ProgramReflection::Variable::Type::Float3x2:
        case ProgramReflection::Variable::Type::Float3x3:
        case ProgramReflection::Variable::Type::Float3x4:
            return 3;
        case ProgramReflection::Variable::Type::Float4x2:
        case ProgramReflection::Variable::Type::Float4x3:
        case ProgramReflection::Variable::Type::Float4x4:
            return 4;
        default:
            should_not_get_here();
            return 0;
        }
    }

    size_t getBytesPerVarType(ProgramReflection::Variable::Type type)
    {
        switch (type)
        {
        case ProgramReflection::Variable::Type::Unknown:
            return 0;
        case ProgramReflection::Variable::Type::Bool:
        case ProgramReflection::Variable::Type::Uint:
        case ProgramReflection::Variable::Type::Int:
        case ProgramReflection::Variable::Type::Float:
            return 4;
        case ProgramReflection::Variable::Type::Bool2:
        case ProgramReflection::Variable::Type::Uint2:
        case ProgramReflection::Variable::Type::Int2:
        case ProgramReflection::Variable::Type::Float2:
            return 8;
        case ProgramReflection::Variable::Type::Bool3:
        case ProgramReflection::Variable::Type::Uint3:
        case ProgramReflection::Variable::Type::Int3:
        case ProgramReflection::Variable::Type::Float3:
            return 12;
        case ProgramReflection::Variable::Type::Bool4:
        case ProgramReflection::Variable::Type::Uint4:
        case ProgramReflection::Variable::Type::Int4:
        case ProgramReflection::Variable::Type::Float4:
        case ProgramReflection::Variable::Type::Float2x2:
            return 16;
        case ProgramReflection::Variable::Type::Float2x3:
        case ProgramReflection::Variable::Type::Float3x2:
            return 24;
        case ProgramReflection::Variable::Type::Float2x4:
        case ProgramReflection::Variable::Type::Float4x2:
            return 32;
        case ProgramReflection::Variable::Type::Float3x3:
            return 36;
        case ProgramReflection::Variable::Type::Float3x4:
        case ProgramReflection::Variable::Type::Float4x3:
            return 48;
        case ProgramReflection::Variable::Type::Float4x4:
            return 64;
        default:
            should_not_get_here();
            return 0;
        }

    }

    // Information we need to track when converting Spire reflection
    // information over to the Falcor equivalent
    struct ReflectionGenerationContext
    {
        ProgramReflection*  pReflector = nullptr;
        ProgramReflection::VariableMap* pVariables = nullptr;
        ProgramReflection::ResourceMap* pResourceMap = nullptr;
        std::string* pLog = nullptr;

        ProgramReflection::VariableMap& getVariableMap() { return *pVariables; }
        ProgramReflection::ResourceMap& getResourceMap() { return *pResourceMap; }
        std::string& getLog() { return *pLog; }
    };

    // Represents a "breadcrumb trail" leading from a particular variable
    // back to the path over member-access and array-indexing operations
    // that led to it.
    // E.g., when trying to construct information for `foo.bar[3].baz`
    // we might have a path that consists of:
    //
    // - An entry for the field `baz` in type `Bar` (which knows its offset)
    // - An entry for element 3 in the type `Bar[]`
    // - An entry for the field `bar` in type `Foo`
    // - An entry for the top-level shader parameter `foo`
    //
    // To compute the correct offset for `baz` we can walk up this chain
    // and add up offsets (taking element stride into account for arrays).
    //
    // In simple cases, one can track this info top-down, by simply keeping
    // a "running total" offset, but that doesn't account for the fact that
    // `baz` might be a texture, UAV, sampler, or uniform, and the offset
    // we'd need to track for each case is different.
    struct ReflectionPath
    {
        ReflectionPath*                     parent = nullptr;
        VariableLayoutReflection*    var = nullptr;
        TypeLayoutReflection*        typeLayout = nullptr;
        uint32_t                            childIndex = 0;
    };

    // Once we've found the path from the root down to a particular leaf
    // variable, `getBindingIndex` can be used to find the final summed-up
    // index (or byte offset, in the case of a uniform).
    uint32_t getBindingIndex(ReflectionPath* path, SpireParameterCategory category)
    {
        uint32_t offset = 0;
        for (auto pp = path; pp; pp = pp->parent)
        {
            if (pp->var)
            {
                offset += (uint32_t)pp->var->getOffset(category);
                continue;
            }
            else if (pp->typeLayout)
            {
                switch (pp->typeLayout->getKind())
                {
                case TypeReflection::Kind::Array:
                    offset += (uint32_t)pp->typeLayout->getElementStride(category) * pp->childIndex;
                    continue;

                case TypeReflection::Kind::Struct:
                    offset += (uint32_t)pp->typeLayout->getFieldByIndex(int(pp->childIndex))->getOffset(category);
                    continue;

                default:
                    break;
                }
            }

            logError("internal error: invalid reflection path");
            return 0;
        }
        return offset;
    }

    size_t getUniformOffset(ReflectionPath* path)
    {
        return getBindingIndex(path, SPIRE_PARAMETER_CATEGORY_UNIFORM);
    }

    uint32_t getBindingSpace(ReflectionPath* path, SpireParameterCategory category)
    {
        // TODO: implement
        return 0;
    }

    static ProgramReflection::Resource::ResourceType getResourceType(TypeReflection* pSpireType)
    {
        switch (pSpireType->unwrapArray()->getKind())
        {
        case TypeReflection::Kind::SamplerState:
            return ProgramReflection::Resource::ResourceType::Sampler;

        case TypeReflection::Kind::Resource:
            switch (pSpireType->getResourceShape() & SPIRE_RESOURCE_BASE_SHAPE_MASK)
            {
            case SPIRE_STRUCTURED_BUFFER:
                return ProgramReflection::Resource::ResourceType::StructuredBuffer;

            case SPIRE_BYTE_ADDRESS_BUFFER:
                return ProgramReflection::Resource::ResourceType::RawBuffer;

            default:
                return ProgramReflection::Resource::ResourceType::Texture;
            }
            break;

        default:
            break;
        }
        should_not_get_here();
        return ProgramReflection::Resource::ResourceType::Unknown;
    }

    static ProgramReflection::ShaderAccess getShaderAccess(TypeReflection* pSpireType)
    {
        // Compute access for an array using the underlying type...
        pSpireType = pSpireType->unwrapArray();

        switch (pSpireType->getKind())
        {
        case TypeReflection::Kind::SamplerState:
        case TypeReflection::Kind::ConstantBuffer:
            return ProgramReflection::ShaderAccess::Read;
            break;

        case TypeReflection::Kind::Resource:
            switch (pSpireType->getResourceAccess())
            {
            case SPIRE_RESOURCE_ACCESS_NONE:
                return ProgramReflection::ShaderAccess::Undefined;

            case SPIRE_RESOURCE_ACCESS_READ:
                return ProgramReflection::ShaderAccess::Read;

            default:
                return ProgramReflection::ShaderAccess::ReadWrite;
            }
            break;

        default:
            should_not_get_here();
            return ProgramReflection::ShaderAccess::Undefined;
        }
    }

    static ProgramReflection::Resource::ReturnType getReturnType(TypeReflection* pType)
    {
        // Could be a resource that doesn't have a specific element type (e.g., a raw buffer)
        if (!pType)
            return ProgramReflection::Resource::ReturnType::Unknown;

        switch (pType->getScalarType())
        {
        case TypeReflection::ScalarType::Float32:
            return ProgramReflection::Resource::ReturnType::Float;
        case TypeReflection::ScalarType::Int32:
            return ProgramReflection::Resource::ReturnType::Int;
        case TypeReflection::ScalarType::UInt32:
            return ProgramReflection::Resource::ReturnType::Uint;
        case TypeReflection::ScalarType::Float64:
            return ProgramReflection::Resource::ReturnType::Double;

            // Could be a resource that uses an aggregate element type (e.g., a structured buffer)
        case TypeReflection::ScalarType::None:
            return ProgramReflection::Resource::ReturnType::Unknown;

        default:
            should_not_get_here();
            return ProgramReflection::Resource::ReturnType::Unknown;
        }
    }

    static ProgramReflection::Resource::Dimensions getResourceDimensions(SpireResourceShape shape)
    {
        switch (shape)
        {
        case SPIRE_TEXTURE_1D:
            return ProgramReflection::Resource::Dimensions::Texture1D;
        case SPIRE_TEXTURE_1D_ARRAY:
            return ProgramReflection::Resource::Dimensions::Texture1DArray;
        case SPIRE_TEXTURE_2D:
            return ProgramReflection::Resource::Dimensions::Texture2D;
        case SPIRE_TEXTURE_2D_ARRAY:
            return ProgramReflection::Resource::Dimensions::Texture2DArray;
        case SPIRE_TEXTURE_2D_MULTISAMPLE:
            return ProgramReflection::Resource::Dimensions::Texture2DMS;
        case SPIRE_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return ProgramReflection::Resource::Dimensions::Texture2DMSArray;
        case SPIRE_TEXTURE_3D:
            return ProgramReflection::Resource::Dimensions::Texture3D;
        case SPIRE_TEXTURE_CUBE:
            return ProgramReflection::Resource::Dimensions::TextureCube;
        case SPIRE_TEXTURE_CUBE_ARRAY:
            return ProgramReflection::Resource::Dimensions::TextureCubeArray;

        case SPIRE_TEXTURE_BUFFER:
        case SPIRE_STRUCTURED_BUFFER:
        case SPIRE_BYTE_ADDRESS_BUFFER:
            return ProgramReflection::Resource::Dimensions::Buffer;

        default:
            should_not_get_here();
            return ProgramReflection::Resource::Dimensions::Unknown;
        }
    }

    static bool verifyResourceDefinition(const ProgramReflection::Resource& prev, ProgramReflection::Resource& current, std::string& log)
    {
        bool match = true;
#define error_msg(msg_) std::string(msg_) + " mismatch.\n";
#define test_field(field_)                                           \
            if(prev.field_ != current.field_)                        \
            {                                                        \
                log += error_msg(#field_)                            \
                match = false;                                       \
            }

        test_field(type);
        test_field(dims);
        test_field(retType);
        test_field(regIndex);
        test_field(registerSpace);
        test_field(arraySize);
#undef test_field
#undef error_msg

        return match;
    }

    static bool reflectStructuredBuffer(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 path);

    static bool reflectConstantBuffer(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 path);


    // Generate reflection data for a single variable
    static bool reflectResource(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 path)
    {
        auto resourceType = getResourceType(pSpireType->getType());
        if (resourceType == ProgramReflection::Resource::ResourceType::StructuredBuffer)
        {
            // reflect this parameter as a buffer
            return reflectStructuredBuffer(
                pContext,
                pSpireType,
                name,
                path);
        }

        ProgramReflection::Resource falcorDesc;
        falcorDesc.type = resourceType;
        falcorDesc.shaderAccess = getShaderAccess(pSpireType->getType());
        if (resourceType == ProgramReflection::Resource::ResourceType::Texture)
        {
            falcorDesc.retType = getReturnType(pSpireType->getResourceResultType());
            falcorDesc.dims = getResourceDimensions(pSpireType->getResourceShape());
        }
        bool isArray = pSpireType->isArray();
        falcorDesc.regIndex = (uint32_t)getBindingIndex(path, pSpireType->getParameterCategory());
        falcorDesc.registerSpace = (uint32_t)getBindingSpace(path, pSpireType->getParameterCategory());
        assert(falcorDesc.registerSpace == 0);
        falcorDesc.arraySize = isArray ? (uint32_t)pSpireType->getTotalArrayElementCount() : 0;

        // If this already exists, definitions should match
        auto& resourceMap = *pContext->pResourceMap;
        const auto& prevDef = resourceMap.find(name);
        if (prevDef == resourceMap.end())
        {
            resourceMap[name] = falcorDesc;
        }
        else
        {
            std::string varLog;
            if (verifyResourceDefinition(prevDef->second, falcorDesc, varLog) == false)
            {
                pContext->getLog() += "Shader resource '" + std::string(name) + "' has different definitions between different shader stages. " + varLog;
                return false;
            }
        }

        // For now, we expose all resources as visible to all stages
        resourceMap[name].shaderMask = 0xFFFFFFFF;

        return true;
    }

    static void reflectType(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 pPath)
    {
        size_t uniformSize = pSpireType->getSize();

        // For any variable that actually occupies space in
        // uniform data, we want to add an entry to
        // the variables map that can be directly queried:
        if (uniformSize != 0)
        {
            ProgramReflection::Variable desc;

            desc.location = getUniformOffset(pPath);
            desc.type = getVariableType(
                pSpireType->getScalarType(),
                pSpireType->getRowCount(),
                pSpireType->getColumnCount());

            switch (pSpireType->getKind())
            {
            default:
                break;

            case TypeReflection::Kind::Array:
                desc.arraySize = (uint32_t)pSpireType->getElementCount();
                desc.arrayStride = (uint32_t)pSpireType->getElementStride(SPIRE_PARAMETER_CATEGORY_UNIFORM);
                break;

            case TypeReflection::Kind::Matrix:
                // TODO(tfoley): Spire needs to report this information!
                //                desc.isRowMajor = (typeDesc.Class == D3D_SVC_MATRIX_ROWS);
                break;
            }

            if (!pContext->pVariables)
            {
                logError("unimplemented: global-scope uniforms");
            }

            (*pContext->pVariables)[name] = desc;
        }

        // We want to reflect resource parameters as soon as we find a
        // type that is an (array of)* (sampler|texture|...)
        //
        // That is, we will look through any number of levels of array-ness
        // to see the type underneath:
        switch (pSpireType->unwrapArray()->getKind())
        {
        case TypeReflection::Kind::Struct:
            // A `struct` type obviously isn't a resource
            break;

            // TODO: If we ever start using arrays of constant buffers,
            // we'd probably want to handle them here too...

        default:
            // This might be a resource, or an array of resources.
            // To find out, let's ask what category of resource
            // it consumes.
            switch (pSpireType->getParameterCategory())
            {
            case ParameterCategory::ShaderResource:
            case ParameterCategory::UnorderedAccess:
            case ParameterCategory::SamplerState:
                // This is a resource, or an array of them (or an array of arrays ...)
                reflectResource(
                    pContext,
                    pSpireType,
                    name,
                    pPath);

                // We don't want to enumerate each individual field
                // as a separate entry in the resources map, so bail out here
                return;

            default:
                break;
            }
            break;
        }

        // If we didn't early exit in the resource case above, then we
        // will go ahead and recurse into the sub-elements of the type
        // (fields of a struct, elements of an array, etc.)
        switch (pSpireType->getKind())
        {
        default:
            // All the interesting cases for non-aggregate values
            // have been handled above.
            break;

        case TypeReflection::Kind::ConstantBuffer:
            // We've found a constant buffer, so reflect it as a top-level buffer
            reflectConstantBuffer(
                pContext,
                pSpireType,
                name,
                pPath);
            break;

        case TypeReflection::Kind::Array:
        {
            // For a variable with array type, we are going to create
            // entries for each element of the array.
            //
            // TODO: This probably isn't a good idea for very large
            // arrays, and obviously doesn't work for arrays that
            // aren't statically sized.
            //
            // TODO: we should probably also handle arrays-of-textures
            // and arrays-of-samplers specially here.

            auto elementCount = (uint32_t)pSpireType->getElementCount();
            TypeLayoutReflection* elementType = pSpireType->getElementTypeLayout();

            assert(name.size());

            for (uint32_t ee = 0; ee < elementCount; ++ee)
            {
                ReflectionPath elementPath;
                elementPath.parent = pPath;
                elementPath.typeLayout = pSpireType;
                elementPath.childIndex = ee;

                reflectType(pContext, elementType, name + '[' + std::to_string(ee) + "]", &elementPath);
            }
        }
        break;

        case TypeReflection::Kind::Struct:
        {
            // For a variable with structure type, we are going
            // to create entries for each field of the structure.
            //
            // TODO: it isn't strictly necessary to do this, but
            // doing something more clever involves additional
            // parsing work during lookup, to deal with `.`
            // operations in the path to a variable.

            uint32_t fieldCount = pSpireType->getFieldCount();
            for (uint32_t ff = 0; ff < fieldCount; ++ff)
            {
                VariableLayoutReflection* field = pSpireType->getFieldByIndex(ff);
                std::string memberName(field->getName());
                std::string fullName = name.size() ? name + '.' + memberName : memberName;

                ReflectionPath fieldPath;
                fieldPath.parent = pPath;
                fieldPath.typeLayout = pSpireType;
                fieldPath.childIndex = ff;

                reflectType(pContext, field->getTypeLayout(), fullName, &fieldPath);
            }
        }
        break;
        }
    }

    static void reflectVariable(
        ReflectionGenerationContext*        pContext,
        VariableLayoutReflection*    pSpireVar,
        ReflectionPath*                     pParentPath)
    {
        // Get the variable name
        std::string name(pSpireVar->getName());

        // Create a path element for the variable
        ReflectionPath varPath;
        varPath.parent = pParentPath;
        varPath.var = pSpireVar;

        // Reflect the Type
        reflectType(pContext, pSpireVar->getTypeLayout(), name, &varPath);
    }

    static void initializeBufferVariables(
        ReflectionGenerationContext*    pContext,
        ReflectionPath*                 pBufferPath,
        TypeLayoutReflection*    pSpireElementType)
    {
        // Element type of a structured buffer need not be a structured type,
        // so don't recurse unless needed...
        if (pSpireElementType->getKind() != TypeReflection::Kind::Struct)
            return;

        uint32_t fieldCount = pSpireElementType->getFieldCount();

        for (uint32_t ff = 0; ff < fieldCount; ff++)
        {
            auto var = pSpireElementType->getFieldByIndex(ff);

            reflectVariable(pContext, var, pBufferPath);
        }
    }

    bool validateBufferDeclaration(const ProgramReflection::BufferReflection* pPrevDesc, const ProgramReflection::VariableMap& varMap, std::string& log)
    {
        bool match = true;
#define error_msg(msg_) std::string(msg_) + " mismatch.\n";
        if (pPrevDesc->getVariableCount() != varMap.size())
        {
            log += error_msg("Variable count");
            match = false;
        }

        for (auto& prevVar = pPrevDesc->varBegin(); prevVar != pPrevDesc->varEnd(); prevVar++)
        {
            const std::string& name = prevVar->first;
            const auto& curVar = varMap.find(name);
            if (curVar == varMap.end())
            {
                log += "Can't find variable '" + name + "' in the new definitions";
            }
            else
            {
#define test_field(field_, msg_)                                      \
            if(prevVar->second.field_ != curVar->second.field_)       \
            {                                                         \
                log += error_msg(prevVar->first + " " + msg_)         \
                match = false;                                       \
            }

                test_field(location, "offset");
                test_field(arraySize, "array size");
                test_field(arrayStride, "array stride");
                test_field(isRowMajor, "row major");
                test_field(type, "Type");
#undef test_field
            }
        }

#undef error_msg

        return match;
    }

    static ProgramReflection::BufferReflection::StructuredType getStructuredBufferType(
        TypeReflection* pSpireType)
    {
        auto invalid = ProgramReflection::BufferReflection::StructuredType::Invalid;

        if (pSpireType->getKind() != TypeReflection::Kind::Resource)
            return invalid; // not a structured buffer

        if (pSpireType->getResourceShape() != SPIRE_STRUCTURED_BUFFER)
            return invalid; // not a structured buffer

        switch (pSpireType->getResourceAccess())
        {
        default:
            should_not_get_here();
            return invalid;

        case SPIRE_RESOURCE_ACCESS_READ:
            return ProgramReflection::BufferReflection::StructuredType::Default;

        case SPIRE_RESOURCE_ACCESS_READ_WRITE:
        case SPIRE_RESOURCE_ACCESS_RASTER_ORDERED:
            return ProgramReflection::BufferReflection::StructuredType::Counter;
        case SPIRE_RESOURCE_ACCESS_APPEND:
            return ProgramReflection::BufferReflection::StructuredType::Append;
        case SPIRE_RESOURCE_ACCESS_CONSUME:
            return ProgramReflection::BufferReflection::StructuredType::Consume;
        }
    }

    static bool reflectBuffer(
        ReflectionGenerationContext*                pContext,
        TypeLayoutReflection*                pSpireType,
        const std::string&                          name,
        ReflectionPath*                             pPath,
        ProgramReflection::BufferData&              bufferDesc,
        ProgramReflection::BufferReflection::Type   bufferType,
        ProgramReflection::ShaderAccess             shaderAccess)
    {
        auto pSpireElementType = pSpireType->getElementTypeLayout();

        ProgramReflection::VariableMap varMap;

        ReflectionGenerationContext context = *pContext;
        context.pVariables = &varMap;

        initializeBufferVariables(
            &context,
            pPath,
            pSpireElementType);

        // TODO(tfoley): This is a bit of an ugly workaround, and it would
        // be good for the Spire API to make it unnecessary...
        auto category = pSpireType->getParameterCategory();
        if (category == ParameterCategory::Mixed)
        {
            if (pSpireType->getKind() == TypeReflection::Kind::ConstantBuffer)
            {
                category = ParameterCategory::ConstantBuffer;
            }
        }

        auto bindingIndex = getBindingIndex(pPath, category);
        auto bindingSpace = getBindingSpace(pPath, category);
        ProgramReflection::BindLocation bindLocation(
            bindingIndex,
            shaderAccess);
        // If the buffer already exists in the program, make sure the definitions match
        const auto& prevDef = bufferDesc.nameMap.find(name);

        if (prevDef != bufferDesc.nameMap.end())
        {
            if (bindLocation != prevDef->second)
            {
                pContext->getLog() += to_string(bufferType) + " buffer '" + name + "' has different bind locations between different shader stages. Falcor do not support that. Use explicit bind locations to avoid this error";
                return false;
            }
            ProgramReflection::BufferReflection* pPrevBuffer = bufferDesc.descMap[bindLocation].get();
            std::string bufLog;
            if (validateBufferDeclaration(pPrevBuffer, varMap, bufLog) == false)
            {
                pContext->getLog() += to_string(bufferType) + " buffer '" + name + "' has different definitions between different shader stages. " + bufLog;
                return false;
            }
        }
        else
        {
            // Create the buffer reflection
            bufferDesc.nameMap[name] = bindLocation;
            bufferDesc.descMap[bindLocation] = ProgramReflection::BufferReflection::create(
                name,
                bindingIndex,
                bindingSpace,
                bufferType,
                getStructuredBufferType(pSpireType->getType()),
                (uint32_t)pSpireElementType->getSize(),
                varMap,
                ProgramReflection::ResourceMap(),
                shaderAccess);
        }

        // For now we expose all buffers as visible to every stage
        uint32_t mask = 0xFFFFFFFF;
        bufferDesc.descMap[bindLocation]->setShaderMask(mask);

        return true;
    }

    bool ProgramReflection::reflectVertexAttributes(
        ShaderReflection*    pSpireReflector,
        std::string&                log)
    {
        // TODO(tfoley): Add vertex input reflection capability to Spire
        return true;
    }

    bool ProgramReflection::reflectPixelShaderOutputs(
        ShaderReflection*    pSpireReflector,
        std::string&                log)
    {
        // TODO(tfoley): Add fragment output reflection capability to Spire
        return true;
    }

    // TODO(tfoley): Should try to strictly use type...
    static ProgramReflection::Resource::ResourceType getResourceType(VariableLayoutReflection* pParameter)
    {
        switch (pParameter->getCategory())
        {
        case ParameterCategory::SamplerState:
            return ProgramReflection::Resource::ResourceType::Sampler;
        case ParameterCategory::ShaderResource:
        case ParameterCategory::UnorderedAccess:
            switch (pParameter->getType()->getResourceShape() & SPIRE_RESOURCE_BASE_SHAPE_MASK)
            {
            case SPIRE_BYTE_ADDRESS_BUFFER:
                return ProgramReflection::Resource::ResourceType::RawBuffer;

            case SPIRE_STRUCTURED_BUFFER:
                return ProgramReflection::Resource::ResourceType::StructuredBuffer;

            default:
                return ProgramReflection::Resource::ResourceType::Texture;

            case SPIRE_RESOURCE_NONE:
                break;
            }
            break;
        case ParameterCategory::Mixed:
            // TODO: propagate this information up the Falcor level
            return ProgramReflection::Resource::ResourceType::Unknown;
        default:
            break;
        }
        should_not_get_here();
        return ProgramReflection::Resource::ResourceType::Unknown;
    }

    static ProgramReflection::ShaderAccess getShaderAccess(ParameterCategory category)
    {
        switch (category)
        {
        case ParameterCategory::ShaderResource:
        case ParameterCategory::SamplerState:
            return ProgramReflection::ShaderAccess::Read;
        case ParameterCategory::UnorderedAccess:
            return ProgramReflection::ShaderAccess::ReadWrite;
        case ParameterCategory::Mixed:
            return ProgramReflection::ShaderAccess::Undefined;
        default:
            should_not_get_here();
            return ProgramReflection::ShaderAccess::Undefined;
        }
    }

#if 0
    bool reflectResource(
        ReflectionGenerationContext*    pContext,
        ParameterReflection*     pParameter)
    {
        ProgramReflection::Resource falcorDesc;
        std::string name(pParameter->getName());

        falcorDesc.type = getResourceType(pParameter);
        falcorDesc.shaderAccess = getShaderAccess(pParameter->getCategory());
        if (falcorDesc.type == ProgramReflection::Resource::ResourceType::Texture)
        {
            falcorDesc.retType = getReturnType(pParameter->getType()->getTextureResultType());
            falcorDesc.dims = getResourceDimensions(pParameter->getType()->getTextureShape());
        }
        bool isArray = pParameter->getType()->isArray();
        falcorDesc.regIndex = pParameter->getBindingIndex();
        falcorDesc.registerSpace = pParameter->getBindingSpace();
        assert(falcorDesc.registerSpace == 0);
        falcorDesc.arraySize = isArray ? (uint32_t)pParameter->getType()->getTotalArrayElementCount() : 0;

        // If this already exists, definitions should match
        auto& resourceMap = pContext->getResourceMap();
        const auto& prevDef = resourceMap.find(name);
        if (prevDef == resourceMap.end())
        {
            resourceMap[name] = falcorDesc;
        }
        else
        {
            std::string varLog;
            if (verifyResourceDefinition(prevDef->second, falcorDesc, varLog) == false)
            {
                pContext->getLog() += "Shader resource '" + std::string(name) + "' has different definitions between different shader stages. " + varLog;
                return false;
            }
        }

        // Update the mask
        resourceMap[name].shaderMask |= (1 << pContext->shaderIndex);

        return true;
    }
#endif

    static bool reflectStructuredBuffer(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 path)
    {
        auto shaderAccess = getShaderAccess(pSpireType->getType());
        return reflectBuffer(
            pContext,
            pSpireType,
            name,
            path,
            pContext->pReflector->mBuffers[(uint32_t)ProgramReflection::BufferReflection::Type::Structured],
            ProgramReflection::BufferReflection::Type::Structured,
            shaderAccess);
    }

    static bool reflectConstantBuffer(
        ReflectionGenerationContext*    pContext,
        TypeLayoutReflection*    pSpireType,
        const std::string&              name,
        ReflectionPath*                 path)
    {
        return reflectBuffer(
            pContext,
            pSpireType,
            name,
            path,
            pContext->pReflector->mBuffers[(uint32_t)ProgramReflection::BufferReflection::Type::Constant],
            ProgramReflection::BufferReflection::Type::Constant,
            ProgramReflection::ShaderAccess::Read);
    }

#if 0
    static bool reflectStructuredBuffer(
        ReflectionGenerationContext*    pContext,
        BufferReflection*        spireParam)
    {
        auto shaderAccess = getShaderAccess(spireParam->getCategory());
        return reflectBuffer(
            pContext,
            spireParam,
            pContext->pReflector->mBuffers[(uint32_t)ProgramReflection::BufferReflection::Type::Structured],
            ProgramReflection::BufferReflection::Type::Structured,
            ProgramReflection::ShaderAccess::Read);
    }

    static bool reflectConstantBuffer(
        ReflectionGenerationContext*    pContext,
        BufferReflection*        spireParam)
    {
        return reflectBuffer(
            pContext,
            spireParam,
            pContext->pReflector->mBuffers[(uint32_t)ProgramReflection::BufferReflection::Type::Constant],
            ProgramReflection::BufferReflection::Type::Constant,
            ProgramReflection::ShaderAccess::Read);
    }

    static bool reflectVariable(
        ReflectionGenerationContext*        pContext,
        VariableLayoutReflection*    spireVar)
    {
        switch (spireVar->getCategory())
        {
        case ParameterCategory::ConstantBuffer:
            return reflectConstantBuffer(pContext, spireVar->asBuffer());

        case ParameterCategory::ShaderResource:
        case ParameterCategory::UnorderedAccess:
        case ParameterCategory::SamplerState:
            return reflectResource(pContext, spireVar);

        case ParameterCategory::Mixed:
        {
            // The parameter spans multiple binding kinds (e.g., both texture and uniform).
            // We need to recursively split it into sub-parameters, each using a single
            // kind of bindable resource.
            //
            // Also, the parameter may have been declared as a constant buffer, so
            // we need to reflect it directly in that case:
            //
            switch (spireVar->getType()->getKind())
            {
            case TypeReflection::Kind::ConstantBuffer:
                return reflectConstantBuffer(pContext, spireVar->asBuffer());

            default:
                //
                // Okay, let's walk it recursively to bind the sub-pieces...
                //
                logError("unimplemented: global-scope uniforms");
                break;
            }
        }
        break;


        case ParameterCategory::Uniform:
            // Ignore uniform parameters during this pass...
            logError("unimplemented: global-scope uniforms");
            return true;

        default:
            break;
        }
    }
#endif

    static bool reflectParameter(
        ReflectionGenerationContext*        pContext,
        VariableLayoutReflection*    spireParam)
    {
        reflectVariable(pContext, spireParam, nullptr);
        return true;
    }

    bool ProgramReflection::reflectResources(
        ShaderReflection*    pSpireReflector,
        std::string&                log)
    {
        ReflectionGenerationContext context;
        context.pReflector = this;
        context.pResourceMap = &mResources;
        context.pLog = &log;

        bool res = true;

        uint32_t paramCount = pSpireReflector->getParameterCount();
        for (uint32_t pp = 0; pp < paramCount; ++pp)
        {
            VariableLayoutReflection* param = pSpireReflector->getParameterByIndex(pp);
            res = reflectParameter(&context, param);
        }

        // Also extract entry-point stuff

        SpireUInt entryPointCount = pSpireReflector->getEntryPointCount();
        for (SpireUInt ee = 0; ee < entryPointCount; ++ee)
        {
            EntryPointReflection* entryPoint = pSpireReflector->getEntryPointByIndex(ee);

            switch (entryPoint->getStage())
            {
            case SPIRE_STAGE_COMPUTE:
            {
                SpireUInt sizeAlongAxis[3];
                entryPoint->getComputeThreadGroupSize(3, &sizeAlongAxis[0]);
                mThreadGroupSizeX = (uint32_t)sizeAlongAxis[0];
                mThreadGroupSizeY = (uint32_t)sizeAlongAxis[1];
                mThreadGroupSizeZ = (uint32_t)sizeAlongAxis[2];
            }
            break;

            default:
                break;
            }
        }

        return res;
    }
}