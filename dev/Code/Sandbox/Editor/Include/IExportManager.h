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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : Export geometry interfaces


#ifndef CRYINCLUDE_EDITOR_INCLUDE_IEXPORTMANAGER_H
#define CRYINCLUDE_EDITOR_INCLUDE_IEXPORTMANAGER_H
#pragma once

#define EXP_NAMESIZE 32
struct IStatObj;
namespace Export
{
    struct Vector3D
    {
        float x, y, z;
    };


    struct Quat
    {
        Vector3D v;
        float w;
    };


    struct UV
    {
        float u, v;
    };


    struct Face
    {
        uint32 idx[3];
    };


    struct Color
    {
        float r, g, b, a;
    };

    typedef char TPath[_MAX_PATH];

    struct Material
    {
        Color diffuse;
        Color specular;
        float opacity;
        float smoothness;
        char name[EXP_NAMESIZE];
        TPath mapDiffuse;
        TPath mapSpecular;
        TPath mapOpacity;
        TPath mapNormals;
        TPath mapDecal;
        TPath mapDisplacement;
    };


    struct Mesh
    {
        Material material;

        virtual int GetFaceCount() const = 0;
        virtual const Face* GetFaceBuffer() const = 0;
    };

    // The numbers in this enum list must reflect the one from IMovieSystem.h
    enum EAnimParamType
    {
        eAnimParamType_FOV                          = 0,
        eAnimParamType_PositionX                        = 51,
        eAnimParamType_PositionY                        = 52,
        eAnimParamType_PositionZ                        = 53,
        eAnimParamType_RotationX                        = 54,
        eAnimParamType_RotationY                        = 55,
        eAnimParamType_RotationZ                        = 56,

        // FocalLength is an exceptional case for FBX importing from Maya. In engine we use FoV, not Focal Length, therefore
        // there is no equivalent eAnimParamType_FocalLength in IMovieSystem.h. However we enumerate it here so we can detect
        // and convert it to FoV during import
        eAnimParamType_FocalLength,
    };

    enum EEntityObjectType
    {
        eEntity = 0,
        eCamera = 1,
        eCameraTarget = 2,
    };

    struct EntityAnimData
    {
        EAnimParamType dataType;
        float keyTime;
        float keyValue;
        float leftTangent;
        float rightTangent;
        float leftTangentWeight;
        float rightTangentWeight;
    };

    struct Object
    {
        Vector3D pos;
        Quat rot;
        Vector3D scale;
        char name[EXP_NAMESIZE];
        char materialName[EXP_NAMESIZE];
        int nParent;
        EEntityObjectType entityType;
        char cameraTargetNodeName[EXP_NAMESIZE];

        virtual int GetVertexCount() const = 0;
        virtual const Vector3D* GetVertexBuffer() const = 0;

        virtual int GetNormalCount() const = 0;
        virtual const Vector3D* GetNormalBuffer() const = 0;

        virtual int GetTexCoordCount() const = 0;
        virtual const UV* GetTexCoordBuffer() const = 0;

        virtual int GetMeshCount() const = 0;
        virtual Mesh* GetMesh(int index) const = 0;

        virtual size_t  MeshHash() const =   0;

        virtual int GetEntityAnimationDataCount() const = 0;
        virtual const EntityAnimData* GetEntityAnimationData(int index) const = 0;
        virtual void SetEntityAnimationData(EntityAnimData entityData) = 0;
    };


    // IData: Collection of data like object meshes, materials, animations, etc.
    // used for export
    // This data is collected by Export Manager implementation
    struct IData
    {
        virtual int GetObjectCount() const = 0;
        virtual Object* GetObject(int index) const = 0;
        virtual Object* AddObject(const char* objectName) = 0;
    };
} // namespace Export



// IExporter: interface to present an exporter
// Exporter is responding to export data from object of IData type
// to file with specified format
// Exporter could be provided by user through plug-in system
struct IExporter
{
    virtual ~IExporter() = default;
    
    // Get file extension of exporter type, f.i. "obj"
    virtual const char* GetExtension() const = 0;

    // Get short format description for showing it in FileSave dialog
    // Example: "Object format"
    virtual const char* GetShortDescription() const = 0;

    // Implementation of en exporting data to the file
    virtual bool ExportToFile(const char* filename, const Export::IData* pData) = 0;
    virtual bool ImportFromFile(const char* filename, Export::IData* pData) = 0;

    // Before Export Manager is destroyed Release will be called
    virtual void Release() = 0;
};



// IExportManager: interface to export manager
struct IExportManager
{
    //! Register exporter
    //! return true if succeed, otherwise false
    virtual bool RegisterExporter(IExporter* pExporter) = 0;

    virtual bool ExportSingleStatObj(IStatObj* pStatObj, const char* filename) = 0;
};



#endif // CRYINCLUDE_EDITOR_INCLUDE_IEXPORTMANAGER_H
