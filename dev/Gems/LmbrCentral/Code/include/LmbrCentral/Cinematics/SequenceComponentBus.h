/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once

#include <AzFramework/Math/MathUtils.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/std/string/string.h>

namespace LmbrCentral
{
    /*!
    * SequenceComponentRequests EBus Interface
    * Messages serviced by SequenceComponents.
    */
    class SequenceComponentRequests
        : public AZ::ComponentBus
    {
    public:
        /**
         * helper class to define an animatable property address
         */
        class AnimatablePropertyAddress
        {
        public:
            AZ_TYPE_INFO(AnimatablePropertyAddress, "{CEE14802-F1E8-4C0A-9750-64C59C39ECE9}");

            AnimatablePropertyAddress(AZ::ComponentId componentId, const AZStd::string& virtualPropertyName)
                : m_componentId(componentId)
                , m_virtualPropertyName(virtualPropertyName) {}

            AnimatablePropertyAddress()
                : m_componentId(AZ::InvalidComponentId) {}

            const AZStd::string& GetVirtualPropertyName() const { return m_virtualPropertyName; }
            AZ::ComponentId      GetComponentId() const { return m_componentId; }

            bool operator== (const AnimatablePropertyAddress& rhs) const 
            { 
                return (m_componentId == rhs.m_componentId && m_virtualPropertyName == rhs.m_virtualPropertyName); 
            }

        private:
            AZ::ComponentId m_componentId;                  // componentId of the component being animated on the sequenceAgent's entity
            AZStd::string   m_virtualPropertyName;          // EBus virtual property name being animated on the component
        };

        /**
         * Interface for an animated value to abstract the type (i.e. float/Vector3/Bool) of the value.
         *
         * Inherited concrete subclasses determines the actual type of the animatedValue and fills in get/set methods
         * for casting to other types
         */
        // forward declarations
        class AnimatedFloatValue;
        class AnimatedVector3Value;
        class AnimatedBoolValue;
        class AnimatedQuaternionValue;

        class AnimatedValue
        {
        public:
            AZ_TYPE_INFO(AnimatedValue, "{5C4BBDD6-8F80-4510-B5B8-8FA0FBD101A6}");

            virtual ~AnimatedValue() {};

            // Query the type of the value
            virtual const AZ::Uuid& GetTypeId() const = 0;

            void GetValue(AZ::Vector3& vector3Value) const
            {
                vector3Value = GetVector3Value();
            }
            void GetValue(AZ::Quaternion& quaternionValue) const
            {
                quaternionValue = GetQuaternionValue();
            }
            void GetValue(float& floatValue) const
            {
                floatValue = GetFloatValue();
            }
            void GetValue(bool& boolValue) const
            {
                boolValue = GetBoolValue();
            }
            // same as above but returning the value
            virtual AZ::Quaternion GetQuaternionValue() const = 0;
            virtual AZ::Vector3 GetVector3Value() const = 0;
            virtual float       GetFloatValue() const = 0;
            virtual bool        GetBoolValue() const = 0;

            // Set the value to the given arg. Returns true if the arg is the 'native' type of the concrete animated value
            virtual bool SetValue(const AZ::Vector3& vector3Value) = 0;
            virtual bool SetValue(const AZ::Quaternion& quaternionValue) = 0;
            virtual bool SetValue(float floatValue) = 0;
            virtual bool SetValue(bool boolValue) = 0;

            virtual bool IsClose(const AnimatedFloatValue& rhs, float tolerance = AZ::g_simdTolerance) const = 0;
            virtual bool IsClose(const AnimatedVector3Value& rhs, float tolerance = AZ::g_simdTolerance) const = 0;
            virtual bool IsClose(const AnimatedQuaternionValue& rhs, float tolerance = AZ::g_simdTolerance) const = 0;
            virtual bool IsClose(const AnimatedBoolValue& rhs, float tolerance = AZ::g_simdTolerance) const = 0;

        protected:
            AnimatedValue() {}     // protected constructor as the interface should never be constructed directly - it's an abstract class
        };

        class AnimatedFloatValue
            : public AnimatedValue
        {
        public:
            AZ_TYPE_INFO(AnimatedFloatValue, "{2C90BCBB-1DF2-47C8-8193-18EFE1C70E20}");

            AnimatedFloatValue(float value = .0f) { m_value = value; }
            ~AnimatedFloatValue() {}

            const AZ::Uuid& GetTypeId() const override
            {
                return AZ::AzTypeInfo<float>::Uuid();
            }

            AZ::Vector3 GetVector3Value() const override
            {
                return AZ::Vector3(m_value);
            }
            AZ::Quaternion GetQuaternionValue() const override
            {
                return AZ::Quaternion(m_value);
            }
            float GetFloatValue() const override
            {
                return m_value;
            }
            bool GetBoolValue() const override
            {
                return (!AZ::IsClose(m_value, .0f, FLT_EPSILON));
            }

            bool SetValue(const AZ::Vector3& vector3Value) override
            {
                m_value = vector3Value.GetX();
                return false;
            }
            bool SetValue(const AZ::Quaternion& quaternionValue) override
            {
                m_value = quaternionValue.GetLength();
                return false;
            }
            bool SetValue(float floatValue) override
            {
                m_value = floatValue;
                return true;
            }
            bool SetValue(bool boolValue) override
            {
                m_value = boolValue ? 1.0f : .0f;
                return false;
            }

            bool IsClose(const AnimatedFloatValue& rhs, float tolerance = AZ::g_fltEps) const override
            {
                return AZ::IsClose(m_value, rhs.GetFloatValue(), tolerance);
            }
            bool IsClose(const AnimatedVector3Value& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return AZ::IsClose(m_value, rhs.GetFloatValue(), tolerance);
            }
            bool IsClose(const AnimatedQuaternionValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return AZ::IsClose(m_value, rhs.GetFloatValue(), tolerance);
            }
            bool IsClose(const AnimatedBoolValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return rhs.GetBoolValue() == (!AZ::IsClose(m_value, .0f, tolerance));
            }

        private:
            float m_value;
        };

        class AnimatedVector3Value
            : public AnimatedValue
        {
        public:
            AZ_TYPE_INFO(AnimatedVector3Value, "{B8CDD566-9D55-47B2-BF91-162E428B237E}");

            AnimatedVector3Value(const AZ::Vector3& value) { m_value = value; }
            ~AnimatedVector3Value() {}

            const AZ::Uuid& GetTypeId() const override
            {
                return AZ::Vector3::TYPEINFO_Uuid();
            }

            AZ::Vector3 GetVector3Value() const override
            {
                return m_value;
            }
            AZ::Quaternion GetQuaternionValue() const override
            {
                // treat m_value as Euler Angles in degrees
                return AzFramework::ConvertEulerDegreesToQuaternion(m_value);
            }
            float GetFloatValue() const override
            {
                return m_value.GetX(); // return the first component
            }
            bool GetBoolValue() const override
            {
                return !m_value.IsClose(AZ::Vector3::CreateZero());
            }

            bool SetValue(const AZ::Vector3& vector3Value) override
            {
                m_value = vector3Value;
                return true;
            }
            bool SetValue(const AZ::Quaternion& quaternionValue) override
            {
                m_value = AzFramework::ConvertQuaternionToEulerDegrees(quaternionValue);
                return true;
            }
            bool SetValue(float floatValue) override
            {
                m_value.Set(floatValue);    // sets vector components to floatValue
                return false;
            }
            bool SetValue(bool boolValue) override
            {
                m_value = boolValue ? AZ::Vector3::CreateOne() : AZ::Vector3::CreateZero();
                return false;
            }

            bool IsClose(const AnimatedFloatValue& rhs, float tolerance = AZ::g_fltEps) const override
            {
                return m_value.IsClose(rhs.GetVector3Value(), tolerance);
            }
            bool IsClose(const AnimatedVector3Value& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value.IsClose(rhs.GetVector3Value(), tolerance);
            }
            bool IsClose(const AnimatedQuaternionValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value.IsClose(rhs.GetVector3Value(), tolerance);
            }
            bool IsClose(const AnimatedBoolValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return rhs.GetBoolValue() == (!m_value.IsClose(AZ::Vector3::CreateZero(), tolerance));
            }
            
        private:
            AZ::Vector3 m_value;
        };

        class AnimatedQuaternionValue
            : public AnimatedValue
        {
        public:
            AZ_TYPE_INFO(AnimatedQuaternionValue, "{572E640B-9375-4E16-8F3A-5DCA1734B820}");

            AnimatedQuaternionValue(const AZ::Quaternion value) { m_value = value; }
            ~AnimatedQuaternionValue() {}

            const AZ::Uuid& GetTypeId() const override
            {
                return AZ::Quaternion::TYPEINFO_Uuid();
            }

            AZ::Vector3 GetVector3Value() const override
            {
                // convert Quaternion to Euler angles
                return AzFramework::ConvertQuaternionToEulerDegrees(m_value);
            }
            AZ::Quaternion GetQuaternionValue() const override
            {
                return m_value;
            }
            float GetFloatValue() const override
            {
                // return the length of the quat
                return m_value.GetLength(); 
            }
            bool GetBoolValue() const override
            {
                return !m_value.IsZero();
            }

            bool SetValue(const AZ::Vector3& vector3Value) override
            {
                // convert from Euler angles
                m_value = AzFramework::ConvertEulerRadiansToQuaternion(vector3Value);
                return false;
            }
            bool SetValue(const AZ::Quaternion& quaternionValue) override
            {
                m_value = quaternionValue;
                return true;
            }
            bool SetValue(float floatValue) override
            {
                m_value.Set(floatValue);    // sets quat with all components set to floatValue
                return false;
            }
            bool SetValue(bool boolValue) override
            {
                m_value = boolValue ? AZ::Quaternion::CreateIdentity() : AZ::Quaternion::CreateZero();
                return false;
            }

            bool IsClose(const AnimatedFloatValue& rhs, float tolerance = AZ::g_fltEps) const override
            {
                return m_value.IsClose(rhs.GetQuaternionValue(), tolerance);
            }
            bool IsClose(const AnimatedVector3Value& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value.IsClose(rhs.GetQuaternionValue(), tolerance);
            }
            bool IsClose(const AnimatedQuaternionValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value.IsClose(rhs.m_value, tolerance);
            }
            bool IsClose(const AnimatedBoolValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return rhs.GetBoolValue() == (!m_value.IsZero(tolerance));
            }

        private:
            AZ::Quaternion m_value;
        };

        class AnimatedBoolValue
            : public AnimatedValue
        {
        public:
            AZ_TYPE_INFO(AnimatedBoolValue, "{5FF422AD-20E7-4109-A2EA-4AACE8213860}");

            AnimatedBoolValue(bool value) { m_value = value; }
            ~AnimatedBoolValue() {}

            const AZ::Uuid& GetTypeId() const override
            {
                return AZ::AzTypeInfo<bool>::Uuid();
            }

            AZ::Vector3 GetVector3Value() const override
            {
                return m_value ? AZ::Vector3::CreateOne() : AZ::Vector3::CreateZero();;
            }
            AZ::Quaternion GetQuaternionValue() const override
            {
                return m_value ? AZ::Quaternion::CreateIdentity() : AZ::Quaternion::CreateZero();;
            }
            float GetFloatValue() const override
            {
                return m_value ? 1.0f : .0f;
            }
            bool GetBoolValue() const override
            {
                return m_value;
            }

            bool SetValue(const AZ::Vector3& vector3Value) override
            {
                m_value = !vector3Value.IsClose(AZ::Vector3::CreateZero());
                return false;
            }
            bool SetValue(const AZ::Quaternion& quaternionValue) override
            {
                m_value = !quaternionValue.IsZero();
                return false;
            }
            bool SetValue(float floatValue) override
            {
                m_value = !AZ::IsClose(floatValue, .0f, FLT_EPSILON);
                return false;
            }
            bool SetValue(bool boolValue) override
            {
                m_value = boolValue;
                return true;
            }

            bool IsClose(const AnimatedFloatValue& rhs, float tolerance = AZ::g_fltEps) const override
            {
                return m_value == (!AZ::IsClose(rhs.GetFloatValue(), .0f, tolerance));
            }
            bool IsClose(const AnimatedVector3Value& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value == (!rhs.GetVector3Value().IsClose(AZ::Vector3::CreateZero(), tolerance));
            }
            bool IsClose(const AnimatedQuaternionValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                return m_value == (!rhs.GetQuaternionValue().IsZero(tolerance));
            }
            bool IsClose(const AnimatedBoolValue& rhs, float tolerance = AZ::g_simdTolerance) const override
            {
                (void)tolerance;                    // avoid compiler warning for unused variable
                return m_value == rhs.m_value;
            }

        private:
            bool m_value;
        };

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides - application is a singleton
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;  // Only one component on a entity can implement the events
        //////////////////////////////////////////////////////////////////////////

        /** 
         * Set a value for an animated property at the given address on the given entity.
         * @param animatedEntityId the entity Id of the entity containing the animatedAddress
         * @param animatedAddress identifies the component and property to be set
         * @param value the value to set - this must be instance of one of the concrete subclasses of AnimatedValue, corresponding to the type of the property referenced by the animatedAdresss
         * @return true if the value was changed.
         */
        virtual bool SetAnimatedPropertyValue(const AZ::EntityId& animatedEntityId, const AnimatablePropertyAddress& animatableAddress, const AnimatedValue& value) = 0;

        /**
         * Get the current value for a property
         * @param returnValue holds the value to get - this must be instance of one of the concrete subclasses of AnimatedValue, corresponding to the type of the property referenced by the animatedAdresss
         * @param animatedEntityId the entity Id of the entity containing the animatedAddress
         * @param animatedAddress identifies the component and property to be set
         */
        virtual void GetAnimatedPropertyValue(AnimatedValue& returnValue, const AZ::EntityId& animatedEntityId, const AnimatablePropertyAddress& animatableAddress) = 0;

        /** Returns the Uuid of the type for the property at the animatableAddress on the given entityId
        */
        virtual AZ::Uuid GetAnimatedAddressTypeId(const AZ::EntityId& enityId, const LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress& animatableAddress) = 0;

        //////////////////////////////////////////////////////////////////////////
        // Behaviors
        /**
        * Play sequence from the start to end times set the sequence
        */
        virtual void Play() {}
        /**
        * Play sequence between the start to end times, outside of which the sequence behaves according to its 'Out of Range' time setting
        * @param startTime                     Sequence start time in seconds
        * @param endTime                       Sequence end time in seconds
        */
        virtual void PlayBetweenTimes(float startTime, float endTime) {(void)startTime; (void)endTime;}
        /**
         * Stop the sequence. Stopping the sequence jumps the play time to the end of the sequence.
         */
        virtual void Stop() {}
        /**
        * Pause the sequence. Sequence must be playing for pause to have an effect. Pausing leaves the play time at its current position.
        */
        virtual void Pause() {}
        /**
        * Resume the sequence. Resume essentially 'unpauses' a sequence. It must have been playing before the pause for playback to start again.
        */
        virtual void Resume() {}
        /**
        * Set the play speed
        * @param newSpeed speed multiplier (1.0 is normal speed, less is slower, more is faster).
        */
        virtual void SetPlaySpeed(float newSpeed) {(void)newSpeed;}
        /**
         * Move the Playhead to the given time
         * @param time time to move the play head to, in seconds, clamped to be between the start and end times of the sequence
         */
        virtual void JumpToTime(float newTime) {(void)newTime;}
        /**
        * Move the Playhead to the end of the sequence
        */
        virtual void JumpToEnd() {}
        /**
        * Move the Playhead to the beginning of the sequence
        */
        virtual void JumpToBeginning() {}
        /**
         * Returns the current play time in seconds
         */
        virtual float GetCurrentPlayTime() { return .0f; }
        /**
        * Returns the current play back speed as a multiplier (1.0 is normal speed, less is slower, more is faster).
        */
        virtual float GetPlaySpeed() { return 1.0f; }
        //////////////////////////////////////////////////////////////////////////
    };
    using SequenceComponentRequestBus = AZ::EBus<SequenceComponentRequests>;

    /**
    * Notifications from the Sequence Component
    */
    class SequenceComponentNotification
        : public AZ::ComponentBus
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits
        // multiple handlers (addressed by AZ::EntityId inherited from AZ::ComponentBus)
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        ////////////////////////////////////////////////////////////////////////

        virtual ~SequenceComponentNotification() {}

        /**
        * Called when Sequence starts
        * @param startTime Time when sequence starts, in seconds
        */
        virtual void OnStart(float startTime) { (void)startTime; }
        /**
        * Called when Sequence stops
        * @param stopTime Time when the sequence stops, in seconds
        */
        virtual void OnStop(float stopTime) { (void)stopTime; }
        /**
        * Called when Sequence pauses
        */
        virtual void OnPause() {}
        /**
        * Called when Sequence resumes
        */
        virtual void OnResume() {}
        /**
        * Called when Sequence is aborted
        * @param abortTime Time when the sequence aborts, in seconds
        */
        virtual void OnAbort(float abortTime) { (void)abortTime; }
        /**
        * Called when Sequence is updated. That is, when the current play time changes, or the playback speed changes.
        * @param updateTime Time when the sequence updates, in seconds
        */
        virtual void OnUpdate(float updateTime) { (void)updateTime; }
        /**
        * Called when a Sequence Event is triggered.
        * @param paramValue The Event Key's Value parameter as a string
        */
        virtual void OnTrackEventTriggered(const char* eventName, const char* eventValue) { (void)eventName;  (void)eventValue; }
    };

    using SequenceComponentNotificationBus = AZ::EBus<SequenceComponentNotification>;
} // namespace LmbrCentral

namespace AZStd
{
    template <>
    struct hash < LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress >
    {
        inline size_t operator()(const LmbrCentral::SequenceComponentRequests::AnimatablePropertyAddress& animatablePropertyAddress) const
        {
            AZStd::hash<AZ::ComponentId> componentIdHasher;
            size_t retVal = componentIdHasher(animatablePropertyAddress.GetComponentId());
            AZStd::hash_combine(retVal, animatablePropertyAddress.GetVirtualPropertyName());
            return retVal;
        }
    };
}  // namespace AZStd