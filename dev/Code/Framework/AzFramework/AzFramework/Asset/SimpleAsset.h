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

#ifndef AZFRAMEWORK_SIMPLEASSET_H
#define AZFRAMEWORK_SIMPLEASSET_H

#include <AzCore/base.h>

#pragma once

/*!
 * Asset references are simply game-folder relative paths.
 * This will change as the asset system comes online, but in the mean time
 * we need something we can reflect and use intuitively in the editor.
 *
 * Asset types are a simple class with a required API, e.g.:
 *
 * class MyAsset
 * {
 *  static const char* GetName() { return "MyAsset"; }
 *  static const char* GetFileFilter() { return "*.myasset;*.myasset2"; }
 *  static const char* GetUuid() { return "{00000000-0000-0000-0000-000000000000}"; }
 * }
 *
 * You must register your asset type's information with the environment
 * and serialization context:
 * SimpleAssetReference<MyAsset>::Register(serializeContext);
 *
 * You can now reflect references to your asset from components, etc. e.g.:
 * In class header:
 *  AzFramework::SimpleAssetReference<MyAsset> m_asset;
 * In reflection:
 *  ->DataElement("SimpleAssetRef", &MeshComponent::m_meshAsset, "My Asset", "The asset to use")g
 *
 * "SimpleAssetRef" tells the UI to use the corresponding widget.
 * UI code will make use of your registered asset information to browse for the correct file types.
 */

#include <AzCore/std/string/string.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/Component.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/typetraits/alignment_of.h>
#include <AzCore/Asset/AssetCommon.h>

namespace AzFramework
{
    /*!
     * Base class for templated asset reference types.
     * - Handles storage of the game-relative asset path.
     * - Handles reflection of reference type for use in serialization/editing.
     */
    class SimpleAssetReferenceBase
    {
    public:

        static const int kMaxVariableNameLength = 128;

        virtual ~SimpleAssetReferenceBase() { }

        AZ_CLASS_ALLOCATOR(SimpleAssetReferenceBase, AZ::SystemAllocator, 0);
        AZ_RTTI(SimpleAssetReferenceBase, "{E16CA6C5-5C78-4AD9-8E9B-F8C1FB4D1DB8}");

        const AZStd::string& GetAssetPath() const { return m_assetPath; }
        void SetAssetPath(const char* path) { m_assetPath = path; }

        virtual AZ::Data::AssetType GetAssetType() const = 0;
        virtual const char* GetFileFilter() const = 0;

        static void Reflect(AZ::SerializeContext& context)
        {
            context.Class<SimpleAssetReferenceBase>()
                ->Version(1)
                ->Field("AssetPath", &SimpleAssetReferenceBase::m_assetPath);

            AZ::EditContext* edit = context.GetEditContext();
            if (edit)
            {
                edit->Class<SimpleAssetReferenceBase>("Asset path", "Asset reference as a project-relative path")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)
                    ;
            }
        }

    protected:

        AZStd::string m_assetPath;
    };

    using AssetInfoString = AZStd::basic_string<
                char,
                AZStd::char_traits<char>,
                AZStd::static_buffer_allocator<128, AZStd::alignment_of<char>::value> >;

    /*!
     * Templated asset reference type.
     * This currently acts as a convenience helper for registering
     * asset type information with the environment.
     * e.g. SimpleAssetReference<Mesh>::Register();
     */
    template<typename AssetType>
    class SimpleAssetReference
        : public SimpleAssetReferenceBase
    {
    public:

        AZ_CLASS_ALLOCATOR(SimpleAssetReference<AssetType>, AZ::SystemAllocator, 0);
        AZ_RTTI((SimpleAssetReference<AssetType>, "{D03D0CF6-9A61-4DBA-AC53-E62453CE940D}", AssetType), SimpleAssetReferenceBase);

        static void Register(AZ::SerializeContext& context)
        {
            RegisterAssetTypeName();
            RegisterAssetTypeFileFilter();

            context.Class<SimpleAssetReference<AssetType>, SimpleAssetReferenceBase>()
                ->Version(1)
            ;
        }

        virtual AZ::Data::AssetType GetAssetType() const { return AZ::Data::AssetType(AZ::AzTypeInfo<AssetType>::Uuid()); }
        virtual const char* GetFileFilter() const { return AssetType::GetFileFilter(); }

    private:

        static void RegisterAssetTypeName()
        {
            char varName[SimpleAssetReferenceBase::kMaxVariableNameLength];
            azsnprintf(varName, SimpleAssetReferenceBase::kMaxVariableNameLength, "assetname%s",
                AZ::AzTypeInfo<AssetType>::Uuid().template ToString<AZStd::string>().c_str());

            s_name = AZ::Environment::FindVariable<AssetInfoString>(varName);
            if (!s_name)
            {
                s_name = AZ::Environment::CreateVariable<AssetInfoString>(varName);
                AZ_Assert(s_name, "Could not create an evironmental variable with name '%s'", varName);
            }

            (*s_name) = AssetType::TYPEINFO_Name();
        }

        static void RegisterAssetTypeFileFilter()
        {
            char varName[SimpleAssetReferenceBase::kMaxVariableNameLength];
            azsnprintf(varName, SimpleAssetReferenceBase::kMaxVariableNameLength, "assetfilter%s",
                AZ::AzTypeInfo<AssetType>::Uuid().template ToString<AZStd::string>().c_str());

            s_filter = AZ::Environment::FindVariable<AssetInfoString>(varName);
            if (!s_filter)
            {
                s_filter = AZ::Environment::CreateVariable<AssetInfoString>(varName);
                AZ_Assert(s_filter, "Could not create an evironmental variable with name '%s'", varName);
            }

            (*s_filter) = AssetType::GetFileFilter();
        }

        static AZ::EnvironmentVariable<AssetInfoString> s_name;
        static AZ::EnvironmentVariable<AssetInfoString> s_filter;
    };

    template<typename AssetType>
    AZ::EnvironmentVariable<AssetInfoString> SimpleAssetReference<AssetType>::s_name;
    template<typename AssetType>
    AZ::EnvironmentVariable<AssetInfoString> SimpleAssetReference<AssetType>::s_filter;

    /*!
     * Retrieves the name of an asset by asset type (which is actually a name Crc).
     * This information is stored in the environment, so it's accessible from any module.
     */
    const char* SimpleAssetTypeGetName(const AZ::Data::AssetType& assetType);

    /*!
     * Retrieves the file filter for an asset type.
     * This information is stored in the environment, so it's accessible from any module.
     */
    const char* SimpleAssetTypeGetFileFilter(const AZ::Data::AssetType& assetType);

} // namespace AzFramework

#endif // AZFRAMEWORK_SIMPLEASSET_H
