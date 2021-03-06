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
#include "Utils/PixelZoom.h"
#include "Graphics/FboHelper.h"

namespace Falcor
{
    PixelZoom::SharedPtr PixelZoom::create()
    {
        return SharedPtr(new PixelZoom());
    }

    void PixelZoom::init(Fbo* backBuffer)
    {
        Fbo::Desc desc = backBuffer->getDesc();
        mpSrcBlitFbo = FboHelper::create2D(backBuffer->getWidth(), backBuffer->getHeight(), desc);
        mpDstBlitFbo = FboHelper::create2D(mDstZoomSize, mDstZoomSize, desc);
    }

    void PixelZoom::render(RenderContext* pCtx, Fbo* backBuffer)
    {
        if (mShouldZoom)
        {
            //copy backbuffer into src blit fbo
            pCtx->copyResource(mpSrcBlitFbo->getColorTexture(0).get(), backBuffer->getColorTexture(0).get());

            //blit src blit fbo into dst blit fbo
            uint32_t offset = mSrcZoomSize / 2;
            vec2 srcPix = vec2(mMousePos.x * backBuffer->getWidth(), mMousePos.y * backBuffer->getHeight());
            clampToEdge(srcPix, backBuffer->getWidth(), backBuffer->getHeight(), offset);
            vec4 srcRect = vec4(srcPix.x - offset, srcPix.y - offset, srcPix.x + offset, srcPix.y + offset);
            vec4 dstRect = vec4(0, 0, mDstZoomSize, mDstZoomSize);
            pCtx->blit(mpSrcBlitFbo->getColorTexture(0)->getSRV(), mpDstBlitFbo->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);

            //blit dst blt fbo into back buffer
            offset = mDstZoomSize / 2;
            clampToEdge(srcPix, backBuffer->getWidth(), backBuffer->getHeight(), offset);
            srcRect = dstRect;
            dstRect = vec4(srcPix.x - offset, srcPix.y - offset, srcPix.x + offset, srcPix.y + offset);
            pCtx->blit(mpDstBlitFbo->getColorTexture(0)->getSRV(), backBuffer->getColorTexture(0)->getRTV(), srcRect, dstRect, Sampler::Filter::Point);
        }
    }

    void PixelZoom::onMouseEvent(const MouseEvent& me)
    {
        if (mShouldZoom)
        {
            mMousePos = me.pos;
            //negative to swap scroll up to zoom in and scoll down to zoom out
            int32_t zoomDelta = -1 * mZoomCoefficient * (int32_t)me.wheelDelta.y;
            mSrcZoomSize = max(mSrcZoomSize + zoomDelta, 3);
        }
    }

    void PixelZoom::clampToEdge(vec2& pix, uint32_t width, uint32_t height, uint32_t offset)
    {
        vec2 posOffset = pix + vec2(offset, offset);
        vec2 negOffset = pix - vec2(offset, offset);

        //x
        if (posOffset.x > width)
        {
            pix.x = pix.x - (posOffset.x - width);
        }
        else if (negOffset.x < 0)
        {
            pix.x = pix.x - negOffset.x;
        }

        //y
        if (posOffset.y > height)
        {
            pix.y = pix.y - (posOffset.y - height);
        }
        else if (negOffset.y < 0)
        {
            pix.y = pix.y - negOffset.y;
        }
    }

    void PixelZoom::onKeyboardEvent(const KeyboardEvent& ke)
    {
        if (ke.key == KeyboardEvent::Key::Z)
        {
            if (ke.type == KeyboardEvent::Type::KeyPressed)
            {
                mShouldZoom = true;
            }
            else
            {
                mShouldZoom = false;
            }
        }
    }

}
