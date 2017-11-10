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

#include <AzCore/Casting/numeric_cast.h>
#include <SceneAPI/FbxSceneBuilder/FbxSceneSystem.h>
#include <SceneAPI/FbxSceneBuilder/Importers/Utilities/FbxMeshImporterUtilities.h>
#include <SceneAPI/FbxSDKWrapper/FbxMeshWrapper.h>
#include <SceneAPI/SceneData/GraphData/MeshData.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace FbxSceneBuilder
        {
            bool BuildSceneMeshFromFbxMesh(const AZStd::shared_ptr<SceneData::GraphData::MeshData>& mesh,
                const FbxSDKWrapper::FbxMeshWrapper& sourceMesh, const FbxSceneSystem& sceneSystem)
            {
                // Get mesh subset count by scanning material IDs in meshes.
                // For negative material ids we will add an additional
                // subset at the end, see "++maxMaterialIndex".

                // These defines material index range for all polygons in the mesh
                // Each polygon has a material index
                int minMeshMaterialIndex = INT_MAX;
                int maxMeshMaterialIndex = INT_MIN;

                FbxLayerElementArrayTemplate<int>* fbxMaterialIndices;
                sourceMesh.GetMaterialIndices(&fbxMaterialIndices); // per polygon

                int fbxPolygonCount = sourceMesh.GetPolygonCount();
                for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                {
                    // if the polygon has less than 3 vertices, it's not a valid polygon and is skipped
                    const int fbxPolygonVertexCount = sourceMesh.GetPolygonSize(fbxPolygonIndex);
                    if (fbxPolygonVertexCount <= 2)
                    {
                        continue;
                    }

                    // Get the material index of each polygon
                    const int meshMaterialIndex = fbxMaterialIndices ? (*fbxMaterialIndices)[fbxPolygonIndex] : -1;
                    minMeshMaterialIndex = AZ::GetMin<int>(minMeshMaterialIndex, meshMaterialIndex);
                    maxMeshMaterialIndex = AZ::GetMax<int>(maxMeshMaterialIndex, meshMaterialIndex);
                }

                if (minMeshMaterialIndex > maxMeshMaterialIndex)
                {
                    return false;
                }

                if (maxMeshMaterialIndex < 0)
                {
                    minMeshMaterialIndex = maxMeshMaterialIndex = 0;
                }
                else if (minMeshMaterialIndex < 0)
                {
                    minMeshMaterialIndex = 0;
                    ++maxMeshMaterialIndex;
                }

                // Fill geometry

                // Control points contain positions of vertices
                AZStd::vector<Vector3> fbxControlPoints = sourceMesh.GetControlPoints();
                const int* const fbxPolygonVertices = sourceMesh.GetPolygonVertices();

                fbxMaterialIndices = nullptr;
                sourceMesh.GetMaterialIndices(&fbxMaterialIndices); // per polygon

                // Iterate through each polygon in the mesh and convert data
                fbxPolygonCount = sourceMesh.GetPolygonCount();
                for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
                {
                    const int fbxPolygonVertexCount = sourceMesh.GetPolygonSize(fbxPolygonIndex);
                    if (fbxPolygonVertexCount <= 2)
                    {
                        continue;
                    }

                    AZ_TraceContext("Polygon Index", fbxPolygonIndex);

                    // Ensure the validity of the material index for the polygon
                    int fbxMaterialIndex = fbxMaterialIndices ? (*fbxMaterialIndices)[fbxPolygonIndex] : -1;
                    if (fbxMaterialIndex < minMeshMaterialIndex || fbxMaterialIndex > maxMeshMaterialIndex)
                    {
                        fbxMaterialIndex = maxMeshMaterialIndex;
                    }

                    const int fbxVertexStartIndex = sourceMesh.GetPolygonVertexIndex(fbxPolygonIndex);

                    // Triangulate polygon as a fan and remember resulting vertices and faces

                    int firstMeshVertexIndex = -1;
                    int previousMeshVertexIndex = -1;

                    AZ::SceneAPI::DataTypes::IMeshData::Face meshFace;
                    int verticesInMeshFace = 0;

                    // Iterate through each vertex in the polygon
                    for (int vertexIndex = 0; vertexIndex < fbxPolygonVertexCount; ++vertexIndex)
                    {
                        const int meshVertexIndex = aznumeric_caster(mesh->GetVertexCount());

                        const int fbxPolygonVertexIndex = fbxVertexStartIndex + vertexIndex;
                        const int fbxControlPointIndex = fbxPolygonVertices[fbxPolygonVertexIndex];

                        Vector3 meshPosition = fbxControlPoints[fbxControlPointIndex];

                        Vector3 meshVertexNormal;
                        sourceMesh.GetPolygonVertexNormal(fbxPolygonIndex, vertexIndex, meshVertexNormal);

                        sceneSystem.SwapVec3ForUpAxis(meshPosition);
                        sceneSystem.ConvertUnit(meshPosition);
                        // Add position
                        mesh->AddPosition(meshPosition);

                        // Add normal
                        sceneSystem.SwapVec3ForUpAxis(meshVertexNormal);
                        meshVertexNormal.Normalize();
                        mesh->AddNormal(meshVertexNormal);

                        mesh->SetVertexIndexToControlPointIndexMap(meshVertexIndex, fbxControlPointIndex);

                        // Add face
                        {
                            if (vertexIndex == 0)
                            {
                                firstMeshVertexIndex = meshVertexIndex;
                            }

                            int meshVertices[3];
                            int meshVertexCount = 0;

                            meshVertices[meshVertexCount++] = meshVertexIndex;

                            // If we already have generated one triangle before, make a new triangle at a time as we encounter a new vertex.
                            // The new triangle is composed with the first vertex of the polygon, the last vertex, and the current vertex.
                            if (vertexIndex >= 3)
                            {
                                meshVertices[meshVertexCount++] = firstMeshVertexIndex;
                                meshVertices[meshVertexCount++] = previousMeshVertexIndex;
                            }

                            for (int faceVertexIndex = 0; faceVertexIndex < meshVertexCount; ++faceVertexIndex)
                            {
                                meshFace.vertexIndex[verticesInMeshFace++] = meshVertices[faceVertexIndex];
                                if (verticesInMeshFace == 3)
                                {
                                    verticesInMeshFace = 0;
                                    mesh->AddFace(meshFace, fbxMaterialIndex);
                                }
                            }
                        }

                        previousMeshVertexIndex = meshVertexIndex;
                    }

                    // Report problem if there are vertices that left for forming a polygon
                    if (verticesInMeshFace != 0)
                    {
                        AZ_TracePrintf(Utilities::ErrorWindow, "Internal error in mesh filler");
                        return false;
                    }
                }

                // Report problem if no vertex or face converted to MeshData
                if (mesh->GetVertexCount() <= 0 || mesh->GetFaceCount() <= 0)
                {
                    AZ_TracePrintf(Utilities::ErrorWindow, "Missing geometry data in mesh node");
                    return false;
                }

                return true;
            }
        }
    }
}
