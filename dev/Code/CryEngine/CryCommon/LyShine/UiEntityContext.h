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

#include <AzCore/Math/Uuid.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Slice/SliceComponent.h>

#include <AzFramework/Entity/EntityContext.h>
#include <AzFramework/Asset/AssetCatalogBus.h>

#include <LyShine/Bus/UiEntityContextBus.h>

namespace AZ
{
    class SerializeContext;
}

namespace AzFramework
{
    class EntityContext;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! The UI Entity Context stores the prefab asset for the root slice of a UI canvas
//! So all of the UI element entities in a canvas are owned indirectly by the context and managed
//! by the entity context.
class UiEntityContext
    : public AzFramework::EntityContext
    , public UiEntityContextRequestBus::Handler
{
public: // member functions

    //! Initialize the entity context and instantiate the root slice
    virtual void InitUiContext() = 0;

    //! Destroy the Entity Context
    virtual void DestroyUiContext() = 0;

    //! Get the Entity for the root asset
    AZ::Entity* GetRootAssetEntity() { return m_rootAsset.Get()->GetEntity(); }

    //! Saves the context's slice root to the specified buffer. If necessary
    //! entities undergo conversion for game: editor -> game components.
    //! \return true if successfully saved. Failure is only possible if serialization data is corrupt.
    virtual bool SaveToStreamForGame(AZ::IO::GenericStream& stream, AZ::DataStream::StreamType streamType) = 0;
};
