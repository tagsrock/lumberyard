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

// Description : Interface for CGeomCache class


#ifndef CRYINCLUDE_CRYCOMMON_IGEOMCACHE_H
#define CRYINCLUDE_CRYCOMMON_IGEOMCACHE_H
#pragma once


#include "smartptr.h"           // TYPEDEF_AUTOPTR

// Summary:
//     Interface to hold geom cache data
struct IGeomCache
    : public IStreamable
{
    // Description:
    //     Increase the reference count of the object.
    // Summary:
    //     Notifies that the object is being used
    virtual int AddRef() = 0;

    // Description:
    //     Decrease the reference count of the object. If the reference count
    //     reaches zero, the object will be deleted from memory.
    // Summary:
    //     Notifies that the object is no longer needed
    virtual int Release() = 0;

    // Description:
    //     Checks if the geometry cache was successfully loaded from disk
    // Return Value:
    //     True if valid, otherwise false
    virtual bool IsValid() const = 0;

    // Description:
    //     Set default material for the geometry.
    // Arguments:
    //     pMaterial - A valid pointer to the material.
    virtual void SetMaterial(_smart_ptr<IMaterial> pMaterial) = 0;

    // Description:
    //     Returns default material of the geometry.
    // Arguments:
    //     nType - Pass 0 to get the physic geometry or pass 1 to get the obstruct geometry
    // Return Value:
    //     A pointer to a phys_geometry class.
    virtual _smart_ptr<IMaterial> GetMaterial() = 0;
    virtual const _smart_ptr<IMaterial> GetMaterial() const = 0;

    // Summary:
    //     Returns the filename of the object
    // Return Value:
    //     A null terminated string which contain the filename of the object.
    virtual const char* GetFilePath() const = 0;

    // Summary:
    //      Returns the duration of the geom cache animation
    // Return value:
    //      float value in seconds
    virtual float GetDuration() const = 0;

    // Summary:
    //      Reloads the cache. Need to call this when cache file changed.
    virtual void Reload() = 0;

    // Summary:
    //      Returns statistics
    // Return value:
    //  SStatistics struct
    struct SStatistics
    {
        bool m_bPlaybackFromMemory;
        float m_averageAnimationDataRate;
        uint m_numStaticMeshes;
        uint m_numStaticVertices;
        uint m_numStaticTriangles;
        uint m_numAnimatedMeshes;
        uint m_numAnimatedVertices;
        uint m_numAnimatedTriangles;
        uint m_numMaterials;
        uint m_staticDataSize;
        uint m_diskAnimationDataSize;
        uint m_memoryAnimationDataSize;
    };

    virtual SStatistics GetStatistics() const = 0;

protected:
    virtual ~IGeomCache() {}; // should be never called, use Release() instead
};


#endif // CRYINCLUDE_CRYCOMMON_IGEOMCACHE_H
