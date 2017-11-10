#include "stdafx.h"

#include "ReflectedVarWrapper.h"
#include "ReflectedPropertyCtrl.h"
#include "UIEnumsDatabase.h"
#include "Objects/ShapeObject.h"
#include "Objects/AIWave.h"

namespace {

    //setting the IVariable to itself in property items was trigger to update limits for that variable. 
    //limits were obtained using IVariable::GetLimits instead of from the Prop::Description
    template <class T, class R>
    void setRangeParams(CReflectedVarRanged<T, R> *reflectedVar, IVariable *pVariable, bool updatingExistingVariable = false)
    {
        float min, max, step;
        bool  hardMin, hardMax;
        if (updatingExistingVariable)
        {
            pVariable->GetLimits(min, max, step, hardMin, hardMax);
        }
        else
        {
            Prop::Description desc(pVariable);
            min = desc.m_rangeMin;
            max = desc.m_rangeMax;
            step = desc.m_step;
            hardMin = desc.m_bHardMin;
            hardMax = desc.m_bHardMax;
        }
        reflectedVar->m_softMinVal = min;
        reflectedVar->m_softMaxVal = max;

        if (hardMin)
        {
            reflectedVar->m_minVal = min;
        }
        else
        {
            reflectedVar->m_minVal = std::numeric_limits<int>::lowest();
        }
        if (hardMax)
        {
            reflectedVar->m_maxVal = max;
        }
        else
        {
            reflectedVar->m_maxVal = std::numeric_limits<int>::max();
        }
        reflectedVar->m_stepSize = step;
    }
}

void ReflectedVarIntAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarInt(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    UpdateRangeLimits(pVariable);
    Prop::Description desc(pVariable);
    m_valueMultiplier = desc.m_valueMultiplier;
}

void ReflectedVarIntAdapter::UpdateRangeLimits(IVariable *pVariable)
{
    setRangeParams<int>(m_reflectedVar.data(), pVariable);
}

void ReflectedVarIntAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    float value;
    if (pVariable->GetType() == IVariable::FLOAT)
    {
        pVariable->Get(value);
    }
    else
    {
        int intValue;
        pVariable->Get(intValue);
        value = intValue;
    }
    m_reflectedVar->m_value = std::round(value * m_valueMultiplier);
}

void ReflectedVarIntAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    //don't round here.  Often the IVariable is actually a float under-the hood
    //for example: DT_PERCENT is stored in float (0 to 1) but has ePropertyType::Integer because editor should be an integer editor ranging from 0 to 100.
    pVariable->Set(m_reflectedVar->m_value / m_valueMultiplier);
}



void ReflectedVarFloatAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarFloat(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    UpdateRangeLimits(pVariable);
    Prop::Description desc(pVariable);
    m_valueMultiplier = desc.m_valueMultiplier;
}

void ReflectedVarFloatAdapter::UpdateRangeLimits(IVariable *pVariable)
{
    setRangeParams<float>(m_reflectedVar.data(), pVariable);
}

void ReflectedVarFloatAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    float value;
    pVariable->Get(value);
    m_reflectedVar->m_value = value * m_valueMultiplier;
}

void ReflectedVarFloatAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(m_reflectedVar->m_value/m_valueMultiplier);
}



void ReflectedVarStringAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarString(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarStringAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    QString value;
    pVariable->Get(value);
    m_reflectedVar->m_value = value.toLatin1().data();
}

void ReflectedVarStringAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(m_reflectedVar->m_value.c_str());
}




void ReflectedVarBoolAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarBool(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarBoolAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    bool value;
    pVariable->Get(value);
    m_reflectedVar->m_value = value;
}

void ReflectedVarBoolAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(m_reflectedVar->m_value);
}




ReflectedVarEnumAdapter::ReflectedVarEnumAdapter()
    : m_updatingEnums(false)
    , m_pVariable(nullptr)
{}

void ReflectedVarEnumAdapter::SetVariable(IVariable *pVariable)
{
    m_pVariable = pVariable;
    Prop::Description desc(pVariable);
    m_enumList = desc.m_enumList;
    m_reflectedVar.reset(new CReflectedVarEnum<AZStd::string>(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    updateReflectedVarEnums();
}


void ReflectedVarEnumAdapter::updateReflectedVarEnums()
{
    if (!m_pVariable)
        return;

    m_updatingEnums = true;
    //Allow derived classes to populate the IVariable's enumList (used by AIWave and AITerritory)
    updateIVariableEnumList(m_pVariable);
    m_enumList = m_pVariable->GetEnumList();
    m_updatingEnums = false;

    //Copy the updated enums to the ReflecteVar
    if (m_enumList)
    {
        const AZStd::string oldValue = m_reflectedVar->m_value;
        AZStd::vector<AZStd::pair<AZStd::string, AZStd::string>> enums;
        for (uint i = 0; !m_enumList->GetItemName(i).isNull(); i++)
        {
            QString sEnumName = m_enumList->GetItemName(i);
            enums.push_back(AZStd::pair<AZStd::string, AZStd::string>(sEnumName.toLatin1().data(), sEnumName.toLatin1().data()));
        }
        m_reflectedVar->setEnums(enums);
        m_reflectedVar->setEnumValue(oldValue);
    }
}

void ReflectedVarEnumAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    const AZStd::string value = pVariable->GetDisplayValue().toLatin1().data();
    m_reflectedVar->setEnumByName(value);
}

void ReflectedVarEnumAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    QString iVarVal = m_reflectedVar->m_selectedEnumName.c_str();
    pVariable->SetDisplayValue(iVarVal);
}

void ReflectedVarEnumAdapter::OnVariableChange(IVariable* pVariable)
{
    //setting the enums on the pVariable will cause the variable to change getting us back here
    //The original property editor did need to update things immediately because it did so when creating the in-place editing control
    if (!m_updatingEnums)
        updateReflectedVarEnums();
}


static inline bool AlphabeticalBaseObjectLess(const CBaseObject* p1, const CBaseObject* p2)
{
    return p1->GetName() < p2->GetName();
}



ReflectedVarAITerritoryAdapter::ReflectedVarAITerritoryAdapter()
    :m_pWaveAdapter(nullptr)
{
}

void ReflectedVarAITerritoryAdapter::SetAIWaveAdapter(ReflectedVarAIWaveAdapter *adapter)
{
    m_pWaveAdapter = adapter;
}

void ReflectedVarAITerritoryAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    ReflectedVarEnumAdapter::SyncIVarToReflectedVar(pVariable);

    //Update AIWave selector with current AITerritory....it's enums depend on selected territory.
    if (m_pWaveAdapter)
        m_pWaveAdapter->SetCurrentTerritory(pVariable->GetDisplayValue());
}

void ReflectedVarAITerritoryAdapter::OnVariableChange(IVariable* var)
{
    ReflectedVarEnumAdapter::OnVariableChange(var);

    //Update AIWave selector with current AITerritory....it's enums depend on selected territory.
    if (m_pWaveAdapter)
        m_pWaveAdapter->SetCurrentTerritory(var->GetDisplayValue());
}

//Ported from CPropertyItem::PopulateAITerritoriesList 
void ReflectedVarAITerritoryAdapter::updateIVariableEnumList(IVariable *pIVariable)
{
    CVariableEnum<QString>* pVariable = static_cast<CVariableEnum<QString>*>(&*pIVariable);
    CVarEnumList<QString> *enumList = new CVarEnumList<QString>();

#ifndef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
    enumList->AddItem("<Auto>", "<Auto>");
#endif
    enumList->AddItem("<None>", "<None>");

    std::vector<CBaseObject*> vTerritories;
    GetIEditor()->GetObjectManager()->FindObjectsOfType(&CAITerritoryObject::staticMetaObject, vTerritories);
    std::sort(vTerritories.begin(), vTerritories.end(), AlphabeticalBaseObjectLess);

    for (std::vector<CBaseObject*>::iterator it = vTerritories.begin(); it != vTerritories.end(); ++it)
    {
        const QString& name = (*it)->GetName();
        enumList->AddItem(name, name);
    }
    //this will trigger a variable change and associated callbacks
    pVariable->SetEnumList(enumList);
}



void ReflectedVarAIWaveAdapter::SetCurrentTerritory(const QString &territory)
{
    m_currentTerritory = territory;
    updateReflectedVarEnums();
}

//Ported from CPropertyItem::PopulateAIWavesList 
void ReflectedVarAIWaveAdapter::updateIVariableEnumList(IVariable *pIVariable)
{
    CVariableEnum<QString>* pVariable = static_cast<CVariableEnum<QString>*>(&*pIVariable);
    CVarEnumList<QString> *enumList = new CVarEnumList<QString>();
    enumList->AddItem("<None>", "<None>");

#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
    if (m_currentTerritory != "<None>")
#else
    if ((m_currentTerritory != "<Auto>") && (m_currentTerritory != "<None>"))
#endif
    {
        std::vector<CAIWaveObject*> vLinkedAIWaves;

        CBaseObject* pBaseObject = GetIEditor()->GetObjectManager()->FindObject(m_currentTerritory);
        if (qobject_cast<CAITerritoryObject*>(pBaseObject))
        {
            CAITerritoryObject* pTerritory = static_cast<CAITerritoryObject*>(pBaseObject);
            pTerritory->GetLinkedWaves(vLinkedAIWaves);
        }

        std::sort(vLinkedAIWaves.begin(), vLinkedAIWaves.end(), AlphabeticalBaseObjectLess);

        for (std::vector<CAIWaveObject*>::iterator it = vLinkedAIWaves.begin(); it != vLinkedAIWaves.end(); ++it)
        {
            const QString& name = (*it)->GetName();
            enumList->AddItem(name, name);
        }
    }
    //this will trigger a variable change and associated callbacks
    pVariable->SetEnumList(enumList);
}


void ReflectedVarDBEnumAdapter::SetVariable(IVariable *pVariable)
{
    Prop::Description desc(pVariable);
    m_pEnumDBItem = desc.m_pEnumDBItem;
    m_reflectedVar.reset(new CReflectedVarEnum<AZStd::string>(pVariable->GetHumanName().toLatin1().data()));
    if (m_pEnumDBItem)
    {
        for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
        {
            QString name = m_pEnumDBItem->strings[i];
            m_reflectedVar->addEnum( m_pEnumDBItem->NameToValue(name).toLatin1().data(), name.toLatin1().data() );
        }
    }
}

void ReflectedVarDBEnumAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    const AZStd::string valueStr = pVariable->GetDisplayValue().toLatin1().data();
    const AZStd::string value = m_pEnumDBItem ? AZStd::string(m_pEnumDBItem->ValueToName(valueStr.c_str()).toLatin1().data()) : valueStr;
    m_reflectedVar->setEnumByName(value);

}

void ReflectedVarDBEnumAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    QString iVarVal = m_reflectedVar->m_selectedEnumName.c_str();
    if (m_pEnumDBItem)
    {
        iVarVal = m_pEnumDBItem->NameToValue(iVarVal);
    }
    pVariable->SetDisplayValue(iVarVal);
}



void ReflectedVarVector2Adapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarVector2(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    UpdateRangeLimits(pVariable);
}

void ReflectedVarVector2Adapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    Vec2 vec;
    pVariable->Get(vec);
    m_reflectedVar->m_value = AZ::Vector2(vec.x, vec.y);
}

void ReflectedVarVector2Adapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(Vec2(m_reflectedVar->m_value.GetX(), m_reflectedVar->m_value.GetY()));
}


void ReflectedVarVector3Adapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarVector3(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    UpdateRangeLimits(pVariable);
}

void ReflectedVarVector3Adapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    Vec3 vec;
    pVariable->Get(vec);
    m_reflectedVar->m_value = AZ::Vector3(vec.x, vec.y, vec.z);
}

void ReflectedVarVector3Adapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(Vec3(m_reflectedVar->m_value.GetX(), m_reflectedVar->m_value.GetY(), m_reflectedVar->m_value.GetZ()));
}


void ReflectedVarVector4Adapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarVector4(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
    UpdateRangeLimits(pVariable);
}

void ReflectedVarVector4Adapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    Vec4 vec;
    pVariable->Get(vec);
    m_reflectedVar->m_value = AZ::Vector4(vec.x, vec.y, vec.z, vec.w);
}

void ReflectedVarVector4Adapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(Vec4(m_reflectedVar->m_value.GetX(), m_reflectedVar->m_value.GetY(), m_reflectedVar->m_value.GetZ(), m_reflectedVar->m_value.GetW()));
}


void ReflectedVarColorAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarColor(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarColorAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    if (pVariable->GetType() == IVariable::VECTOR)
    {
        Vec3 v(0, 0, 0);
        pVariable->Get(v);
        const QColor col = ColorLinearToGamma(ColorF(v.x, v.y, v.z));
        m_reflectedVar->m_color.Set(col.redF(), col.greenF(), col.blueF());
    }
    else
    {
        int col(0);
        pVariable->Get(col);
        const QColor qcolor = ColorToQColor((uint32)col);
        m_reflectedVar->m_color.Set(qcolor.redF(), qcolor.greenF(), qcolor.blueF());
    }
}

void ReflectedVarColorAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    if (pVariable->GetType() == IVariable::VECTOR)
    {
        ColorF colLin = ColorGammaToLinear(QColor::fromRgbF(m_reflectedVar->m_color.GetX(), m_reflectedVar->m_color.GetY(), m_reflectedVar->m_color.GetZ()));
        pVariable->Set(Vec3(colLin.r, colLin.g, colLin.b));
    }
    else
    {
        int ir = m_reflectedVar->m_color.GetX() * 255.0f;
        int ig = m_reflectedVar->m_color.GetY() * 255.0f;
        int ib = m_reflectedVar->m_color.GetZ() * 255.0f;

        pVariable->Set(static_cast<int>(RGB(ir, ig, ib)));
    }
}



void ReflectedVarAnimationAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarAnimation(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarAnimationAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    m_reflectedVar->m_entityID = static_cast<AZ::EntityId>(pVariable->GetUserData().value<AZ::u64>());
    m_reflectedVar->m_animation = pVariable->GetDisplayValue().toLatin1().data();
}

void ReflectedVarAnimationAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->SetUserData(static_cast<AZ::u64>(m_reflectedVar->m_entityID));
    pVariable->SetDisplayValue(m_reflectedVar->m_animation.c_str());

}

void ReflectedVarResourceAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarResource(pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarResourceAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    QString path;
    pVariable->Get(path);
    m_reflectedVar->m_path = path.toLatin1().data();
    Prop::Description desc(pVariable);
    m_reflectedVar->m_propertyType = desc.m_type;

}

void ReflectedVarResourceAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    const bool bForceModified = (m_reflectedVar->m_propertyType == ePropertyGeomCache);
    pVariable->SetForceModified(bForceModified);
    pVariable->SetDisplayValue(m_reflectedVar->m_path.c_str());

    //shouldn't be able to change the type, so ignore m_reflecatedVar->m_properyType
}


ReflectedVarGenericPropertyAdapter::ReflectedVarGenericPropertyAdapter(PropertyType propertyType)
    :m_propertyType(propertyType)
{}

void ReflectedVarGenericPropertyAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarGenericProperty(m_propertyType, pVariable->GetHumanName().toLatin1().data()));
    m_reflectedVar->m_description = pVariable->GetDescription().toLatin1().data();
}

void ReflectedVarGenericPropertyAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    QString value;
    pVariable->Get(value);
    if (m_reflectedVar->m_propertyType == ePropertyMaterial)
        value.replace('\\', '/');

    m_reflectedVar->m_value = value.toLatin1().data();
}

void ReflectedVarGenericPropertyAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(m_reflectedVar->m_value.c_str());
}

void ReflectedVarUserAdapter::SetVariable(IVariable *pVariable)
{
    m_reflectedVar.reset(new CReflectedVarUser( pVariable->GetHumanName().toLatin1().data()));
}

void ReflectedVarUserAdapter::SyncReflectedVarToIVar(IVariable *pVariable)
{
    QString value;
    pVariable->Get(value);
    m_reflectedVar->m_value = value.toLatin1().data();

    //extract the list of custom items from the IVariable user data
    IVariable::IGetCustomItems* pGetCustomItems = static_cast<IVariable::IGetCustomItems*> (pVariable->GetUserData().value<void *>());
    if (pGetCustomItems != 0)
    {
        std::vector<IVariable::IGetCustomItems::SItem> items;
        QString dlgTitle;
        // call the user supplied callback to fill-in items and get dialog title
        bool bShowIt = pGetCustomItems->GetItems(pVariable, items, dlgTitle);
        if (bShowIt) // if func didn't veto, show the dialog
        {
            m_reflectedVar->m_enableEdit = true;
            m_reflectedVar->m_useTree = pGetCustomItems->UseTree();
            m_reflectedVar->m_treeSeparator = pGetCustomItems->GetTreeSeparator();
            m_reflectedVar->m_dialogTitle = dlgTitle.toLatin1().data();
            m_reflectedVar->m_itemNames.resize(items.size());
            m_reflectedVar->m_itemDescriptions.resize(items.size());

            QByteArray ba;
            int i = -1;
            std::generate(m_reflectedVar->m_itemNames.begin(), m_reflectedVar->m_itemNames.end(), [&items, &i, &ba]() { ++i; ba = items[i].name.toLatin1(); return ba.data(); });
            i = -1;
            std::generate(m_reflectedVar->m_itemDescriptions.begin(), m_reflectedVar->m_itemDescriptions.end(), [&items, &i, &ba]() { ++i; ba = items[i].desc.toLatin1(); return ba.data(); });

        }
    }
    else
    {
        m_reflectedVar->m_enableEdit = false;
    }
}

void ReflectedVarUserAdapter::SyncIVarToReflectedVar(IVariable *pVariable)
{
    pVariable->Set(m_reflectedVar->m_value.c_str());
}


ReflectedVarSplineAdapter::ReflectedVarSplineAdapter(ReflectedPropertyItem *parentItem, PropertyType propertyType)
    : m_propertyType(propertyType)
    , m_bDontSendToControl(false)
    , m_parentItem(parentItem)
{
}

void ReflectedVarSplineAdapter::SetVariable(IVariable* pVariable)
{
    m_reflectedVar.reset(new CReflectedVarSpline(m_propertyType, pVariable->GetHumanName().toLatin1().data()));

}

void ReflectedVarSplineAdapter::SyncReflectedVarToIVar(IVariable* pVariable)
{
    if (!m_bDontSendToControl)
    {
        m_reflectedVar->m_spline = reinterpret_cast<uint64_t>(pVariable->GetSpline());
    }
}


void ReflectedVarSplineAdapter::SyncIVarToReflectedVar(IVariable* pVariable)
{
    // Splines update variables directly so don't call OnVariableChange or SetValue here or values will be overwritten.

    // Call OnSetValue to force this field to notify this variable that its model has changed without going through the
    // full OnVariableChange pass
    //
    // Set m_bDontSendToControl to prevent the control's data from being overwritten (as the variable's data won't
    // necessarily be up to date vs the controls at the point this happens).
    m_bDontSendToControl = true;
    pVariable->OnSetValue(false);
    m_bDontSendToControl = false;

    m_parentItem->SendOnItemChange();
}
