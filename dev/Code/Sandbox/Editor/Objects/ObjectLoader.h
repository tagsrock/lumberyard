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

#ifndef CRYINCLUDE_EDITOR_OBJECTS_OBJECTLOADER_H
#define CRYINCLUDE_EDITOR_OBJECTS_OBJECTLOADER_H
#pragma once


#include "Util/GuidUtil.h"
#include "ErrorReport.h"

class CPakFile;

typedef std::map<GUID, GUID, guid_less_predicate> TGUIDRemap;

/** CObjectLoader used to load Bas Object and resolve ObjectId references while loading.
*/
class SANDBOX_API CObjectArchive
{
public:
    XmlNodeRef node; //!< Current archive node.
    bool bLoading;
    bool bUndo;

    CObjectArchive(IObjectManager* objMan, XmlNodeRef xmlRoot, bool loading);
    ~CObjectArchive();

    //! Resolve callback with only one parameter of CBaseObject.
    typedef Functor1<CBaseObject*> ResolveObjRefFunctor1;
    //! Resolve callback with two parameters one is pointer to CBaseObject and second use data integer.
    typedef Functor2<CBaseObject*, unsigned int> ResolveObjRefFunctor2;

    /** Register Object id.
        @param objectId Original object id from the file.
        @param realObjectId Changed object id.
    */
    //void RegisterObjectId( int objectId,int realObjectId );

    // Return object ID remapped after loading.
    GUID ResolveID(REFGUID id);

    //! Set object resolve callback, it will be called once object with specified Id is loaded.
    void SetResolveCallback(CBaseObject* fromObject, REFGUID objectId, ResolveObjRefFunctor1 func);
    //! Set object resolve callback, it will be called once object with specified Id is loaded.
    void SetResolveCallback(CBaseObject* fromObject, REFGUID objectId, ResolveObjRefFunctor2 func, uint32 userData);
    //! Resolve all object ids and call callbacks on resolved objects.
    void ResolveObjects();

    // Save object to archive.
    void SaveObject(CBaseObject* pObject, bool bSaveInGroupObjects = false, bool bSaveInPrefabObjects = false);

    //! Load multiple objects from archive.
    void LoadObjects(XmlNodeRef& rootObjectsNode);

    //! Load one object from archive.
    CBaseObject* LoadObject(const XmlNodeRef& objNode, CBaseObject* pPrevObject = NULL);

    //////////////////////////////////////////////////////////////////////////
    int GetLoadedObjectsCount() { return m_loadedObjects.size(); }
    CBaseObject* GetLoadedObject(int nIndex) const { return m_loadedObjects[nIndex].pObject; }

    //! If true new loaded objects will be assigned new GUIDs.
    void MakeNewIds(bool bEnable);

    void EnableReconstructPrefabObject(bool bEnable);

    //! Remap object ids.
    void RemapID(REFGUID oldId, REFGUID newId);

    //! Report error during loading.
    void ReportError(CErrorRecord& err);
    //! Assigner different error report class.
    void SetErrorReport(CErrorReport* errReport);
    //! Display collected error reports.
    void ShowErrors();

    void EnableProgressBar(bool bEnable) { m_bProgressBarEnabled = bEnable; };

    CPakFile* GetGeometryPak(const char* sFilename);
    CBaseObject* GetCurrentObject();

    void AddSequenceIdMapping(uint32 oldId, uint32 newId);
    uint32 RemapSequenceId(uint32 id) const;
    bool IsAmongPendingIds(uint32 id) const;

    bool IsReconstructingPrefab() const { return m_nFlags & eObjectLoader_ReconstructPrefabs; }
    bool IsSavingInPrefab() const { return m_nFlags & eObjectLoader_SavingInPrefabLib; }
    void SetShouldResetInternalMembers(bool reset);
    bool ShouldResetInternalMembers() const { return m_nFlags & eObjectLoader_ResetInternalMembers; }

private:
    struct SLoadedObjectInfo
    {
        int nSortOrder;
        _smart_ptr<CBaseObject> pObject;
        XmlNodeRef xmlNode;
        GUID newGuid;
        bool operator <(const SLoadedObjectInfo& oi) const { return nSortOrder < oi.nSortOrder;   }
    };


    IObjectManager* m_objectManager;
    struct Callback
    {
        ResolveObjRefFunctor1 func1;
        ResolveObjRefFunctor2 func2;
        uint32 userData;
        _smart_ptr<CBaseObject> fromObject;
        Callback() { func1 = 0; func2 = 0; userData = 0; };
    };
    typedef std::multimap<GUID, Callback, guid_less_predicate> Callbacks;
    Callbacks m_resolveCallbacks;

    // Set of all saved objects to this archive.
    typedef std::set<_smart_ptr<CBaseObject> > ObjectsSet;
    ObjectsSet m_savedObjects;

    //typedef std::multimap<int,_smart_ptr<CBaseObject> > OrderedObjects;
    //OrderedObjects m_orderedObjects;
    std::vector<SLoadedObjectInfo> m_loadedObjects;

    // Loaded objects IDs, used for remapping of GUIDs.
    TGUIDRemap m_IdRemap;

    enum EObjectLoaderFlags
    {
        eObjectLoader_MakeNewIDs = 0x0001,      // If true new loaded objects will be assigned new GUIDs.
        eObjectLoader_ReconstructPrefabs = 0x0002,  // If the loading is for reconstruction from prefab.
        eObjectLoader_ResetInternalMembers = 0x0004, // In case we are deserializing and we would like to wipe all previous state
        eObjectLoader_SavingInPrefabLib = 0x0008 // We are serializing in the prefab library (used to omit some not needed attributes of objects)
    };
    int m_nFlags;
    IErrorReport* m_pCurrentErrorReport;
    CPakFile* m_pGeometryPak;
    CBaseObject* m_pCurrentObject;

    bool m_bNeedResolveObjects;
    bool m_bProgressBarEnabled;

    // This table is used when there is any collision of ids while importing TrackView sequences.
    std::map<uint32, uint32> m_sequenceIdRemap;
    std::vector<uint32> m_pendingIds;
};

#endif // CRYINCLUDE_EDITOR_OBJECTS_OBJECTLOADER_H
