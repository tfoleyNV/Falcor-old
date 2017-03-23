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
#include <string>
#include "ProgramReflection.h"
#include "Texture.h"
#include "VariablesBuffer.h"
#include "Graphics/Program.h"
#include "API/LowLevel/DescriptorHeap.h"

#include "Sampler.h"

namespace Falcor
{
    class Sampler;

    /** Variable naming rules are very similar to OpenGL variable naming rules.\n
        When accessing a variable by name, you can only use a name which points to a basic Type, or an array of basic Type (so if you want the start of a structure, ask for the first field in the struct).\n
        Note that Falcor has 2 flavors of setting variable by names - SetVariable() and SetVariableArray(). Naming rules for N-dimensional arrays of a basic Type are a little different between the two.\n
        SetVariable() must include N indices. SetVariableArray() can include N indices, or N-1 indices (implicit [0] as last index).\n\n
    */
    class ConstantBuffer : public VariablesBuffer, public inherit_shared_from_this<VariablesBuffer, ConstantBuffer>
    {
    public:
        class SharedPtr : public std::shared_ptr<ConstantBuffer>
        {
        public:
            class Var
            {
            public:
                Var(ConstantBuffer* pBuf, size_t offset) : mpBuf(pBuf), mOffset(offset) {}
                template<typename T> void operator=(const T& val) { mpBuf->setVariable(mOffset, val); }

                size_t getOffset() const { return mOffset; }
            protected:
                ConstantBuffer* mpBuf;
                size_t mOffset;
            };

            SharedPtr() = default;
            SharedPtr(ConstantBuffer* pBuf) : std::shared_ptr<ConstantBuffer>(pBuf) {}
            SharedPtr(std::shared_ptr<ConstantBuffer> pBuf) : std::shared_ptr<ConstantBuffer>(pBuf) {}

            Var operator[](size_t offset) { return Var(get(), offset); }
            Var operator[](const std::string& var) { return Var(get(), get()->getVariableOffset(var)); }
        };

        using SharedConstPtr = std::shared_ptr<const ConstantBuffer>;

        /** create a new constant buffer.\n
            Even though the buffer is created with a specific reflection object, it can be used with other programs as long as the buffer declarations are the same across programs.
            \param[in] pReflector A buffer-reflection object describing the buffer layout
            \param[in] overrideSize - if 0, will use the buffer size as declared in the shader. Otherwise, will use this value as the buffer size. Useful when using buffers with dynamic arrays.
            \return A new buffer object if the operation was successful, otherwise nullptr
        */
        static SharedPtr create(const ProgramReflection::BufferTypeReflection::SharedConstPtr& pReflector, size_t overrideSize = 0);

        static SharedPtr create(const ProgramReflection::BufferReflection::SharedConstPtr& pReflector, size_t overrideSize = 0)
        {
            return create(pReflector->getTypeReflection(), overrideSize);
        }

        /** create a new constant buffer from a program object.\n
        This function is purely syntactic sugar. It will fetch the requested buffer reflector from the active program version and create the buffer from it
        \param[in] pProgram A program object which defines the buffer
        \param[in] name The buffer's name
        \param[in] overrideSize - if 0, will use the buffer size as declared in the shader. Otherwise, will use this value as the buffer size. Useful when using buffers with dynamic arrays.
        \return A new buffer object if the operation was successful, otherwise nullptr
        */
        static SharedPtr create(Program::SharedPtr& pProgram, const std::string& name, size_t overrideSize = 0);

        ~ConstantBuffer();

        /** Set a variable into the buffer.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] name The variable name. See notes about naming in the ConstantBuffer class description.
        \param[in] value Value to set
        */
        template<typename T>
        void setVariable(const std::string& name, const T& value)
        {
            return VariablesBuffer::setVariable(name, 0, value);
        }

        /** Set a variable array in the buffer.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] offset The variable byte offset inside the buffer
        \param[in] pValue Pointer to an array of values to set
        \param[in] count pValue array size
        */
        template<typename T>
        void setVariableArray(size_t offset, const T* pValue, size_t count)
        {
            return VariablesBuffer::setVariableArray(offset, 0, pValue, count);
        }

        /** Set a variable into the buffer.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] offset The variable byte offset inside the buffer
        \param[in] value Value to set
        */
        template<typename T>
        void setVariable(size_t offset, const T& value)
        {
            return VariablesBuffer::setVariable(offset, 0, value);
        }

        /** Set a variable array in the buffer.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] name The variable name. See notes about naming in the ConstantBuffer class description.
        \param[in] pValue Pointer to an array of values to set
        \param[in] count pValue array size
        */
        template<typename T>
        void setVariableArray(const std::string& name, const T* pValue, size_t count)
        {
            return VariablesBuffer::setVariableArray(name, 0, pValue, count);
        }

        /** Set a texture or image.
        The function will validate that the resource Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] name The variable name in the program. See notes about naming in the ConstantBuffer class description.
        \param[in] pTexture The resource to bind. If bBindAsImage is set, binds as image.
        \param[in] pSampler The sampler to use for filtering. If this is nullptr, the default sampler will be used
        */
        void setTexture(const std::string& name, const Texture* pTexture, const Sampler* pSampler)
        {
            return VariablesBuffer::setTexture(name, pTexture, pSampler);
        }

        /** Set a texture or image.
        The function will validate that the resource Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] name The variable name in the program. See notes about naming in the ConstantBuffer class description.
        \param[in] pTexture The resource to bind
        \param[in] pSampler The sampler to use for filtering. If this is nullptr, the default sampler will be used
        \param[in] count Number of textures to bind
        */
        void setTextureArray(const std::string& name, const Texture* pTexture[], const Sampler* pSampler, size_t count)
        {
            return VariablesBuffer::setTextureArray(name, pTexture, pSampler, count);
        }

        /** Set a texture or image.
        The function will validate that the resource Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] offset The variable byte offset inside the buffer
        \param[in] pTexture The resource to bind. If bBindAsImage is set, binds as image.
        \param[in] pSampler The sampler to use for filtering. If this is nullptr, the default sampler will be used
        */
        void setTexture(size_t Offset, const Texture* pTexture, const Sampler* pSampler)
        {
            return VariablesBuffer::setTexture(Offset, pTexture, pSampler);
        }

        virtual void uploadToGPU(size_t offset = 0, size_t size = -1) const override;

        DescriptorHeap::Entry getCBV() const;
    protected:
        ConstantBuffer(const ProgramReflection::BufferTypeReflection::SharedConstPtr& pReflector, size_t size);
        mutable DescriptorHeap::Entry mCBV;
#ifdef FALCOR_D3D11
        friend class RenderContext;
        std::map<uint32_t, ID3D11ShaderResourceViewPtr>* mAssignedResourcesMap;
        std::map<uint32_t, ID3D11SamplerStatePtr>* mAssignedSamplersMap;
        const std::map<uint32_t, ID3D11ShaderResourceViewPtr>& getAssignedResourcesMap() const { return *mAssignedResourcesMap; }
        const std::map<uint32_t, ID3D11SamplerStatePtr>& getAssignedSamplersMap() const { return *mAssignedSamplersMap; }
#endif
    };

    class ComponentInstance : public std::enable_shared_from_this<ComponentInstance>
    {
    public:
        using SharedPtr = std::shared_ptr<ComponentInstance>;

        static SharedPtr create(const ProgramReflection::BufferTypeReflection::SharedConstPtr& pReflector);

        template<typename T>
        void setVariable(size_t offset, const T& value)
        {
            mConstantBuffer->setVariable(offset, value);
        }

        template<typename T>
        void setVariable(const std::string& name, const T& value)
        {
            mConstantBuffer->setVariable(name, value);
        }

        void setBlob(const void* pSrc, size_t offset, size_t size)
        {
            mConstantBuffer->setBlob(pSrc, offset, size);
        }

        void setTexture(const std::string& name, const Texture* pTexture);

        void setSampler(const std::string& name, const Sampler* pSampler);

        void setVariable(const std::string& name, const Texture* pTexture)
        {
            setTexture(name, pTexture);
        }

        void setVariable(const std::string& name, const Sampler* pSampler)
        {
            setSampler(name, pSampler);
        }

        size_t getVariableOffset(const std::string& name)
        {
            return mConstantBuffer->getVariableOffset(name);
        }

        SpireModule* getSpireComponentClass() const { return mReflector->getSpireComponentClass(); }

    //private:
        ProgramReflection::BufferTypeReflection::SharedConstPtr mReflector;
        ConstantBuffer::SharedPtr mConstantBuffer;

        std::vector<Texture::SharedConstPtr> mBoundTextures;
        std::vector<Sampler::SharedConstPtr> mBoundSamplers;
    };
}