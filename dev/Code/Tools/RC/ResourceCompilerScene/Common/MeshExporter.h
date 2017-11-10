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

#include <CryHeaders.h>
#include <SceneAPI/SceneCore/Components/ExportingComponent.h>

class CMesh;

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IMeshData;
        }
    }
    namespace RC
    {
        struct NodeExportContext;

        class MeshExporter
            : public SceneAPI::SceneCore::ExportingComponent
        {
        public:
            AZ_COMPONENT(MeshExporter, "{1F826DB8-D6B0-4392-90C8-8F6E63F649CA}", SceneAPI::SceneCore::ExportingComponent);

            MeshExporter();
            ~MeshExporter() override = default;

            static void Reflect(ReflectContext* context);

            SceneAPI::Events::ProcessingResult ProcessMesh(NodeExportContext& context) const;

        protected:
#if defined(AZ_COMPILER_MSVC) && AZ_COMPILER_MSVC <= 1800
            // Workaround for VS2013 - Delete the copy constructor and make it private
            // https://connect.microsoft.com/VisualStudio/feedback/details/800328/std-is-copy-constructible-is-broken
            MeshExporter(const MeshExporter&) = delete;
#endif
            void SetMeshFaces(const SceneAPI::DataTypes::IMeshData& meshData, CMesh& mesh, EPhysicsGeomType physicalizeType) const;
            bool SetMeshVertices(const SceneAPI::DataTypes::IMeshData& meshData, CMesh& mesh) const;
            bool SetMeshNormals(const SceneAPI::DataTypes::IMeshData& meshData, CMesh& mesh) const;
            void SetMeshTopologyIds(const SceneAPI::DataTypes::IMeshData& meshData, CMesh& mesh, NodeExportContext& context) const;
        };
    } // namespace RC
} // namespace AZ
