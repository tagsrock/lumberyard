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

#include "StdAfx.h"
#include "MaterialManager.h"

#include "Material.h"
#include "MaterialLibrary.h"
#include "ErrorReport.h"

#include "Viewport.h"
#include "ModelViewport.h"
#include "MaterialSender.h"

#include "ICryAnimation.h"
#include "ISourceControl.h"

#include "Util/BoostPythonHelpers.h"
#include "ShaderEnum.h"

#include "Terrain/Layer.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/SurfaceType.h"

#include "Editor/Core/QtEditorApplication.h"
#include <QtUtilWin.h>
#include <QFileInfo>
#include <QMessageBox>
#include "MainWindow.h"

#include "MaterialUtils.h"
#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzToolsFramework/AssetBrowser/EBusFindAssetTypeByName.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

static const char* MATERIALS_LIBS_PATH = "Materials/";
static unsigned int s_highlightUpdateCounter = 0;

// Convert a material name into a material identifier (no extension, no gamename, etc) so that it can be compared
// in the hash.
static QString UnifyMaterialName(const QString& source)
{
    char tempBuffer[AZ_MAX_PATH_LEN];
    azstrcpy(tempBuffer, AZ_MAX_PATH_LEN, source.toLatin1().data());
    MaterialUtils::UnifyMaterialName(tempBuffer);
    return QString(tempBuffer);
}

struct SHighlightMode
{
    float m_colorHue;
    float m_period;
    bool m_continuous;
};

static SHighlightMode g_highlightModes[] = {
    { 0.70f, 0.8f, true }, // purple
    { 0.25f, 0.75f, false }, // green
    { 0.0, 0.75f, true } // red
};

class CMaterialHighlighter
{
public:
    void Start(CMaterial* pMaterial, int modeFlag);
    void Stop(CMaterial* pMaterial, int modeFlag);
    void GetHighlightColor(ColorF* color, float* intensity, int flags);

    void ClearMaterials() { m_materials.clear(); };
    void RestoreMaterials();
    void Update();
private:
    struct SHighlightOptions
    {
        int m_modeFlags;
    };

    typedef std::map<CMaterial*, SHighlightOptions> Materials;
    Materials m_materials;
};


void CMaterialHighlighter::Start(CMaterial* pMaterial, int modeFlag)
{
    Materials::iterator it = m_materials.find(pMaterial);
    if (it == m_materials.end())
    {
        SHighlightOptions& options = m_materials[pMaterial];
        options.m_modeFlags = modeFlag;
    }
    else
    {
        SHighlightOptions& options = it->second;
        options.m_modeFlags |= modeFlag;
    }
}

void CMaterialHighlighter::Stop(CMaterial* pMaterial, int modeFlag)
{
    if (pMaterial)
    {
        pMaterial->SetHighlightFlags(0);
    }

    Materials::iterator it = m_materials.find(pMaterial);
    if (it == m_materials.end())
    {
        return;
    }

    SHighlightOptions& options = it->second;
    MAKE_SURE((options.m_modeFlags & modeFlag) != 0, return );

    options.m_modeFlags &= ~modeFlag;
    if (options.m_modeFlags == 0)
    {
        m_materials.erase(it);
    }
}

void CMaterialHighlighter::RestoreMaterials()
{
    for (Materials::iterator it = m_materials.begin(); it != m_materials.end(); ++it)
    {
        if (it->first)
        {
            it->first->SetHighlightFlags(0);
        }
    }
}

void CMaterialHighlighter::Update()
{
    unsigned int counter = s_highlightUpdateCounter;

    Materials::iterator it;
    for (it = m_materials.begin(); it != m_materials.end(); ++it)
    {
        // Only update each material every 4 frames
        if (counter++ % 4 == 0)
        {
            it->first->SetHighlightFlags(it->second.m_modeFlags);
        }
    }

    s_highlightUpdateCounter = (s_highlightUpdateCounter + 1) % 4;
}

void CMaterialHighlighter::GetHighlightColor(ColorF* color, float* intensity, int flags)
{
    MAKE_SURE(color != 0, return );
    MAKE_SURE(intensity != 0, return );

    *intensity = 0.0f;

    if (flags == 0)
    {
        return;
    }

    int flagIndex = 0;
    while (flags)
    {
        if ((flags & 1) != 0)
        {
            break;
        }
        flags = flags >> 1;
        ++flagIndex;
    }

    MAKE_SURE(flagIndex < sizeof(g_highlightModes) / sizeof(g_highlightModes[0]), return );

    const SHighlightMode& mode = g_highlightModes[flagIndex];
    float t = GetTickCount() / 1000.0f;
    float h = mode.m_colorHue;
    float s = 1.0f;
    float v = 1.0f;

    color->fromHSV(h + sinf(t * g_PI2 * 5.0f) * 0.025f, s, v);
    color->a = 1.0f;

    if (mode.m_continuous)
    {
        *intensity = fabsf(sinf(t * g_PI2 / mode.m_period));
    }
    else
    {
        *intensity = max(0.0f, sinf(t * g_PI2 / mode.m_period));
    }
}

boost::python::list PyGetMaterials(QString materialName = "", bool selectedOnly = false)
{
    boost::python::list result;

    GetIEditor()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, NULL);
    CMaterialManager* pMaterialMgr = GetIEditor()->GetMaterialManager();

    if (!materialName.isEmpty())
    {
        result.append(PyScript::CreatePyGameMaterial((CMaterial*)pMaterialMgr->FindItemByName(materialName)));
    }
    else if (selectedOnly)
    {
        if (materialName.isEmpty() && pMaterialMgr->GetSelectedItem() != NULL)
        {
            result.append(PyScript::CreatePyGameMaterial((CMaterial*)pMaterialMgr->GetSelectedItem()));
        }
    }
    else
    {
        // Acquire all of the materials via iterating across the objects.
        CBaseObjectsArray objects;
        GetIEditor()->GetObjectManager()->GetObjects(objects);
        for (int i = 0; i < objects.size(); i++)
        {
            result.append(PyScript::CreatePyGameMaterial(objects[i]->GetMaterial()));
        }
    }
    return result;
}

BOOST_PYTHON_FUNCTION_OVERLOADS(pyGetMaterialsOverload, PyGetMaterials, 0, 2);
REGISTER_PYTHON_OVERLOAD_COMMAND(PyGetMaterials, general, get_materials, pyGetMaterialsOverload,
    "Get all, subgroup, or selected materials in the material editor.",
    "general.get_materials(str materialName=\'\', selectedOnly=False, levelOnly=False)");

//////////////////////////////////////////////////////////////////////////
// CMaterialManager implementation.
//////////////////////////////////////////////////////////////////////////
CMaterialManager::CMaterialManager(CRegistrationContext& regCtx)
    : m_pHighlighter(new CMaterialHighlighter)
    , m_highlightMask(eHighlight_All)
    , m_currentFolder("")
{
    m_bUniqGuidMap = false;
    m_bUniqNameMap = true;

    m_bMaterialsLoaded = false;
    m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level", true);

    m_MatSender = new CMaterialSender(true);

    EBusFindAssetTypeByName result("Material"); //from MaterialAssetTypeInfo.cpp, case insensitive
    AZ::AssetTypeInfoBus::BroadcastResult(result, &AZ::AssetTypeInfo::GetAssetType);
    m_materialAssetType = result.GetAssetType();

    RegisterCommands(regCtx);
}

//////////////////////////////////////////////////////////////////////////
CMaterialManager::~CMaterialManager()
{
    delete m_pHighlighter;
    m_pHighlighter = 0;

    if (gEnv->p3DEngine)
    {
        gEnv->p3DEngine->GetMaterialManager()->SetListener(NULL);
    }

    if (m_MatSender)
    {
        delete m_MatSender;
        m_MatSender = 0;
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Set3DEngine()
{
    if (gEnv->p3DEngine)
    {
        gEnv->p3DEngine->GetMaterialManager()->SetListener(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ClearAll()
{
    SetCurrentMaterial(NULL);
    CBaseLibraryManager::ClearAll();

    m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level", true);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::CreateMaterial(const QString& sMaterialName,const XmlNodeRef& node, int nMtlFlags, unsigned long nLoadingFlags)
{
    CMaterial* pMaterial = new CMaterial(sMaterialName, nMtlFlags);

    if (node)
    {
        CBaseLibraryItem::SerializeContext serCtx(node, true);
        serCtx.bUniqName = true;
        pMaterial->Serialize(serCtx);
    }
    if (!pMaterial->IsPureChild() && !(pMaterial->GetFlags() & MTL_FLAG_UIMATERIAL))
    {
        RegisterItem(pMaterial);
    }

    return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::CreateMaterial(const char* sMaterialName,const XmlNodeRef& node, int nMtlFlags, unsigned long nLoadingFlags)
{
    return CreateMaterial(QString(sMaterialName), node, nMtlFlags, nLoadingFlags);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Export(XmlNodeRef& node)
{
    XmlNodeRef libs = node->newChild("MaterialsLibrary");
    for (int i = 0; i < GetLibraryCount(); i++)
    {
        IDataBaseLibrary* pLib = GetLibrary(i);
        // Level libraries are saved in in level.
        XmlNodeRef libNode = libs->newChild("Library");

        // Export library.
        libNode->setAttr("Name", pLib->GetName().toLatin1().data());
    }
}

//////////////////////////////////////////////////////////////////////////
int CMaterialManager::ExportLib(CMaterialLibrary* pLib, XmlNodeRef& libNode)
{
    int num = 0;
    // Export library.
    libNode->setAttr("Name", pLib->GetName().toLatin1().data());
    libNode->setAttr("File", pLib->GetFilename().toLatin1().data());
    char version[50];
    GetIEditor()->GetFileVersion().ToString(version, AZ_ARRAY_SIZE(version));
    libNode->setAttr("SandboxVersion", version);

    // Serialize prototypes.
    for (int j = 0; j < pLib->GetItemCount(); j++)
    {
        CMaterial* pMtl = (CMaterial*)pLib->GetItem(j);

        // Only export real used materials.
        if (pMtl->IsDummy() || !pMtl->IsUsed() || pMtl->IsPureChild())
        {
            continue;
        }

        XmlNodeRef itemNode = libNode->newChild("Material");
        itemNode->setAttr("Name", pMtl->GetName().toLatin1().data());
        num++;
    }
    return num;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetSelectedItem(IDataBaseItem* pItem)
{
    m_pSelectedItem = (CBaseLibraryItem*)pItem;
    SetCurrentMaterial((CMaterial*)pItem);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetCurrentMaterial(CMaterial* pMtl)
{
    if (m_pCurrentMaterial)
    {
        // Changing current material. save old one.
        if (m_pCurrentMaterial->IsModified())
        {
            m_pCurrentMaterial->Save();
        }
    }

    m_pCurrentMaterial = pMtl;
    if (m_pCurrentMaterial)
    {
        m_pCurrentMaterial->OnMakeCurrent();
        m_pCurrentEngineMaterial = m_pCurrentMaterial->GetMatInfo();
    }
    else
    {
        m_pCurrentEngineMaterial = 0;
    }

    m_pSelectedItem = pMtl;
    m_pSelectedParent = pMtl ? pMtl->GetParent() : NULL;

    NotifyItemEvent(m_pCurrentMaterial, EDB_ITEM_EVENT_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetCurrentFolder(const QString& folder)
{
    m_currentFolder = folder;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetMarkedMaterials(const std::vector<_smart_ptr<CMaterial> >& markedMaterials)
{
    m_markedMaterials = markedMaterials;
}

void CMaterialManager::OnLoadShader(CMaterial* pMaterial)
{
    RemoveFromHighlighting(pMaterial, eHighlight_All);
    AddForHighlighting(pMaterial);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::GetCurrentMaterial() const
{
    return m_pCurrentMaterial;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CMaterialManager::MakeNewItem()
{
    CMaterial* pMaterial = new CMaterial("", 0);
    return pMaterial;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CMaterialManager::MakeNewLibrary()
{
    return new CMaterialLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
QString CMaterialManager::GetRootNodeName()
{
    return "MaterialsLibs";
}
//////////////////////////////////////////////////////////////////////////
QString CMaterialManager::GetLibsPath()
{
    if (m_libsPath.isEmpty())
    {
        m_libsPath = MATERIALS_LIBS_PATH;
    }
    return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ReportDuplicateItem(CBaseLibraryItem* pItem, CBaseLibraryItem* pOldItem)
{
    QString sLibName;
    if (pOldItem->GetLibrary())
    {
        sLibName = pOldItem->GetLibrary()->GetName();
    }
    CErrorRecord err;
    err.pItem = (CMaterial*)pOldItem;
    err.error = QObject::tr("Material %1 with the duplicate name to the loaded material %2 ignored").arg(pItem->GetName(), pOldItem->GetName());
    GetIEditor()->GetErrorReport()->ReportError(err);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Serialize(XmlNodeRef& node, bool bLoading)
{
    //CBaseLibraryManager::Serialize( node,bLoading );
    if (bLoading)
    {
    }
    else
    {
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
    CBaseLibraryManager::OnEditorNotifyEvent(event);
    switch (event)
    {
    case eNotify_OnInit:
        InitMatSender();
        break;
    case eNotify_OnIdleUpdate:
        m_pHighlighter->Update();
        break;
    case eNotify_OnBeginGameMode:
        m_pHighlighter->RestoreMaterials();
        break;
    case eNotify_OnBeginNewScene:
        SetCurrentMaterial(0);
        break;
    case eNotify_OnBeginSceneOpen:
        SetCurrentMaterial(0);
        break;
    case eNotify_OnMissionChange:
        SetCurrentMaterial(0);
        break;
    case eNotify_OnCloseScene:
        SetCurrentMaterial(0);
        m_pHighlighter->ClearMaterials();
        break;
    case eNotify_OnQuit:
        SetCurrentMaterial(0);
        if (gEnv->p3DEngine)
        {
            gEnv->p3DEngine->GetMaterialManager()->SetListener(NULL);
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::LoadMaterial(const QString& sMaterialName, bool bMakeIfNotFound)
{
    LOADING_TIME_PROFILE_SECTION(GetISystem());

    QString sMaterialNameClear = UnifyMaterialName(sMaterialName);
    QString fullSourcePath = MaterialToFilename(sMaterialNameClear);
    QString relativePath = PathUtil::ReplaceExtension(sMaterialNameClear.toUtf8().data(), MATERIAL_FILE_EXT);

    return LoadMaterialInternal(sMaterialNameClear, fullSourcePath, relativePath, bMakeIfNotFound);
}

CMaterial* CMaterialManager::LoadMaterialWithFullSourcePath(const QString& relativeFilePath, const QString& fullSourcePath, bool makeIfNotFound /*= true*/)
{
    QString materialNameClear = UnifyMaterialName(relativeFilePath);
    return LoadMaterialInternal(materialNameClear, fullSourcePath, relativeFilePath, makeIfNotFound);
}

CMaterial* CMaterialManager::LoadMaterialInternal(const QString &materialNameClear, const QString &fullSourcePath, const QString &relativeFilePath, bool makeIfNotFound)
{    
    AzFramework::AssetSystemRequestBus::Broadcast(&AzFramework::AssetSystem::AssetSystemRequests::CompileAssetSync, AZStd::string(relativeFilePath.toUtf8().data()));

    // Load material with this name if not yet loaded.
    CMaterial* pMaterial = (CMaterial*)FindItemByName(materialNameClear);
    if (pMaterial)
    {
        // If this is a dummy material that was created before for not found mtl file,
        // try reload the mtl file again to get valid material data.
        if (pMaterial->IsDummy())
        {
            XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(fullSourcePath.toUtf8().data());
            if (mtlNode)
            {
                DeleteMaterial(pMaterial);
                pMaterial = CreateMaterial(materialNameClear, mtlNode);
            }
        }
        return pMaterial;
    }


    XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(fullSourcePath.toUtf8().data());
    if (!mtlNode)
    {
        // try again with the product file in case its present
        mtlNode = GetISystem()->LoadXmlFromFile(relativeFilePath.toUtf8().data());
    }

    if (mtlNode)
    {
        pMaterial = CreateMaterial(materialNameClear, mtlNode);
    }
    else
    {
        if (makeIfNotFound)
        {
            pMaterial = new CMaterial(materialNameClear);
            pMaterial->SetDummy(true);
            RegisterItem(pMaterial);

            CErrorRecord err;
            err.error = QObject::tr("Material %1 not found").arg(materialNameClear);
            GetIEditor()->GetErrorReport()->ReportError(err);
        }
    }
    //

    return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::LoadMaterial(const char* sMaterialName, bool bMakeIfNotFound)
{
    return LoadMaterial(QString(sMaterialName), bMakeIfNotFound);
}

//////////////////////////////////////////////////////////////////////////
static bool MaterialRequiresSurfaceType(CMaterial* pMaterial)
{
    // Do not enforce Surface Type...

    // ...over editor UI materials
    if ((pMaterial->GetFlags() & MTL_FLAG_UIMATERIAL) != 0)
    {
        return false;
    }

    // ...over SKY
    if (pMaterial->GetShaderName() == "DistanceCloud" ||
        pMaterial->GetShaderName() == "Sky" ||
        pMaterial->GetShaderName() == "SkyHDR")
    {
        return false;
    }
    // ...over terrain materials
    if (pMaterial->GetShaderName() == "Terrain.Layer")
    {
        return false;
    }
    // ...over vegetation
    if (pMaterial->GetShaderName() == "Vegetation")
    {
        return false;
    }

    // ...over decals
    bool requiresSurfaceType = true;
    CVarBlock* pShaderGenParams = pMaterial->GetShaderGenParamsVars();
    if (pShaderGenParams)
    {
        if (IVariable* pVar = pShaderGenParams->FindVariable("Decal"))
        {
            int value = 0;
            pVar->Get(value);
            if (value)
            {
                requiresSurfaceType = false;
            }
        }
        // The function GetShaderGenParamsVars allocates a new CVarBlock object, so let's clean it up here
        delete pShaderGenParams;
    }
    return requiresSurfaceType;
}

//////////////////////////////////////////////////////////////////////////
int CMaterialManager::GetHighlightFlags(CMaterial* pMaterial) const
{
    if (pMaterial == NULL)
    {
        return 0;
    }

    if ((pMaterial->GetFlags() & MTL_FLAG_NODRAW) != 0)
    {
        return 0;
    }

    int result = 0;

    if (pMaterial == m_pHighlightMaterial)
    {
        result |= eHighlight_Pick;
    }

    const QString& surfaceTypeName = pMaterial->GetSurfaceTypeName();
    if (surfaceTypeName.isEmpty() && MaterialRequiresSurfaceType(pMaterial))
    {
        result |= eHighlight_NoSurfaceType;
    }

    if (ISurfaceTypeManager* pSurfaceManager = GetIEditor()->Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager())
    {
        const ISurfaceType* pSurfaceType =  pSurfaceManager->GetSurfaceTypeByName(surfaceTypeName.toLatin1().data());
        if (pSurfaceType && pSurfaceType->GetBreakability() != 0)
        {
            result |= eHighlight_Breakable;
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::AddForHighlighting(CMaterial* pMaterial)
{
    if (pMaterial == NULL)
    {
        return;
    }

    int highlightFlags = (GetHighlightFlags(pMaterial) & m_highlightMask);
    if (highlightFlags != 0)
    {
        m_pHighlighter->Start(pMaterial, highlightFlags);
    }

    int count = pMaterial->GetSubMaterialCount();
    for (int i = 0; i < count; ++i)
    {
        CMaterial* pChild = pMaterial->GetSubMaterial(i);
        if (!pChild)
        {
            continue;
        }

        AddForHighlighting(pChild);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::RemoveFromHighlighting(CMaterial* pMaterial, int mask)
{
    if (pMaterial == NULL)
    {
        return;
    }

    m_pHighlighter->Stop(pMaterial, mask);

    int count = pMaterial->GetSubMaterialCount();
    for (int i = 0; i < count; ++i)
    {
        CMaterial* pChild = pMaterial->GetSubMaterial(i);
        if (!pChild)
        {
            continue;
        }

        RemoveFromHighlighting(pChild, mask);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::UpdateHighlightedMaterials()
{
    IDataBaseItemEnumerator* pEnum = CBaseLibraryManager::GetItemEnumerator();
    if (!pEnum)
    {
        return;
    }

    CMaterial* pMaterial = (CMaterial*)pEnum->GetFirst();
    while (pMaterial)
    {
        RemoveFromHighlighting(pMaterial, eHighlight_All);
        AddForHighlighting(pMaterial);
        pMaterial =  (CMaterial*)pEnum->GetNext();
    }

    pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnRequestMaterial(_smart_ptr<IMaterial> pMatInfo)
{
    const char* pcName = pMatInfo->GetName();
    CMaterial* pMaterial = (CMaterial*) pMatInfo->GetUserData();

    if (!pMaterial && pcName && *pcName)
    {
        pMaterial = LoadMaterial(pcName, false);
    }

    if (pMaterial)
    {
        _smart_ptr<IMaterial> pNewMatInfo = pMaterial->GetMatInfo(true);
        assert(pNewMatInfo == pMatInfo);
        //Only register if the material is not registered
        if (!pMaterial->IsRegistered())
        {
            RegisterItem(pMaterial);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnCreateMaterial(_smart_ptr<IMaterial> pMatInfo)
{
    // Ignore if the editor material already exists
    bool materialAlreadyExists = FindItemByName(UnifyMaterialName(pMatInfo->GetName())) != nullptr;

    if (!materialAlreadyExists && !(pMatInfo->GetFlags() & MTL_FLAG_PURE_CHILD) && !(pMatInfo->GetFlags() & MTL_FLAG_UIMATERIAL))
    {
        CMaterial* pMaterial = new CMaterial(pMatInfo->GetName());
        pMaterial->SetFromMatInfo(pMatInfo);
        RegisterItem(pMaterial);

        AddForHighlighting(pMaterial);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnDeleteMaterial(_smart_ptr<IMaterial> pMaterial)
{
    CMaterial* pMtl = (CMaterial*)pMaterial->GetUserData();
    if (pMtl)
    {
        RemoveFromHighlighting(pMtl, eHighlight_All);
        DeleteMaterial(pMtl);
    }
}

bool CMaterialManager::IsCurrentMaterial(_smart_ptr<IMaterial> pMaterial) const
{
    if (!pMaterial)
    {
        return false;
    }

    CMaterial* pMtl = static_cast<CMaterial*>(pMaterial->GetUserData());
    bool currentMaterial = (pMtl == m_pCurrentMaterial);

    if (pMtl->GetParent())
    {
        currentMaterial |= (pMtl->GetParent() == m_pCurrentMaterial);
    }

    for (size_t subMatIdx = 0; subMatIdx < pMtl->GetMatInfo()->GetSubMtlCount(); ++subMatIdx)
    {
        if (static_cast<CMaterial*>(pMtl->GetMatInfo()->GetSubMtl(subMatIdx)->GetUserData()) == m_pCurrentMaterial)
        {
            currentMaterial = true;
            break;
        }
    }

    return currentMaterial;
}


//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::FromIMaterial(_smart_ptr<IMaterial> engineMaterial)
{
    if (!engineMaterial)
    {
        return nullptr;
    }
    CMaterial* editorMaterial = (CMaterial*)engineMaterial->GetUserData();
    if (!editorMaterial)
    {
        // If the user data isn't set, check for an existing material with the same name
        editorMaterial = static_cast<CMaterial*>(FindItemByName(UnifyMaterialName(engineMaterial->GetName())));
    }
    return editorMaterial;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SaveAllLibs()
{
}

//////////////////////////////////////////////////////////////////////////
QString CMaterialManager::FilenameToMaterial(const QString& filename)
{
    // Convert a full or relative path to a normalized name that can be used in a hash (so lowercase, relative path, correct slashes, remove extension)    
    // note that it may already be an asset path, if so, don't add the overhead of calling into the AP and convert it.
    // if it starts with an alias (@) or if its an absolute file path, we need to convert it.  Otherwise we really don't...
    QString name = filename;
    if ( (name.left(1) == "@") || (AzFramework::StringFunc::Path::HasDrive(name.toUtf8().data())) )
    {
        name = Path::FullPathToGamePath(filename); // convert any full path to a relative path instead.
    }
    QByteArray n = name.toUtf8();
    MaterialUtils::UnifyMaterialName(n.data());  // Utility function used by all other parts of the code to unify slashes, lowercase, and remove extension

    return QString::fromUtf8(n);
}

//////////////////////////////////////////////////////////////////////////
QString CMaterialManager::MaterialToFilename(const QString& sMaterialName)
{
    QString materialWithExtension = Path::ReplaceExtension(sMaterialName, MATERIAL_FILE_EXT);
    QString fileName = Path::GamePathToFullPath(materialWithExtension);
    if (fileName.right(4).toLower() != MATERIAL_FILE_EXT)
    {
        // we got something back which is not a mtl, fall back heuristic:
        AZStd::string pathName(fileName.toUtf8().data());
        AZStd::string fileNameOfMaterial;
        AzFramework::StringFunc::Path::StripFullName(pathName); // remove the filename of the path to the FBX file so now it just contains the folder of the fbx file.
        AzFramework::StringFunc::Path::GetFullFileName(materialWithExtension.toUtf8().data(), fileNameOfMaterial); // remove the path part of the material so it only contains the file name
        AZStd::string finalName;
        AzFramework::StringFunc::Path::Join(pathName.c_str(), fileNameOfMaterial.c_str(), finalName);
        fileName = finalName.c_str();
    }
    return fileName;
}

const AZ::Data::AssetType& CMaterialManager::GetMaterialAssetType()
{
    return m_materialAssetType;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::DeleteMaterial(CMaterial* pMtl)
{
    assert(pMtl);
    _smart_ptr<CMaterial> _ref(pMtl);
    if (pMtl == GetCurrentMaterial())
    {
        SetCurrentMaterial(NULL);
    }

    DeleteItem(pMtl);

    // Delete it from all sub materials.
    for (int i = 0; i < m_pLevelLibrary->GetItemCount(); i++)
    {
        CMaterial* pMultiMtl = (CMaterial*)m_pLevelLibrary->GetItem(i);
        if (pMultiMtl->IsMultiSubMaterial())
        {
            for (int slot = 0; slot < pMultiMtl->GetSubMaterialCount(); slot++)
            {
                if (pMultiMtl->GetSubMaterial(slot) == pMultiMtl)
                {
                    // Clear this sub material slot.
                    pMultiMtl->SetSubMaterial(slot, 0);
                }
            }
        }
    }
}

void CMaterialManager::RemoveMaterialFromDisk(const char * fileName)
{
    QFile::remove(fileName);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::RegisterCommands(CRegistrationContext& regCtx)
{
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "duplicate", "", "", functor(*this, &CMaterialManager::Command_Duplicate));
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "merge", "", "", functor(*this, &CMaterialManager::Command_Merge));
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "delete", "", "", functor(*this, &CMaterialManager::Command_Delete));
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "assign_to_selection", "", "", functor(*this, &CMaterialManager::Command_AssignToSelection));
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "select_assigned_objects", "", "", functor(*this, &CMaterialManager::Command_SelectAssignedObjects));
    CommandManagerHelper::RegisterCommand(regCtx.pCommandManager, "material", "select_from_object", "", "", functor(*this, &CMaterialManager::Command_SelectFromObject));
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialManager::SelectSaveMaterial(QString& itemName, const char* defaultStartPath)
{
    QString startPath;
    if (defaultStartPath && defaultStartPath[0] != '\0')
    {
        startPath = defaultStartPath;
    }
    else
    {
        startPath = GetIEditor()->GetSearchPath(EDITOR_PATH_MATERIALS);
    }

    QString filename;
    if (!CFileUtil::SelectSaveFile("Material Files (*.mtl)", "mtl", startPath, filename))
    {
        return false;
    }

    // KDAB Not sure why Path::GamePathToFullPath is being used here & filename should be an
    // KDAB absolute path under Qt: suspect this should be removed.
    itemName = Path::GamePathToFullPath(filename);
    itemName = FilenameToMaterial(itemName);
    if (itemName.isEmpty())
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::SelectNewMaterial(int nMtlFlags, const char* sStartPath)
{
    QString path = m_pCurrentMaterial ? Path::GetPath(m_pCurrentMaterial->GetFilename()) : m_currentFolder;
    QString itemName;
    if (!SelectSaveMaterial(itemName, path.toLatin1().data()))
    {
        return 0;
    }

    if (FindItemByName(itemName))
    {
        Warning("Material with name %s already exist", itemName.toLatin1().data());
        return 0;
    }

    _smart_ptr<CMaterial> mtl = CreateMaterial(itemName, XmlNodeRef(), nMtlFlags);
    mtl->Update();
    mtl->Save();
    SetCurrentMaterial(mtl);
    return mtl;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Create()
{
    SelectNewMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_CreateMulti()
{
    SelectNewMaterial(MTL_FLAG_MULTI_SUBMTL);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_ConvertToMulti()
{
    CMaterial* pMaterial = GetCurrentMaterial();

    if (pMaterial && pMaterial->GetSubMaterialCount() == 0)
    {
        CMaterial* pSubMat = new CMaterial(*pMaterial);
        pSubMat->SetName(pSubMat->GetShortName());
        pSubMat->SetFlags(pSubMat->GetFlags() | MTL_FLAG_PURE_CHILD);

        pMaterial->SetFlags(MTL_FLAG_MULTI_SUBMTL);
        pMaterial->SetSubMaterialCount(1);
        pMaterial->SetSubMaterial(0, pSubMat);

        pMaterial->Save();
        pMaterial->Reload();
        SetSelectedItem(pSubMat);
    }
    else
    {
        Warning(pMaterial ? "material.convert_to_multi called on invalid material setup" : "material.convert_to_multi called while no material selected");
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Duplicate()
{
    CMaterial* pSrcMtl = GetCurrentMaterial();

    if (!pSrcMtl)
    {
        CErrorRecord err;
        err.error = "material.duplicate called while no materials selected";
        GetIEditor()->GetErrorReport()->ReportError(err);
        return;
    }

    if (GetIEditor()->IsSourceControlAvailable())
    {
        uint32 attrib = pSrcMtl->GetFileAttributes();

        if ((attrib & SCC_FILE_ATTRIBUTE_INPAK) &&  (attrib & SCC_FILE_ATTRIBUTE_MANAGED) && !(attrib & SCC_FILE_ATTRIBUTE_NORMAL))
        {
            // Get latest for making folders with right case
            GetIEditor()->GetSourceControl()->GetLatestVersion(pSrcMtl->GetFilename().toLatin1().data());
        }
    }

    if (pSrcMtl != 0 && !pSrcMtl->IsPureChild())
    {
        QString name = MakeUniqueItemName(pSrcMtl->GetName());
        // Create a new material.
        _smart_ptr<CMaterial> pMtl = DuplicateMaterial(name.toLatin1().data(), pSrcMtl);
        if (pMtl)
        {
            pMtl->Save();
            SetSelectedItem(pMtl);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::DuplicateMaterial(const char* newName, CMaterial* pOriginal)
{
    if (!newName)
    {
        assert(0 && "NULL newName passed into CMaterialManager::DuplicateMaterial");
        return 0;
    }
    if (!pOriginal)
    {
        assert(0 && "NULL pOriginal passed into CMaterialManager::DuplicateMaterial");
        return 0;
    }


    XmlNodeRef node = GetISystem()->CreateXmlNode("Material");
    CBaseLibraryItem::SerializeContext ctx(node, false);
    ctx.bCopyPaste = true;
    pOriginal->Serialize(ctx);

    return CreateMaterial(newName, node, pOriginal->GetFlags());
}

void CMaterialManager::GenerateUniqueSubmaterialName(const CMaterial* pSourceMaterial, const CMaterial* pTargetMaterial, QString& uniqueSubmaterialName) const
{
    QString sourceMaterialName = pSourceMaterial->GetName();

    // We don't need the whole path to the material, just the base name
    QFileInfo filename(sourceMaterialName);
    sourceMaterialName = filename.baseName();

    uniqueSubmaterialName = sourceMaterialName;
    size_t nameIndex = 0;

    bool nameUpdated = true;
    while (nameUpdated)
    {
        nameUpdated = false;
        for (size_t k = 0; k < pTargetMaterial->GetSubMaterialCount(); ++k)
        {
            CMaterial* pSubMaterial = pTargetMaterial->GetSubMaterial(k);
            if (pSubMaterial && pSubMaterial->GetName() == uniqueSubmaterialName)
            {
                ++nameIndex;
                uniqueSubmaterialName = QStringLiteral("%1%2").arg(sourceMaterialName).arg(nameIndex, 2, 10, QLatin1Char('0'));
                nameUpdated = true;
                break;
            }
        }
    }
}

bool CMaterialManager::DuplicateAsSubMaterialAtIndex(CMaterial* pSourceMaterial, CMaterial* pTargetMaterial, int subMaterialIndex)
{
    if (pSourceMaterial && pTargetMaterial && pTargetMaterial->GetSubMaterialCount() > subMaterialIndex)
    {
        // Resolve name collisions between the source material and the submaterials in the target material
        QString newSubMaterialName;
        GenerateUniqueSubmaterialName(pSourceMaterial, pTargetMaterial, newSubMaterialName);

        // Mark the material to be duplicated as a PURE_CHILD since it is being duplicated as a submaterial
        int sourceMaterialFlags = pSourceMaterial->GetFlags();
        pSourceMaterial->SetFlags(sourceMaterialFlags | MTL_FLAG_PURE_CHILD);

        CMaterial* pNewSubMaterial = DuplicateMaterial(newSubMaterialName.toLatin1().data(), pSourceMaterial);
        pTargetMaterial->SetSubMaterial(subMaterialIndex, pNewSubMaterial);

        // Reset the flags of the source material to their original values
        pSourceMaterial->SetFlags(sourceMaterialFlags);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Merge()
{
    QString itemName;
    QString defaultMaterialPath;
    if (m_pCurrentMaterial)
    {
        defaultMaterialPath = Path::GetPath(m_pCurrentMaterial->GetFilename());
    }
    if (!SelectSaveMaterial(itemName, defaultMaterialPath.toLatin1().data()))
    {
        return;
    }

    _smart_ptr<CMaterial> pNewMaterial = CreateMaterial(itemName, XmlNodeRef(), MTL_FLAG_MULTI_SUBMTL);

    size_t totalSubMaterialCount = 0;
    for (_smart_ptr<CMaterial> pMaterial : m_markedMaterials)
    {
        if (pMaterial->IsMultiSubMaterial())
        {
            totalSubMaterialCount += pMaterial->GetSubMaterialCount();
        }
        else
        {
            totalSubMaterialCount++;
        }
    }
    pNewMaterial->SetSubMaterialCount(totalSubMaterialCount);

    size_t subMaterialIndex = 0;
    for (_smart_ptr<CMaterial> pMaterial : m_markedMaterials)
    {
        if (pMaterial->IsMultiSubMaterial())
        {
            // Loop through each submaterial and duplicate it as a submaterial in the new material
            for (size_t j = 0; j < pMaterial->GetSubMaterialCount(); ++j)
            {
                CMaterial* pSubMaterial = pMaterial->GetSubMaterial(j);
                if (DuplicateAsSubMaterialAtIndex(pSubMaterial, pNewMaterial, subMaterialIndex))
                {
                    ++subMaterialIndex;
                }
            }
        }
        else
        {
            // Duplicate the material as a submaterial in the new material
            if (DuplicateAsSubMaterialAtIndex(pMaterial, pNewMaterial, subMaterialIndex))
            {
                ++subMaterialIndex;
            }
        }
    }

    pNewMaterial->Update();
    pNewMaterial->Save();
    SetCurrentMaterial(pNewMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Delete()
{
    CMaterial* pMtl = GetCurrentMaterial();
    if (pMtl)
    {
        CUndo undo("Delete Material");
        QString str = QObject::tr("Delete Material %1?\r\nNote: Material file %2 will also be deleted.")
            .arg(pMtl->GetName(), pMtl->GetFilename());
        if (QMessageBox::question(QApplication::activeWindow(), QObject::tr("Delete Confirmation"), str) == QMessageBox::Yes)
        {
            AZStd::string matName = pMtl->GetFilename().toUtf8().data();
            DeleteMaterial(pMtl);
            RemoveMaterialFromDisk(matName.c_str());
            SetCurrentMaterial(0);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_AssignToSelection()
{
    CMaterial* pMtl = GetCurrentMaterial();
    if (pMtl)
    {
        CUndo undo("Assign Material");
        CSelectionGroup* pSel = GetIEditor()->GetSelection();
        if (pMtl->IsPureChild())
        {
            int nSelectionCount = pSel->GetCount();
            bool bAllDesignerObject = nSelectionCount == 0 ? false : true;
            for (int i = 0; i < nSelectionCount; ++i)
            {
                if (pSel->GetObject(i)->GetType() != OBJTYPE_SOLID)
                {
                    bAllDesignerObject = false;
                    break;
                }
            }
            if (!bAllDesignerObject)
            {
                if (QMessageBox::information(QApplication::activeWindow(), QObject::tr("Assign Submaterial"), QObject::tr("You can assign submaterials to objects only for preview purpose. This assignment will not be saved with the level and will not be exported to the game."), QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
                {
                    return;
                }
            }
        }
        if (!pSel->IsEmpty())
        {
            for (int i = 0; i < pSel->GetCount(); i++)
            {
                pSel->GetObject(i)->SetMaterial(pMtl);
                pSel->GetObject(i)->UpdateGroup();
                pSel->GetObject(i)->UpdatePrefab();
            }
        }
    }
    CViewport* pViewport = GetIEditor()->GetActiveView();
    if (pViewport)
    {
        pViewport->Drop(QPoint(-1, -1), pMtl);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_ResetSelection()
{
    CSelectionGroup* pSel = GetIEditor()->GetSelection();
    if (!pSel->IsEmpty())
    {
        CUndo undo("Reset Material");
        for (int i = 0; i < pSel->GetCount(); i++)
        {
            pSel->GetObject(i)->SetMaterial(0);
        }
    }
    CViewport* pViewport = GetIEditor()->GetActiveView();
    if (pViewport)
    {
        pViewport->Drop(QPoint(-1, -1), 0);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectAssignedObjects()
{
    CMaterial* pMtl = GetCurrentMaterial();
    if (pMtl)
    {
        CUndo undo("Select Object(s)");
        CBaseObjectsArray objects;
        GetIEditor()->GetObjectManager()->GetObjects(objects);
        for (int i = 0; i < objects.size(); i++)
        {
            CBaseObject* pObject = objects[i];
            if (pObject->GetMaterial() == pMtl || pObject->GetRenderMaterial() == pMtl)
            {
                if (pObject->IsHidden() || pObject->IsFrozen())
                {
                    continue;
                }
                GetIEditor()->GetObjectManager()->SelectObject(pObject);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectFromObject()
{
    if (GetIEditor()->IsInPreviewMode())
    {
        CViewport* pViewport = GetIEditor()->GetActiveView();
        if (CModelViewport* p = viewport_cast<CModelViewport*>(pViewport))
        {
            CMaterial* pMtl = p->GetMaterial();
            SetCurrentMaterial(pMtl);
        }
        return;
    }

    CSelectionGroup* pSel = GetIEditor()->GetSelection();
    if (pSel->IsEmpty())
    {
        return;
    }

    for (int i = 0; i < pSel->GetCount(); i++)
    {
        CMaterial* pMtl = pSel->GetObject(i)->GetRenderMaterial();
        if (pMtl)
        {
            SetCurrentMaterial(pMtl);
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::PickPreviewMaterial()
{
    XmlNodeRef data = XmlHelpers::CreateXmlNode("ExportMaterial");
    CMaterial* pMtl = GetCurrentMaterial();
    if (!pMtl)
    {
        return;
    }

    if (pMtl->IsPureChild() && pMtl->GetParent())
    {
        pMtl = pMtl->GetParent();
    }

    if (pMtl->GetFlags() & MTL_FLAG_WIRE)
    {
        data->setAttr("Flag_Wire", 1);
    }
    if (pMtl->GetFlags() & MTL_FLAG_2SIDED)
    {
        data->setAttr("Flag_2Sided", 1);
    }

    data->setAttr("Name", pMtl->GetName().toLatin1().data());
    data->setAttr("FileName", pMtl->GetFilename().toLatin1().data());

    XmlNodeRef node = data->newChild("Material");

    CBaseLibraryItem::SerializeContext serCtx(node, false);
    pMtl->Serialize(serCtx);


    if (!pMtl->IsMultiSubMaterial())
    {
        XmlNodeRef texturesNode = node->findChild("Textures");
        if (texturesNode)
        {
            for (int i = 0; i < texturesNode->getChildCount(); i++)
            {
                XmlNodeRef texNode = texturesNode->getChild(i);
                QString file;
                if (texNode->getAttr("File", file))
                {
                    texNode->setAttr("File", Path::GamePathToFullPath(file).toLatin1().data());
                }
            }
        }
    }
    else
    {
        XmlNodeRef childsNode = node->findChild("SubMaterials");
        if (childsNode)
        {
            int nSubMtls = childsNode->getChildCount();
            for (int i = 0; i < nSubMtls; i++)
            {
                XmlNodeRef node = childsNode->getChild(i);
                XmlNodeRef texturesNode = node->findChild("Textures");
                if (texturesNode)
                {
                    for (int i = 0; i < texturesNode->getChildCount(); i++)
                    {
                        XmlNodeRef texNode = texturesNode->getChild(i);
                        QString file;
                        if (texNode->getAttr("File", file))
                        {
                            texNode->setAttr("File", Path::GamePathToFullPath(file).toLatin1().data());
                        }
                    }
                }
            }
        }
    }


    m_MatSender->SendMessage(eMSM_GetSelectedMaterial, data);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SyncMaterialEditor()
{
    if (!m_MatSender)
    {
        return;
    }

    if (!m_MatSender->GetMessage())
    {
        return;
    }

    if (m_MatSender->m_h.msg == eMSM_Create)
    {
        XmlNodeRef node = m_MatSender->m_node->findChild("Material");
        if (!node)
        {
            return;
        }

        QString sMtlName;
        QString sMaxFile;

        XmlNodeRef root = m_MatSender->m_node;
        root->getAttr("Name", sMtlName);
        root->getAttr("MaxFile", sMaxFile);

        int IsMulti = 0;
        root->getAttr("IsMulti", IsMulti);

        int nMtlFlags = 0;
        if (IsMulti)
        {
            nMtlFlags |= MTL_FLAG_MULTI_SUBMTL;
        }

        if (root->haveAttr("Flag_Wire"))
        {
            nMtlFlags |= MTL_FLAG_WIRE;
        }
        if (root->haveAttr("Flag_2Sided"))
        {
            nMtlFlags |= MTL_FLAG_2SIDED;
        }

        _smart_ptr<CMaterial> pMtl = SelectNewMaterial(nMtlFlags, Path::GetPath(sMaxFile).toLatin1().data());

        if (!pMtl)
        {
            return;
        }

        if (!IsMulti)
        {
            node->delAttr("Shader");   // Remove shader attribute.
            XmlNodeRef texturesNode = node->findChild("Textures");
            if (texturesNode)
            {
                for (int i = 0; i < texturesNode->getChildCount(); i++)
                {
                    XmlNodeRef texNode = texturesNode->getChild(i);
                    QString file;
                    if (texNode->getAttr("File", file))
                    {
                        //make path relative to the project specific game folder
                        QString newfile = Path::MakeGamePath(file);
                        if (!newfile.isEmpty())
                        {
                            file = newfile;
                        }
                        texNode->setAttr("File", file.toLatin1().data());
                    }
                }
            }
        }
        else
        {
            XmlNodeRef childsNode = node->findChild("SubMaterials");
            if (childsNode)
            {
                int nSubMtls = childsNode->getChildCount();
                for (int i = 0; i < nSubMtls; i++)
                {
                    XmlNodeRef node = childsNode->getChild(i);
                    node->delAttr("Shader");   // Remove shader attribute.
                    XmlNodeRef texturesNode = node->findChild("Textures");
                    if (texturesNode)
                    {
                        for (int i = 0; i < texturesNode->getChildCount(); i++)
                        {
                            XmlNodeRef texNode = texturesNode->getChild(i);
                            QString file;
                            if (texNode->getAttr("File", file))
                            {
                                //make path relative to the project specific game folder
                                QString newfile = Path::MakeGamePath(file);
                                if (!newfile.isEmpty())
                                {
                                    file = newfile;
                                }
                                texNode->setAttr("File", file.toLatin1().data());
                            }
                        }
                    }
                }
            }
        }

        CBaseLibraryItem::SerializeContext ctx(node, true);
        ctx.bUndo = true;
        pMtl->Serialize(ctx);

        pMtl->Update();

        SetCurrentMaterial(0);
        SetCurrentMaterial(pMtl);
    }

    if (m_MatSender->m_h.msg == eMSM_GetSelectedMaterial)
    {
        PickPreviewMaterial();
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::InitMatSender()
{
    //MatSend(true);
    m_MatSender->Create();
    QWidget* mainWindow = MainWindow::instance();
    m_MatSender->SetupWindows(mainWindow, mainWindow);
    XmlNodeRef node = XmlHelpers::CreateXmlNode("Temp");
    m_MatSender->SendMessage(eMSM_Init, node);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial(CMaterial* pMaterial)
{
    if (pMaterial)
    {
        GetIEditor()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pMaterial);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial(_smart_ptr<IMaterial> pMtl)
{
    if (pMtl)
    {
        CMaterial* pEdMaterial = FromIMaterial(pMtl);
        if (pEdMaterial)
        {
            GetIEditor()->OpenDataBaseLibrary(EDB_TYPE_MATERIAL, pEdMaterial);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetHighlightedMaterial(CMaterial* pMtl)
{
    if (m_pHighlightMaterial)
    {
        RemoveFromHighlighting(m_pHighlightMaterial, eHighlight_Pick);
    }

    m_pHighlightMaterial = pMtl;
    if (m_pHighlightMaterial)
    {
        AddForHighlighting(m_pHighlightMaterial);
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::HighlightedMaterialChanged(CMaterial* pMtl)
{
    if (!pMtl)
    {
        return;
    }

    RemoveFromHighlighting(pMtl, eHighlight_All);
    AddForHighlighting(pMtl);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetHighlightMask(int highlightMask)
{
    if (m_highlightMask != highlightMask)
    {
        m_highlightMask = highlightMask;

        UpdateHighlightedMaterials();
    }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GatherResources(_smart_ptr<IMaterial> pMaterial, CUsedResources& resources)
{
    if (!pMaterial)
    {
        return;
    }

    int nSubMtlCount = pMaterial->GetSubMtlCount();
    if (nSubMtlCount > 0)
    {
        for (int i = 0; i < nSubMtlCount; i++)
        {
            GatherResources(pMaterial->GetSubMtl(i), resources);
        }
    }
    else
    {
        SShaderItem& shItem = pMaterial->GetShaderItem();
        if (shItem.m_pShaderResources)
        {
            SInputShaderResources res;
            shItem.m_pShaderResources->ConvertToInputResource(&res);

            for (int i = 0; i < EFTT_MAX; i++)
            {
                if (!res.m_Textures[i].m_Name.empty())
                {
                    resources.Add(res.m_Textures[i].m_Name.c_str());
                }
            }

            gEnv->pRenderer->EF_ReleaseInputShaderResource(&res);
        }
    }
}

///////////////////////////////////////////////////////////////////////////
void CMaterialManager::GetHighlightColor(ColorF* color, float* intensity, int flags)
{
    MAKE_SURE(m_pHighlighter, return );
    m_pHighlighter->GetHighlightColor(color, intensity, flags);
}

