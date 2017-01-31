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

#include "Graphics/Paths/MovableObject.h"
#include "Utils/AABB.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"

namespace Falcor
{
    class SceneRenderer;
    class Model;

    template<typename ObjectType>
    class ObjectInstance : public IMovableObject, public inherit_shared_from_this<IMovableObject, ObjectInstance<ObjectType>>
    {
    public:
        using SharedPtr = std::shared_ptr<ObjectInstance>;
        using SharedConstPtr = std::shared_ptr<const ObjectInstance>;

        /** Constructs a object instance with a transform
            \param[in] pObject Object to create an instance of
            \param[in] baseTransform Base transform matrix of the instance
            \param[in] name Name of the instance
            \return A new instance of the object if pObject
        */
        static SharedPtr create(const typename ObjectType::SharedPtr& pObject, const glm::mat4& baseTransform, const std::string& name = "")
        {
            assert(pObject);
            return SharedPtr(new ObjectInstance<ObjectType>(pObject, baseTransform, name));
        }

        /** Constructs a object instance with a transform
            \param[in] pObject Object to create an instance of
            \param[in] translation Base translation of the instance
            \param[in] target Base look-at target of the instance
            \param[in] up Base up vector of the instance
            \param[in] scale Base scale of the instance
            \param[in] setBaseTransform If true, initializes base transform with the transform arguments. Otherwise base transform is identity, and arguments are applied to the instance.
            \param[in] name Name of the instance
            \return A new instance of the object
        */
        static SharedPtr create(const typename ObjectType::SharedPtr& pObject, const glm::vec3& translation, const glm::vec3& target, const glm::vec3& up, const glm::vec3& scale, const std::string& name = "")
        {
            if (setBaseTransform)
            {
                mBase
                return create(pObject, calculateTransformMatrix(translation, target, up, scale), name);
            }
            else
            {
                // #CR Apply in scene importer
                SharedPtr pInstance = create(pObject, glm::mat4(), name);
                pInstance->setTranslation(translation, false);
                pInstance->mTarget = target;
                pInstance->mUp = up;
                pInstance->setScaling(scale);
                return pInstance;
            }
        }

        /** Constructs a object instance with a transform
            \param[in] pObject Object to create an instance of
            \param[in] translation Base translation of the instance
            \param[in] rotation Euler angle rotations of the instance
            \param[in] scale Base scale of the instance
            \param[in] setBaseTransform If true, initializes base transform with the transform arguments. Otherwise base transform is identity, and arguments are applied to the instance.
            \param[in] name Name of the instance
            \return A new instance of the object
        */
        static SharedPtr create(const typename ObjectType::SharedPtr& pObject, const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale, bool setBaseTransform = true, const std::string& name = "")
        {
            if (setBaseTransform)
            {
                return create(pObject, calculateTransformMatrix(translation, rotation, scale), name);
            }
            else
            {
                SharedPtr pInstance = create(pObject, glm::mat4(), name);
                pInstance->setTranslation(translation, false);
                pInstance->setRotation(rotation);
                pInstance->setScaling(scale);
                return pInstance;
            }
        }

        /** Gets object for which this is an instance of
            \return Object for this instance
        */
        const typename ObjectType::SharedPtr& getObject() const { return mpObject; };

        /** Sets visibility of this instance
            \param[in] visible Visibility of this instance
        */
        void setVisible(bool visible) { mVisible = visible; };

        /** Gets whether this instance is visible
            \return Whether this instance is visible
        */
        bool isVisible() const { return mVisible; };

        /** Gets instance name
            \return Instance name
        */
        const std::string& getName() const { return mName; }

        /** Sets instance name
            \param[in] name Instance name
        */
        void setName(const std::string& name) { mName = name; }

        /** Sets position/translation of the instance
            \param[in] translation Instance translation
            \param[in] updateLookAt If true, translates the look-at target as well to maintain rotation
        */
        void setTranslation(const glm::vec3& translation, bool updateLookAt)
        {
            if (updateLookAt)
            {
                glm::vec3 toLookAt = mTarget - mTranslation;
                mTarget = translation + toLookAt;
            }

            mTranslation = translation;
            mFinalTransformDirty = true;
        };

        /** Gets the position/translation of the instance
            \return Translation of the instance
        */
        const glm::vec3& getTranslation() const { return mTranslation; };

        /** Sets scale of the instance
            \param[in] scaling Instance scale
        */
        void setScaling(const glm::vec3& scaling) { mScale = scaling; mFinalTransformDirty = true; }

        /** Gets scale of the instance
            \return Scale of the instance
        */
        const glm::vec3& getScaling() const { return mScale; }

        /** Sets orientation of the instance
            \param[in] rotation Euler angles of rotation
        */
        void setRotation(const glm::vec3& rotation)
        {
            // Construct matrix from Euler angles and take upper 3x3
            const glm::mat3 rotMtx(glm::yawPitchRoll(rotation[1], rotation[0], rotation[2]));

            glm::vec3 up = rotMtx * glm::vec3(0, 1, 0);
            glm::vec3 forward = rotMtx * glm::vec3(0, 0, -1);

            // Get look-at info
            mUp = up;
            mTarget = mTranslation + forward; // position + forward

            mFinalTransformDirty = true;
        }

        /** Gets Euler angle rotations for the instance
            \return Vec3 containing Euler angle rotations
        */
        glm::vec3 getEulerRotation() const 
        {
            glm::vec3 result;

            glm::mat4 rotationMtx = glm::lookAt(glm::vec3(), mTarget - mTranslation, mUp);
            glm::extractEulerAngleXYZ(rotationMtx, result[0], result[1], result[2]);

            return -result;
        }

        /** Gets the up vector of the instance
            \return Up vector
        */
        const glm::vec3& getUpVector() const { return mUp; }

        /** Gets look-at target of the instance's orientation
            \return Look-at target position
        */
        const glm::vec3& getTarget() const { return mTarget; }

        /** Gets the transform matrix
            \return Transform matrix
        */
        const glm::mat4& getTransformMatrix() const
        {
            updateInstanceProperties();
            return mFinalTransformMatrix;
        }

        /** Gets the bounding box
            \return Bounding box
        */
        const BoundingBox& getBoundingBox() const
        {
            updateInstanceProperties();
            return mBoundingBox;
        }

        /** IMovableObject interface
        */
        virtual void move(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) override
        {
            mAdditionalTransformMatrix = calculateTransformMatrix(position, target, up, glm::vec3(1.0f));
            mFinalTransformDirty = true;
        }

    private:

        void updateInstanceProperties() const
        {
            if (mFinalTransformDirty == true)
            {
                mFinalTransformMatrix = mAdditionalTransformMatrix * calculateTransformMatrix(mTranslation, mTarget, mUp, mScale) * mBaseTransformMatrix;
                mBoundingBox = mpObject->getBoundingBox().transform(mFinalTransformMatrix);

                mFinalTransformDirty = false;
            }
        }

        static glm::mat4 calculateTransformMatrix(const glm::vec3& translation, const glm::vec3& target, const glm::vec3& up, const glm::vec3& scale)
        {
            glm::mat4 translationMtx = glm::translate(glm::mat4(), translation);
            glm::mat4 rotationMtx = glm::lookAt(glm::vec3(), target - translation, up);
            glm::mat4 scalingMtx = glm::scale(glm::mat4(), scale);

            return translationMtx * rotationMtx * scalingMtx;
        }

        static glm::mat4 calculateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
        {
            glm::mat4 translationMtx = glm::translate(glm::mat4(), translation);
            glm::mat4 rotationMtx = glm::yawPitchRoll(rotation[1], rotation[0], rotation[2]);
            glm::mat4 scalingMtx = glm::scale(glm::mat4(), scale);

            return translationMtx * rotationMtx * scalingMtx;
        }

        ObjectInstance(const typename ObjectType::SharedPtr& pObject, const glm::mat4& baseTransform, const std::string& name)
            : mpObject(pObject)
            , mName(name)
            , mUp(glm::vec3(0.0f, 1.0f, 0.0f))
            , mTarget(glm::vec3(0.0f, 0.0f, -1.0f))
            , mScale(glm::vec3(1.0f, 1.0f, 1.0f))
            , mBaseTransformMatrix(baseTransform)
        {
        }

        friend class Model;

        std::string mName;
        bool mVisible = true;

        typename ObjectType::SharedPtr mpObject;


        // #CR mesh loads to matrix, scene loads model instances to components, then calculates matrix
        struct 
        {
            glm::vec3 mTranslation;
            glm::vec3 up;
            glm::vec3 target;
            glm::vec3 scale;
            glm::mat4 matrix;
        } mBase, mMovable;

        mutable glm::mat4 mFinalTransformMatrix;
        mutable BoundingBox mBoundingBox;
        mutable bool mFinalTransformDirty = true;
    };
}
