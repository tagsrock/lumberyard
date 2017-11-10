#include "stdafx.h"

#include "PropertyResourceCtrl.h"
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QVector>
#include "IAssetBrowser.h"
#include "IResourceSelectorHost.h"
#include "AssetBrowser/AssetBrowserDialog.h"
#include "EditorCoreAPI.h"
#include <Controls/QToolTipWidget.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>

BrowseButton::BrowseButton(PropertyType type, QWidget* parent /*= nullptr*/)
    : QToolButton(parent)
    , m_propertyType(type)
{
    connect(this, &QAbstractButton::clicked, this, &BrowseButton::OnClicked);
    setText("...");
}

void BrowseButton::SetPathAndEmit(const QString& path)
{
    //only emit if path changes, except for ePropertyGeomCache. Old property control
    if (path != m_path || m_propertyType == ePropertyGeomCache)
    {
        m_path = path;
        emit PathChanged(m_path);
    }
}

class FileBrowseButton
    : public BrowseButton
{
public:
    AZ_CLASS_ALLOCATOR(FileBrowseButton, AZ::SystemAllocator, 0);
    FileBrowseButton(PropertyType type, QWidget* pParent = nullptr)
        : BrowseButton(type, pParent)
    {
        setIcon(QIcon(":/reflectedPropertyCtrl/img/file_browse.png"));
    }

private:
    void OnClicked() override
    {
        QString tempValue("");
        QString ext("");
        if (m_path.isEmpty() == false)
        {
            if (Path::GetExt(m_path) == "")
            {
                tempValue = "";
            }
            else
            {
                tempValue = m_path;
            }
        }

        AssetSelectionModel selection;

        if (m_propertyType == ePropertyTexture)
        {
            // Filters for texture.
            selection = AssetSelectionModel::AssetGroupSelection("Texture");
        }
        else if (m_propertyType == ePropertyModel)
        {
            // Filters for models.
            selection = AssetSelectionModel::AssetGroupSelection("Geometry");
        }
        else if (m_propertyType == ePropertyGeomCache)
        {
            // Filters for geom caches.
            selection = AssetSelectionModel::AssetTypeSelection("Geom Cache");
        }
        else if (m_propertyType == ePropertyFile)
        {
            // Filters for files.
            selection = AssetSelectionModel::AssetTypeSelection("File");
        }
        else
        {
            return;
        }

        AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
        if (selection.IsValid())
        {
            QString newPath = Path::FullPathToGamePath(selection.GetResult()->GetFullPath().c_str()).c_str();

            switch (m_propertyType)
            {
            case ePropertyTexture:
            case ePropertyModel:
            case ePropertyMaterial:
                newPath.replace("\\\\", "/");
            }
            switch (m_propertyType)
            {
            case ePropertyTexture:
            case ePropertyModel:
            case ePropertyMaterial:
            case ePropertyFile:
                if (newPath.size() > MAX_PATH)
                {
                    newPath.resize(MAX_PATH);
                }
            }

            SetPathAndEmit(newPath);
        }
    }
};

class ResourceSelectorButton
    : public BrowseButton
{
public:
    AZ_CLASS_ALLOCATOR(ResourceSelectorButton, AZ::SystemAllocator, 0);

    ResourceSelectorButton(PropertyType type, QWidget* pParent = nullptr)
        : BrowseButton(type, pParent)
    {
        setToolTip(tr("Select resource"));
    }

private:
    void OnClicked() override
    {
        SResourceSelectorContext x;
        x.parentWidget = this;
        x.typeName = Prop::GetPropertyTypeToResourceType(m_propertyType);
        QString newPath = GetIEditor()->GetResourceSelectorHost()->SelectResource(x, m_path.toStdString().c_str()).c_str();
        SetPathAndEmit(newPath);
    }
};

class AssetBrowserButton
    : public BrowseButton
{
public:
    AZ_CLASS_ALLOCATOR(AssetBrowserButton, AZ::SystemAllocator, 0);
    AssetBrowserButton(PropertyType type, QWidget* pParent = nullptr)
        : BrowseButton(type, pParent)
    {
        if (type == ePropertyTexture)
        {
            setIcon(QIcon(":/reflectedPropertyCtrl/img/texture_browse.png"));
        }
    }

private:
    void OnClicked() override
    {
        switch (m_propertyType)
        {
        case ePropertyTexture:
        {
            QString strInputString = Path::GetRelativePath(m_path);
            CAssetBrowserDialog::Open(strInputString.toLatin1().data(), "Textures");
            break;
        }
        case ePropertyMaterial:
        {
            CAssetBrowserDialog::Open(m_path.toStdString().c_str(), "Materials");
            return;
            break;
        }
        default:
            break;
        }
    }
};

class AssetBrowserApplyButton
    : public BrowseButton
{
public:
    AZ_CLASS_ALLOCATOR(AssetBrowserApplyButton, AZ::SystemAllocator, 0);
    AssetBrowserApplyButton(PropertyType type, QWidget* pParent = nullptr)
        : BrowseButton(type, pParent)
    {
        setIcon(QIcon(":/reflectedPropertyCtrl/img/apply.png"));
    }

private:
    void OnClicked() override
    {
        switch (m_propertyType)
        {
        case ePropertyTexture:
        case ePropertyMaterial:
            if (GetIEditor()->GetAssetBrowser()->isAvailable())
            {
                SetPathAndEmit(QString(GetIEditor()->GetAssetBrowser()->GetFirstSelectedFilename()));
            }
            break;
        default:
            break;
        }
    }
};

class TextureEditButton
    : public BrowseButton
{
public:
    AZ_CLASS_ALLOCATOR(TextureEditButton, AZ::SystemAllocator, 0);
    TextureEditButton(QWidget* pParent = nullptr)
        : BrowseButton(ePropertyTexture, pParent)
    {
        setIcon(QIcon(":/reflectedPropertyCtrl/img/texture_edit.png"));
    }

private:
    void OnClicked() override
    {
        CFileUtil::EditTextureFile(m_path.toLatin1().data(), true);
    }
};

FileResourceSelectorWidget::FileResourceSelectorWidget(QWidget* pParent /*= nullptr*/)
    : QWidget(pParent)
    , m_propertyType(ePropertyInvalid)
{
    m_pathEdit = new QLineEdit;
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->addWidget(m_pathEdit, 1);

    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tooltip = new QToolTipWidget(this);

    installEventFilter(this);

    connect(m_pathEdit, &QLineEdit::editingFinished, [this]() { OnPathChanged(m_pathEdit->text()); });
}

bool FileResourceSelectorWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (m_propertyType == ePropertyTexture)
    {
        if (event->type() == QEvent::ToolTip)
        {
            QHelpEvent* e = (QHelpEvent*)event;

            m_tooltip->AddSpecialContent("TEXTURE", m_path);
            m_tooltip->TryDisplay(e->globalPos(), m_pathEdit, QToolTipWidget::ArrowDirection::ARROW_RIGHT);

            return true;
        }

        if (event->type() == QEvent::Leave)
        {
            m_tooltip->hide();
        }
    }

    return false;
}

void FileResourceSelectorWidget::SetPropertyType(PropertyType type)
{
    if (m_propertyType == type)
    {
        return;
    }

    //if the property type changed for some reason, delete all the existing widgets
    if (!m_buttons.isEmpty())
    {
        qDeleteAll(m_buttons.begin(), m_buttons.end());
        m_buttons.clear();
    }

    m_propertyType = type;

    switch (type)
    {
    case ePropertyTexture:
        AddButton(new FileBrowseButton(type));
        AddButton(new AssetBrowserButton(type));
        AddButton(new AssetBrowserApplyButton(type));
        AddButton(new TextureEditButton);
        break;
    case ePropertyModel:
    case ePropertyGeomCache:
    case ePropertyAudioTrigger:
    case ePropertyAudioSwitch:
    case ePropertyAudioSwitchState:
    case ePropertyAudioRTPC:
    case ePropertyAudioEnvironment:
    case ePropertyAudioPreloadRequest:
        AddButton(new ResourceSelectorButton(type));
        break;
    case ePropertyFile:
        AddButton(new FileBrowseButton(type));
        break;
    default:
        break;
    }

    m_mainLayout->invalidate();
}

void FileResourceSelectorWidget::AddButton(BrowseButton* button)
{
    m_mainLayout->addWidget(button);
    m_buttons.push_back(button);
    connect(button, &BrowseButton::PathChanged, this, &FileResourceSelectorWidget::OnPathChanged);
}

void FileResourceSelectorWidget::OnPathChanged(const QString& path)
{
    SetPath(path);
    emit PathChanged(m_path);
}

void FileResourceSelectorWidget::SetPath(const QString& path)
{
#ifdef KDAB_PORT_TODO
    const bool bForceModified = (ftype == IFileUtil::EFILE_TYPE_GEOMCACHE);
#endif

    const QString newPath = path.toLower();

    if (m_path != newPath)
    {
        m_path = newPath;
        UpdateWidgets();
    }
}


void FileResourceSelectorWidget::UpdateWidgets()
{
    m_pathEdit->setText(m_path);

    foreach(BrowseButton * button, m_buttons)
    {
        button->SetPath(m_path);
    }
}

QString FileResourceSelectorWidget::GetPath() const
{
    return m_path;
}



QWidget* FileResourceSelectorWidget::GetLastInTabOrder()
{
    return m_buttons.empty() ? nullptr : m_buttons.last();
}

QWidget* FileResourceSelectorWidget::GetFirstInTabOrder()
{
    return m_buttons.empty() ? nullptr : m_buttons.first();
}

void FileResourceSelectorWidget::UpdateTabOrder()
{
    if (m_buttons.count() >= 2)
    {
        for (int i = 0; i < m_buttons.count() - 1; ++i)
        {
            setTabOrder(m_buttons[i], m_buttons[i + 1]);
        }
    }
}

QWidget* FileResourceSelectorWidgetHandler::CreateGUI(QWidget* pParent)
{
    FileResourceSelectorWidget* newCtrl = aznew FileResourceSelectorWidget(pParent);
    connect(newCtrl, &FileResourceSelectorWidget::PathChanged, [newCtrl]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, newCtrl);
        });
    return newCtrl;
}

void FileResourceSelectorWidgetHandler::ConsumeAttribute(FileResourceSelectorWidget* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
    Q_UNUSED(GUI);
    Q_UNUSED(attrib);
    Q_UNUSED(attrValue);
    Q_UNUSED(debugName);
}

void FileResourceSelectorWidgetHandler::WriteGUIValuesIntoProperty(size_t index, FileResourceSelectorWidget* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    Q_UNUSED(index);
    Q_UNUSED(node);
    CReflectedVarResource val = instance;
    val.m_propertyType = GUI->GetPropertyType();
    val.m_path = GUI->GetPath().toLatin1().data();
    instance = static_cast<property_t>(val);
}

bool FileResourceSelectorWidgetHandler::ReadValuesIntoGUI(size_t index, FileResourceSelectorWidget* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    Q_UNUSED(index);
    Q_UNUSED(node);
    CReflectedVarResource val = instance;
    GUI->SetPropertyType(val.m_propertyType);
    GUI->SetPath(val.m_path.c_str());
    return false;
}

#include <Controls/ReflectedPropertyControl/PropertyResourceCtrl.moc>

