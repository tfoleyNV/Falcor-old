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
#include "Framework.h"
#include <vector>
#include "API/Shader.h"

namespace Falcor
{
    struct ShaderData
    {
        ID3DBlobPtr pBlob;
    };

    static const char* kEntryPoint = "main";

    const char* getTargetString(ShaderType type)
    {
        switch (type)
        {
        case ShaderType::Vertex:
            return "vs_5_0";
        case ShaderType::Pixel:
            return "ps_5_0";
        case ShaderType::Hull:
            return "hs_5_0";
        case ShaderType::Domain:
            return "ds_5_0";
        case ShaderType::Geometry:
            return "gs_5_0";
        case ShaderType::Compute:
            return "cs_5_0";
        default:
            should_not_get_here();
            return "";
        }
    }

#if FALCOR_USE_SPIRE_AS_COMPILER
    struct SpireBlob : ID3DBlob
    {
        void* buffer;
        size_t bufferSize;
        size_t refCount;

        SpireBlob(void* buffer, size_t bufferSize)
            : buffer(buffer)
            , bufferSize(bufferSize)
            , refCount(1)
        {}

        // IUnknown

        virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override
        {
            *ppvObject = this;
            return S_OK;
        }

        virtual ULONG STDMETHODCALLTYPE AddRef( void) override
        {
            ++refCount;
            return (ULONG) refCount;
        }

        virtual ULONG STDMETHODCALLTYPE Release( void) override
        {
            --refCount;
            if(refCount == 0)
            {
                delete this;
            }
            return (ULONG) refCount;
        }

        // ID3DBlob

        virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() override
        {
            return buffer;
        }

        virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() override
        {
            return bufferSize;
        }
    };

    ID3DBlobPtr Shader::compile(const std::string& source, std::string& errorLog)
    {
        ID3DBlob* pCode;
        ID3DBlobPtr pErrors;

        UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG;
#endif

        HRESULT hr = D3DCompile(source.c_str(), source.size(), nullptr, nullptr, nullptr, kEntryPoint, getTargetString(mType), flags, 0, &pCode, &pErrors);
        if(FAILED(hr))
        {
            errorLog = convertBlobToString(pErrors.GetInterfacePtr());
            return nullptr;
        }

        return pCode;
    }

#if 0
    ID3DBlobPtr Shader::compileSpire(
        const std::string&      source,
        std::string&            errorLog,
        DefineList const&       defines,
        ShaderReflectionHandle& outReflector)
    {
        SpireCompilationContext* spireContext = spCreateCompilationContext(NULL);
        SpireDiagnosticSink* spireSink = spCreateDiagnosticSink(spireContext);

        // TODO: Spire should really support a callback API for all file I/O,
        // rather than having us specify data directories to it...
        for (auto path : getDataDirectoriesList())
        {
            spAddSearchPath(spireContext, path.c_str());
        }

        // TODO: Need to pass #define flags down to Spire here, since we aren't doing
        // it in our own preprocessor any more...
        for(auto shaderDefine : defines)
        {
            spAddPreprocessorDefine(spireContext, shaderDefine.first.c_str(), shaderDefine.second.c_str());
        }

#if defined(FALCOR_GL)
        spSetCodeGenTarget(spireContext, SPIRE_GLSL);
#elif defined(FALCOR_D3D11) || defined(FALCOR_D3D12)
        // Note: we could compile Spire directly to DXBC (by having Spire invoke the MS compiler for us,
        // but that path seems to have more issues at present, so let's just go to HLSL instead...)
        spSetCodeGenTarget(spireContext, SPIRE_HLSL);
        spAddPreprocessorDefine(spireContext, "FALCOR_HLSL", "1");
#else
#error unknown shader compilation target
#endif

        SpireCompileFlags spireFlags = 0;

        // Don't actually perform semantic checking: just pass through functions bodies to downstream compiler
        spireFlags |= SPIRE_COMPILE_FLAG_NO_CHECKING;

        spSetCompileFlags(spireContext, spireFlags);
        spAddEntryPoint(spireContext, "main", getTargetString(mType));

//        spSetCodeGenTarget(spireContext, SPIRE_DXBC);

        SpireCompilationResult* result = spCompileShaderFromSource(spireContext, source.c_str(), "falcor", spireSink);

        int diagnosticsSize = spGetDiagnosticOutput(spireSink, NULL, 0);
        if (diagnosticsSize != 0)
        {
            char* diagnostics = (char*)malloc(diagnosticsSize);
            spGetDiagnosticOutput(spireSink, diagnostics, diagnosticsSize);
            errorLog += diagnostics;
            free(diagnostics);
        }
        if(spDiagnosticSinkHasAnyErrors(spireSink) != 0)
        {
            spDestroyDiagnosticSink(spireSink);
            spDestroyCompilationContext(spireContext);
            return nullptr;
        }

        int bufferSize = 0;
        char const* buffer = spGetShaderStageSource(result, NULL, NULL, &bufferSize);

        // Now invoke the MS compiler to translate the generated HLSL down to DXBC
        ID3DBlobPtr compiledBlob = compile(buffer, errorLog);


        // Extract the reflection data from Spire, rather than use the MS-compiled version
        auto reflectionBlob = (spire::ShaderReflection*) spGetReflection(result);
        assert(reflectionBlob);

        // Note: the reflection data that we query from Spire only survives as long as
        // the `SpireCompilationResult` that owns it, so we have to make our own copy.
        auto reflectionBlobSize = reflectionBlob->getReflectionDataSize();
        spire::ShaderReflection* reflectionBlobCopy = (spire::ShaderReflection*) malloc(reflectionBlobSize);
        memcpy(reflectionBlobCopy, reflectionBlob, reflectionBlobSize);

        spDestroyCompilationResult(result);
        spDestroyDiagnosticSink(spireSink);
        spDestroyCompilationContext(spireContext);

        outReflector = reflectionBlobCopy;
        return compiledBlob;
    }
#endif
#endif

    Shader::Shader(ShaderType type) : mType(type)
    {
        mpPrivateData = new ShaderData;
    }

    Shader::~Shader()
    {
        ShaderData* pData = (ShaderData*)mpPrivateData;
        safe_delete(pData);
    }

    bool Shader::init(
            const std::string&  shaderString,
            std::string&        log)
    {
        // Compile the shader
        ShaderData* pData = (ShaderData*)mpPrivateData;
        pData->pBlob = compile(shaderString, log);

        if (pData->pBlob == nullptr)
        {
            return nullptr;
        }

#ifdef FALCOR_D3D11
        // create the shader object
        switch (type)
        {
        case ShaderType::Vertex:
            pData->pHandle = createVertexShader(pData->pBlob);
            break;
        case ShaderType::Pixel:
            pData->pHandle = createPixelShader(pData->pBlob);
            break;
        case ShaderType::Hull:
            pData->pHandle = createHullShader(pData->pBlob);
            break;
        case ShaderType::Domain:
            pData->pHandle = createDomainShader(pData->pBlob);
            break;
        case ShaderType::Geometry:
            pData->pHandle = createGeometryShader(pData->pBlob);
            break;
        case ShaderType::Compute:
            pData->pHandle = createComputeShader(pData->pBlob);
            break;
        default:
            should_not_get_here();
            return nullptr;
        }

        if (pData->pHandle == nullptr)
        {
            return false;
        }
#elif defined FALCOR_D3D12
        mApiHandle = { pData->pBlob->GetBufferPointer(), pData->pBlob->GetBufferSize() };
#endif

        return true;
    }

    Shader::SharedPtr Shader::create(
        const std::string&          shaderString,
        ShaderType                  type,
        std::string&                log)
    {
        SharedPtr pShader = SharedPtr(new Shader(type));
        return pShader->init(shaderString, log) ? pShader : nullptr;
    }

    ID3DBlobPtr Shader::getCodeBlob() const
    {
        const ShaderData* pData = (ShaderData*)mpPrivateData;
        return pData->pBlob;
    }
}
