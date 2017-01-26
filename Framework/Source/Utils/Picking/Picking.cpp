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
#include "Utils/Picking/Picking.h"
#include "Graphics/FboHelper.h"

namespace Falcor
{
    size_t Picking::sDrawIDOffset = ConstantBuffer::kInvalidOffset;
    static const char* kPerStaticMeshCbName = "InternalPerStaticMeshCB";

    Picking::UniquePtr Picking::create(const Scene::SharedPtr& pScene, uint32_t fboWidth, uint32_t fboHeight)
    {
        return UniquePtr(new Picking(pScene, fboWidth, fboHeight));
    }

    bool Picking::pick(RenderContext* pContext, const glm::vec2& mousePos, Camera* pCamera)
    {
        calculateScissor(mousePos);
        renderScene(pContext, pCamera);
        readPickResults(pContext);
        return mPickResults != 0;
    }

    ObjectInstance<Mesh>::SharedPtr Picking::getPickedMeshInstance() const
    {
        auto it = mDrawIDToInstance.find(mPickResults);

        if (it == mDrawIDToInstance.end())
        {
            return nullptr;
        }

        return it->second;
    }

    ObjectInstance<Model>::SharedPtr Picking::getPickedModelInstance() const
    {
        return nullptr;
    }

    void Picking::resizeFBO(uint32_t width, uint32_t height)
    {
        Fbo::Desc fboDesc;
        fboDesc.setColorTarget(0, Falcor::ResourceFormat::R16Uint).setDepthStencilTarget(Falcor::ResourceFormat::D24UnormS8);

        mpFBO = FboHelper::create2D(width, height, fboDesc);
    }

    Picking::Picking(const Scene::SharedPtr& pScene, uint32_t fboWidth, uint32_t fboHeight)
        : SceneRenderer(pScene)
    {
        mpGraphicsState = GraphicsState::create();

        // Create FBO
        resizeFBO(fboWidth, fboHeight);
        mpGraphicsState->setFbo(mpFBO);

        // Compile shaders
        mpProgram = GraphicsProgram::createFromFile("", "Picking.ps.hlsl");
        mpGraphicsState->setProgram(mpProgram);

        mpProgramVars = GraphicsVars::create(mpProgram->getActiveVersion()->getReflector());

        // Depth State
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthTest(true);
        mpGraphicsState->setDepthStencilState(DepthStencilState::create(dsDesc));

        // Rasterizer State
        RasterizerState::Desc rsDesc;
        rsDesc.setCullMode(RasterizerState::CullMode::Back);
        mpGraphicsState->setRasterizerState(RasterizerState::create(rsDesc));
    }

    void Picking::renderScene(RenderContext* pContext, Camera* pCamera)
    {
        mDrawIDToInstance.clear();

        const glm::vec4 clearColor;
        pContext->clearFbo(mpFBO.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

        // Save state
        auto pPrevGraphicsState = pContext->getGraphicsState();

        mpGraphicsState->setScissors(0, mScissor);

        // Render
        pContext->setGraphicsState(mpGraphicsState);
        pContext->setGraphicsVars(mpProgramVars);

        updateVariableOffsets(pContext->getGraphicsVars()->getReflection().get());

        SceneRenderer::renderScene(pContext, pCamera);

        // Restore state
        pContext->setGraphicsState(pPrevGraphicsState);
    }

    void Picking::readPickResults(RenderContext* pContext)
    {
        mPickResults = 0;

        std::vector<uint8_t> textureData = pContext->readTextureSubresource(mpFBO->getColorTexture(0).get(), 0);
        uint16_t* pData = (uint16_t*)textureData.data();

        // #TODO MATH

        const uint32_t elements = (uint32_t)textureData.size() / 2;
        for (uint32_t i = 0; i < elements; i++)
        {
            if (pData[i] != 0)
            {
                mPickResults = pData[i];
                return;
            }
        }
    }

    bool Picking::setPerMeshInstanceData(RenderContext* pContext, const glm::mat4& translation, const Model::MeshInstance::SharedPtr& instance, uint32_t drawInstanceID, const CurrentWorkingData& currentData)
    {
        ConstantBuffer* pCB = pContext->getGraphicsVars()->getConstantBuffer(kPerStaticMeshCbName).get();
        pCB->setBlob(&currentData.drawID, sDrawIDOffset + drawInstanceID * sizeof(uint32_t), sizeof(uint32_t));

        mDrawIDToInstance[currentData.drawID] = instance;

        return SceneRenderer::setPerMeshInstanceData(pContext, translation, instance, drawInstanceID, currentData);
    }

    bool Picking::setPerMaterialData(RenderContext* pContext, const CurrentWorkingData& currentData)
    {
        return true;
    }

    void Picking::calculateScissor(const glm::vec2& mousePos)
    {
        glm::vec2 mouseCoords = mousePos * glm::vec2(mpFBO->getWidth(), mpFBO->getHeight());;

        mScissor.originX = (int32_t)mouseCoords.x;
        mScissor.originY = (int32_t)mouseCoords.y;
        mScissor.width = 1;
        mScissor.height = 1;
    }

    void Picking::updateVariableOffsets(const ProgramReflection* pReflector)
    {
        if (sDrawIDOffset == ConstantBuffer::kInvalidOffset)
        {
            const auto pPerMeshCbData = pReflector->getBufferDesc(kPerStaticMeshCbName, ProgramReflection::BufferReflection::Type::Constant);
            assert(pPerMeshCbData);

            sDrawIDOffset = pPerMeshCbData->getVariableData("gDrawId[0]")->location;
        }
    }

}
