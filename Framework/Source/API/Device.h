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
#include "API/Window.h"
#include "API/Texture.h"
#include "API/FBO.h"
#include "API/RenderContext.h"
#include "Api/LowLevel/DescriptorHeap.h"
#include "API/LowLevel/ResourceAllocator.h"

namespace Falcor
{
#ifdef _DEBUG
#define DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define DEFAULT_ENABLE_DEBUG_LAYER false
#endif

    class Device
    {
    public:
        using SharedPtr = std::shared_ptr<Device>;
        using SharedConstPtr = std::shared_ptr<const Device>;
        using ApiHandle = DeviceHandle;

		/** Device configuration
		*/
        struct Desc
        {
			ResourceFormat colorFormat = ResourceFormat::RGBA8UnormSrgb;    ///< The color buffer format
			ResourceFormat depthFormat = ResourceFormat::D24UnormS8;        ///< The depth buffer format
            int apiMajorVersion = DEFAULT_API_MAJOR_VERSION; ///< Requested API major version. Context creation fails if this version is not supported.
            int apiMinorVersion = DEFAULT_API_MINOR_VERSION; ///< Requested API minor version. Context creation fails if this version is not supported.
            bool useDebugContext = false;             ///< create a debug context. NOTE: Debug configuration always creates a debug context
            std::vector<std::string> requiredExtensions; ///< Extensions required by the sample
			bool enableVsync = false;           ///< Controls vertical-sync
            bool enableDebugLayer = DEFAULT_ENABLE_DEBUG_LAYER;    ///< Enable the debug layer. The default for release build is false, for debug build it's true.
        };

		/** Create a new device.
		\param[in] pWindow a previously-created window object
		\param[in] desc Device configuration desctiptor.
		\return nullptr if the function failed, otherwise a new device object
		*/
		static SharedPtr create(Window::SharedPtr& pWindow, const Desc& desc);

		/** Destructor
		*/
        ~Device();

		/** Enable/disable vertical sync
		*/
		void setVSync(bool enable);

		/** Check if the window is occluded
		*/
		bool isWindowOccluded() const;

		/** Check if the device support an extension
		*/
		static bool isExtensionSupported(const std::string & name);

		/** Get the FBO object associated with the swap-chain.
			This can change each frame, depending on the API used
		*/
        Fbo::SharedPtr getSwapChainFbo() const;

		/** Get the default render-context.
			The default render-context is managed completly by the device. The user should just queue commands into it, the device will take care of allocation, submission and synchronization
		*/
		RenderContext::SharedPtr getRenderContext() const { return mpRenderContext; }

		/** Get the native API handle
		*/
		DeviceHandle getApiHandle() { return mApiHandle; }

		/** Present the back-buffer to the window
		*/
		void present();

		/** Check if vertical sync is enabled
		*/
		bool isVsyncEnabled() const { return mVsyncOn; }

        /** Resize the swap-chain
            \return A new FBO object
        */
        Fbo::SharedPtr resizeSwapChain(uint32_t width, uint32_t height);

        // SPIRE: split CPU and GPU heaps for SRVs and samplers
        DescriptorHeap::SharedPtr getShaderSrvDescriptorHeap() const { return mpShaderSrvHeap; }
        DescriptorHeap::SharedPtr getCpuSrvDescriptorHeap() const { return mpCpuSrvHeap; }

        DescriptorHeap::SharedPtr getDsvDescriptorHeap() const { return mpDsvHeap; }
//SPIRE:        DescriptorHeap::SharedPtr getUavDescriptorHeap() const { return mpUavHeap; }
//SPIRE:        DescriptorHeap::SharedPtr getCpuUavDescriptorHeap() const { return mpCpuUavHeap; }
        DescriptorHeap::SharedPtr getRtvDescriptorHeap() const { return mpRtvHeap; }

        DescriptorHeap::SharedPtr getShaderSamplerDescriptorHeap() const { return mpShaderSamplerHeap; }
        DescriptorHeap::SharedPtr getCpuSamplerDescriptorHeap() const { return mpCpuSamplerHeap; }

        ResourceAllocator::SharedPtr getResourceAllocator() const { return mpResourceAllocator; }
        DescriptorAllocator* getDescriptorAllocator() const { return const_cast<DescriptorAllocator*>(&mDescriptorAllocator); }
        void releaseResource(ApiObjectHandle pResource);


        //SPIRE:
        void copyDescriptor(
            DescriptorHeap::CpuHandle   dest,
            DescriptorHeap::CpuHandle   src,
            DescriptorHeap::Type        type);

    private:
		Device(Window::SharedPtr pWindow) : mpWindow(pWindow) {}
		bool init(const Desc& desc);
        bool updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);
        void executeDeferredReleases();

        ApiHandle mApiHandle;
        ResourceAllocator::SharedPtr mpResourceAllocator;

        // SPIRE:
        DescriptorAllocator mDescriptorAllocator;

        DescriptorHeap::SharedPtr mpRtvHeap;
        DescriptorHeap::SharedPtr mpDsvHeap;
        DescriptorHeap::SharedPtr mpShaderSamplerHeap;
        DescriptorHeap::SharedPtr mpCpuSamplerHeap;
        DescriptorHeap::SharedPtr mpShaderSrvHeap;
        DescriptorHeap::SharedPtr mpCpuSrvHeap;
//        DescriptorHeap::SharedPtr mpUavHeap;
//        DescriptorHeap::SharedPtr mpCpuUavHeap; // We need it for clearing UAVs

		Window::SharedPtr mpWindow;
		void* mpPrivateData;
		RenderContext::SharedPtr mpRenderContext;
		bool mVsyncOn;
        size_t mFrameID = 0;
	};

    extern Device::SharedPtr gpDevice;
}