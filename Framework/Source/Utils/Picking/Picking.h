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
#pragma once

#include "Graphics/Scene/SceneRenderer.h"
#include "Graphics/Model/ObjectInstance.h"

namespace Falcor
{
    class Picking : public SceneRenderer
    {
    public:
        using UniquePtr = std::unique_ptr<Picking>;
        using UniqueConstPtr = std::unique_ptr<const Picking>;

        static UniquePtr create(const Scene::SharedPtr& pScene, uint32_t fboWidth, uint32_t fboHeight);

        /** Performs a picking operation on the scene
            \param[in] mousePos Mouse position in the range [0,1] with (0,0) being the top left corner. Same coordinate space as in MouseEvent.
            \param[in] pContext Render context to render scene with
            \return Whether an object was picked or not.
        */
        bool pick(RenderContext* pContext, const glm::vec2& mousePos, Camera* pCamera);

        ObjectInstance<Mesh>::SharedPtr getPickedMeshInstance() const;
        ObjectInstance<Model>::SharedPtr getPickedModelInstance() const;

        void resizeFBO(uint32_t width, uint32_t height);

    private:

        Picking(const Scene::SharedPtr& pScene, uint32_t fboWidth, uint32_t fboHeight);

        void renderScene(RenderContext* pContext, Camera* pCamera);
        void readPickResults(RenderContext* pContext);

        virtual bool setPerMeshInstanceData(RenderContext* pContext, const glm::mat4& translation, const Model::MeshInstance::SharedPtr& instance, uint32_t drawInstanceID, const CurrentWorkingData& currentData) override;
        virtual bool setPerMaterialData(RenderContext* pContext, const CurrentWorkingData& currentData) override;

        void calculateScissor(const glm::vec2& mousePos);

        static void updateVariableOffsets(const ProgramReflection* pReflector);

        static size_t sDrawIDOffset;

        std::unordered_map<uint32_t, ObjectInstance<Mesh>::SharedPtr> mDrawIDToInstance;

        uint32_t mPickResults = 0;

        Fbo::SharedPtr mpFBO;
        GraphicsProgram::SharedPtr mpProgram;
        GraphicsVars::SharedPtr mpProgramVars;
        GraphicsState::SharedPtr mpGraphicsState;

        GraphicsState::Scissor mScissor;
        glm::vec2 mMousePos; // #TODO REMOVE THIS
    };
}
