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

#include <AzCore/EBus/EBus.h>

#include <LyShine/Animation/IUiAnimation.h>

struct IUiAnimNode;

namespace AZ
{
    class Entity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiAnimNodeInterface
    : public AZ::EBusTraits
{
public:

    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
    typedef IUiAnimNode* BusIdType;
    //! Only one implementation for an IAnimNode* can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
    //////////////////////////////////////////////////////////////////////////

public: // member functions

    virtual ~UiAnimNodeInterface() {}

    virtual AZ::EntityId GetAzEntityId() = 0;
    virtual void SetAzEntity(AZ::Entity* entity) = 0;
};

typedef AZ::EBus<UiAnimNodeInterface> UiAnimNodeBus;

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiAnimationInterface
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiAnimationInterface() {}

    //! Start a sequence
    virtual void StartSequence(const AZStd::string& sequenceName) = 0;

    //! Stop a sequence
    virtual void StopSequence(const AZStd::string& sequenceName) = 0;

    //! Abort a sequence
    virtual void AbortSequence(const AZStd::string& sequenceName) = 0;

    //! Pause a sequence
    virtual void PauseSequence(const AZStd::string& sequenceName) = 0;

    //! Resume a sequence
    virtual void ResumeSequence(const AZStd::string& sequenceName) = 0;

    //! Reset a sequence
    virtual void ResetSequence(const AZStd::string& sequenceName) = 0;

    //! Get the speed of a sequence
    virtual float GetSequencePlayingSpeed(const AZStd::string& sequenceName) = 0;

    //! Set the speed of a sequence
    virtual void SetSequencePlayingSpeed(const AZStd::string& sequenceName, float speed) = 0;

    //! Get the current time of a sequence
    virtual float GetSequencePlayingTime(const AZStd::string& sequenceName) = 0;

    //! Get whether a sequence is currently playing
    virtual bool IsSequencePlaying(const AZStd::string& sequenceName) = 0;

public: // static member data

    //! Only one component on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
};

typedef AZ::EBus<UiAnimationInterface> UiAnimationBus;

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiAnimationNotifications
    : public AZ::ComponentBus
{
public:

    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides
    static const bool EnableEventQueue = true;
    //////////////////////////////////////////////////////////////////////////

public: // member functions

    virtual ~UiAnimationNotifications(){}

    //! Called on an animation event
    virtual void OnUiAnimationEvent(IUiAnimationListener::EUiAnimationEvent uiAnimationEvent, AZStd::string animSequenceName) = 0;
};

typedef AZ::EBus<UiAnimationNotifications> UiAnimationNotificationBus;
