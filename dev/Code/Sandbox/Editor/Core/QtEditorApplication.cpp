
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
#include "StdAfx.h"

#include "QtEditorApplication.h"

#include <QByteArray>
#include <QWidget>
#include <QWheelEvent>
#include <QAbstractEventDispatcher>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QQmlEngine>
#include <QDebug>
#include "../Plugins/EditorUI_QT/UIFactory.h"
#include <AzQtComponents/Components/LumberyardStylesheet.h>
#include <AzQtComponents/Utilities/QtPluginPaths.h>
#include <QToolBar>
#include <QTimer>

#if defined(AZ_PLATFORM_WINDOWS)
#include <QtGui/qpa/qplatformnativeinterface.h>
#endif

#include "Material/MaterialManager.h"

#include "Util/BoostPythonHelpers.h"

#include <AzCore/EBus/EBus.h>
#include <AzCore/UserSettings/UserSettings.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Component/Entity.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserComponent.h>

#if defined(AZ_PLATFORM_WINDOWS)
#   include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_win.h>
#elif defined(AZ_PLATFORM_APPLE_OSX)
/*
#   include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_darwin.h>
@class NSEvent
 */
#endif // defined(AZ_PLATFORM_*)

enum
{
    // in milliseconds
    GameModeIdleFrequency = 0,
    EditorModeIdleFrequency = 1,
    InactiveModeFrequency = 10,
};

// QML imports that go in the editor folder (relative to the project root)
#define QML_IMPORT_USER_LIB_PATH "Editor/UI/qml"

// QML Imports that are part of Qt (relative to the executable)
#define QML_IMPORT_SYSTEM_LIB_PATH "qtlibs/qml"


// internal, private namespace:
namespace
{
    class RecursionGuard
    {
    public:
        RecursionGuard(bool& value)
            : m_refValue(value)
        {
            m_reset = !value;
            m_refValue = true;
        }

        ~RecursionGuard()
        {
            if (m_reset)
            {
                m_refValue = false;
            }
        }

        bool areWeRecursing()
        {
            return !m_reset;
        }

    private:
        bool& m_refValue;
        bool m_reset;
    };

    class GlobalEventFilter
        : public QObject
    {
    public:
        GlobalEventFilter(QObject* watch)
            : QObject(watch) {}

        bool eventFilter(QObject* obj, QEvent* e)
        {
            static bool recursionChecker = false;
            RecursionGuard guard(recursionChecker);

            if (guard.areWeRecursing())
            {
                return false;
            }

            switch (e->type())
            {
                case QEvent::Wheel:
                {
                    QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(e);

                    // make the wheel event fall through to windows underneath the mouse, even if they don't have focus
                    QWidget* widget = QApplication::widgetAt(wheelEvent->globalPos());
                    if ((widget != nullptr) && (obj != widget))
                    {
                        return QApplication::instance()->sendEvent(widget, e);
                    }
                }
                break;

                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                {
                    if (GetIEditor()->IsInGameMode())
                    {
                        // don't let certain keys fall through to the game when it's running
                        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
                        auto key = keyEvent->key();

                        if ((key == Qt::Key_Alt) || (key == Qt::Key_AltGr) || ((key >= Qt::Key_F1) && (key <= Qt::Key_F35)))
                        {
                            return true;
                        }
                    }
                }
                break;

                case QEvent::Shortcut:
                {
                    // eat shortcuts in game mode
                    if (GetIEditor()->IsInGameMode())
                    {
                        return true;
                    }
                }
                break;
            }

            return false;
        }
    };

    static void LogToDebug(QtMsgType Type, const QMessageLogContext& Context, const QString& message)
    {
#if defined(WIN32) || defined(WIN64)
        OutputDebugStringW(L"Qt: ");
        OutputDebugStringW(reinterpret_cast<const wchar_t*>(message.utf16()));
        OutputDebugStringW(L"\n");
#endif
    }
}

namespace Editor
{
    void ScanDirectories(QFileInfoList& directoryList, const QStringList& filters, QFileInfoList& files, ScanDirectoriesUpdateCallBack updateCallback)
    {
        while (!directoryList.isEmpty())
        {
            QDir directory(directoryList.front().absoluteFilePath(), "*", QDir::Name | QDir::IgnoreCase, QDir::AllEntries);
            directoryList.pop_front();

            if (directory.exists())
            {
                // Append each file from this directory that matches one of the filters to files
                directory.setNameFilters(filters);
                directory.setFilter(QDir::Files);
                files.append(directory.entryInfoList());

                // Add all of the subdirectories from this directory to the queue to be searched
                directory.setNameFilters(QStringList("*"));
                directory.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
                directoryList.append(directory.entryInfoList());
                if (updateCallback)
                {
                    updateCallback();
                }
            }
        }
    }

    EditorQtApplication::EditorQtApplication(int& argc, char** argv)
        : QApplication(argc, argv)
        , m_inWinEventFilter(false)
        , m_stylesheet(new AzQtComponents::LumberyardStylesheet(this))
        , m_idleTimer(new QTimer(this))
        , m_qtEntity(aznew AZ::Entity())
    {
        setWindowIcon(QIcon(":/Application/res/editor_icon.ico"));

        // set the default key store for our preferences:
        setOrganizationName("Amazon");
        setOrganizationDomain("amazon.com");
        setApplicationName("Lumberyard");

        connect(m_idleTimer, &QTimer::timeout, this, &EditorQtApplication::maybeProcessIdle);

        connect(this, &QGuiApplication::applicationStateChanged, [this]() {
            ResetIdleTimer(GetIEditor() ? GetIEditor()->IsInGameMode() : true);
        });
        installEventFilter(this);
    }

    void EditorQtApplication::Initialize()
    {
        GetIEditor()->RegisterNotifyListener(this);

        m_stylesheet->Initialize(this);

        // install QTranslator
        InstallEditorTranslators();

        // install hooks and filters last and revoke them first
        InstallFilters();
        InitializeQML();

        // install this filter. It will be a parent of the application and cleaned up when it is cleaned up automically
        auto globalEventFilter = new GlobalEventFilter(this);
        installEventFilter(globalEventFilter);

        //Setup reusable dialogs
        UIFactory::Initialize();

        InitQtEntity();
    }

    void EditorQtApplication::InitQtEntity()
    {
        m_qtEntity->AddComponent(aznew AzToolsFramework::AssetBrowser::AssetBrowserComponent());
        m_qtEntity->Init();
        m_qtEntity->Activate();
    }

    void EditorQtApplication::LoadSettings() 
    {
        AZ::SerializeContext* context;
        EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(context, "No serialize context");
        char resolvedPath[AZ_MAX_PATH_LEN];
        AZ::IO::FileIOBase::GetInstance()->ResolvePath("@user@/EditorUserSettings.xml", resolvedPath, AZ_MAX_PATH_LEN);
        m_localUserSettings.Load(resolvedPath, context);
        m_localUserSettings.Activate(AZ::UserSettings::CT_LOCAL);
        m_activatedLocalUserSettings = true;
    }

    void EditorQtApplication::SaveSettings()
    {
        if (m_activatedLocalUserSettings)
        {
            AZ::SerializeContext* context;
            EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
            AZ_Assert(context, "No serialize context");
            char resolvedPath[AZ_MAX_PATH_LEN];
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@user@/EditorUserSettings.xml", resolvedPath, AZ_ARRAY_SIZE(resolvedPath));
            m_localUserSettings.Save(resolvedPath, context);
            m_localUserSettings.Deactivate();
            m_activatedLocalUserSettings = false;
        }
    }

    void EditorQtApplication::maybeProcessIdle()
    {
        if (!m_isMovingOrResizing)
        {
            if (auto winapp = CCryEditApp::instance())
            {
                winapp->OnIdle(0);
            }
        }
    }

    void EditorQtApplication::InstallQtLogHandler()
    {
        qInstallMessageHandler(LogToDebug);
    }

    void EditorQtApplication::InstallFilters()
    {
        if (auto dispatcher = QAbstractEventDispatcher::instance())
        {
            dispatcher->installNativeEventFilter(this);
        }
    }

    void EditorQtApplication::UninstallFilters()
    {
        if (auto dispatcher = QAbstractEventDispatcher::instance())
        {
            dispatcher->removeNativeEventFilter(this);
        }
    }

    EditorQtApplication::~EditorQtApplication()
    {
        GetIEditor()->UnregisterNotifyListener(this);

        //Clean reusable dialogs
        UIFactory::Deinitialize();

        UninitializeQML();
        UninstallFilters();

        UninstallEditorTranslators();
    }

    static QWindow* windowForWidget(const QWidget* widget)
    {
        QWindow* window = widget->windowHandle();
        if (window)
        {
            return window;
        }
        const QWidget* nativeParent = widget->nativeParentWidget();
        if (nativeParent)
        {
            return nativeParent->windowHandle();
        }

        return nullptr;
    }

#if defined(AZ_PLATFORM_WINDOWS)
    static HWND getHWNDForWidget(const QWidget* widget)
    {
        QWindow* window = windowForWidget(widget);
        if (window && window->handle())
        {
            QPlatformNativeInterface* nativeInterface = QGuiApplication::platformNativeInterface();
            return static_cast<HWND>(nativeInterface->nativeResourceForWindow(QByteArrayLiteral("handle"), window));
        }

        // Using NULL here because it's Win32 and that's what they use
        return NULL;
    }
#endif

    bool EditorQtApplication::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
    {
#if defined(AZ_PLATFORM_WINDOWS)
        MSG* msg = (MSG*)message;

        if (msg->message == WM_MOVING || msg->message == WM_SIZING)
        {
            m_isMovingOrResizing = true;
        }
        else if (msg->message == WM_EXITSIZEMOVE)
        {
            m_isMovingOrResizing = false;
        }

        // Ensure that the Windows WM_INPUT messages get passed through to the AzFramework input system,
        // but only while in game mode so we don't accumulate raw input events before we start actually
        // ticking the input devices, otherwise the queued events will get sent when entering game mode.
        if (msg->message == WM_INPUT && GetIEditor()->IsInGameMode())
        {
            UINT rawInputSize;
            const UINT rawInputHeaderSize = sizeof(RAWINPUTHEADER);
            GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, NULL, &rawInputSize, rawInputHeaderSize);

            LPBYTE rawInputBytes = new BYTE[rawInputSize];
            CRY_ASSERT(rawInputBytes);

            const UINT bytesCopied = GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, rawInputBytes, &rawInputSize, rawInputHeaderSize);
            CRY_ASSERT(bytesCopied == rawInputSize);

            RAWINPUT* rawInput = (RAWINPUT*)rawInputBytes;
            CRY_ASSERT(rawInput);

            EBUS_EVENT(AzFramework::RawInputNotificationBusWin, OnRawInputEvent, *rawInput);
            return false;
        }
        else if (msg->message == WM_DEVICECHANGE)
        {
            if (msg->wParam == 0x0007) // DBT_DEVNODES_CHANGED
            {
                EBUS_EVENT(AzFramework::RawInputNotificationBusWin, OnRawInputDeviceChangeEvent);
            }
            return true;
        }
#elif defined(AZ_PLATFORM_APPLE_OSX)
        // ToDo: Enable this once we need game mode input on OSX
        // if (GetIEditor()->IsInGameMode())
        // {
        //     NSEvent* event = (NSEvent*)message;
        //     EBUS_EVENT(AzFramework::RawInputNotificationBusOsx, OnRawInputEvent, event);
        // }
#endif
        return false;
    }

    void EditorQtApplication::OnEditorNotifyEvent(EEditorNotifyEvent event)
    {
        switch (event)
        {
            case eNotify_OnStyleChanged:
                RefreshStyleSheet();
                emit skinChanged();
            break;

            case eNotify_OnQuit:
                GetIEditor()->UnregisterNotifyListener(this);
            break;

            case eNotify_OnEndGameMode:
                ResetIdleTimer(false);
            break;

            case eNotify_OnBeginGameMode:
                ResetIdleTimer(true);
            break;
        }
    }

    QColor EditorQtApplication::InterpolateColors(QColor a, QColor b, float factor)
    {
        return QColor(int(a.red() * (1.0f - factor) + b.red() * factor),
            int(a.green() * (1.0f - factor) + b.green() * factor),
            int(a.blue() * (1.0f - factor) + b.blue() * factor),
            int(a.alpha() * (1.0f - factor) + b.alpha() * factor));
    }

    static void WriteStylesheetForQtDesigner(const QString& processedStyle)
    {
        QString outputStylePath = QDir::cleanPath(QDir::homePath() + QDir::separator() + "lumberyard_editor_stylesheet.qss");
        QFile outputStyleFile(outputStylePath);
        bool successfullyWroteStyleFile = false;
        if (outputStyleFile.open(QFile::WriteOnly))
        {
            QTextStream outStream(&outputStyleFile);
            outStream << processedStyle;
            outputStyleFile.close();
            successfullyWroteStyleFile = true;

            if (GetIEditor() != nullptr)
            {
                if (GetIEditor()->GetSystem() != nullptr)
                {
                    if (GetIEditor()->GetSystem()->GetILog() != nullptr)
                    {
                        GetIEditor()->GetSystem()->GetILog()->LogWithType(IMiniLog::eMessage, "Wrote LumberYard's compiled Qt Style to '%s'", outputStylePath.toLatin1().data());
                    }
                }
            }
        }
    }

    void EditorQtApplication::RefreshStyleSheet()
    {
        m_stylesheet->Refresh(this);
    }

    void EditorQtApplication::InitializeQML()
    {
        if (!m_qmlEngine)
        {
            m_qmlEngine = new QQmlEngine();

            // assumption:  Qt is already initialized.  You can use Qt's stuff to do anything you want now.
            QDir appDir(QCoreApplication::applicationDirPath());
            m_qmlEngine->addImportPath(appDir.filePath(QML_IMPORT_SYSTEM_LIB_PATH));
            // now find engine root and use that:

            QString rootDir = AzQtComponents::FindEngineRootDir(this);
            if (!rootDir.isEmpty())
            {
                m_qmlEngine->addImportPath(QDir(rootDir).filePath(QML_IMPORT_USER_LIB_PATH));
            }

            // now that QML is initialized, broadcast to any interested parties:
            GetIEditor()->Notify(eNotify_QMLReady);
        }
    }


    void EditorQtApplication::UninitializeQML()
    {
        if (m_qmlEngine)
        {
            GetIEditor()->Notify(eNotify_BeforeQMLDestroyed);
            delete m_qmlEngine;
            m_qmlEngine = nullptr;
        }
    }

    void EditorQtApplication::setIsMovingOrResizing(bool isMovingOrResizing)
    {
        if (m_isMovingOrResizing == isMovingOrResizing)
        {
            return;
        }

        m_isMovingOrResizing = isMovingOrResizing;
    }

    const QColor& EditorQtApplication::GetColorByName(const QString& name)
    {
        return m_stylesheet->GetColorByName(name);
    }

    QQmlEngine* EditorQtApplication::GetQMLEngine() const
    {
        return m_qmlEngine;
    }

    EditorQtApplication* EditorQtApplication::instance()
    {
        return static_cast<EditorQtApplication*>(QApplication::instance());
    }

    bool EditorQtApplication::IsActive()
    {
        return applicationState() == Qt::ApplicationActive;
    }

    QTranslator* EditorQtApplication::CreateAndInitializeTranslator(const QString& filename, const QString& directory)
    {
        Q_ASSERT(QFile::exists(directory + "/" + filename));

        QTranslator* translator = new QTranslator();
        translator->load(filename, directory);
        installTranslator(translator);
        return translator;
    }

    void EditorQtApplication::InstallEditorTranslators()
    {
        m_editorTranslator =        CreateAndInitializeTranslator("editor_en-us.qm", ":/Translations");
        m_flowgraphTranslator =     CreateAndInitializeTranslator("flowgraph_en-us.qm", ":/Translations");
        m_assetBrowserTranslator =  CreateAndInitializeTranslator("assetbrowser_en-us.qm", ":/Translations");
    }

    void EditorQtApplication::DeleteTranslator(QTranslator*& translator)
    {
        removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    void EditorQtApplication::UninstallEditorTranslators()
    {
        DeleteTranslator(m_editorTranslator);
        DeleteTranslator(m_flowgraphTranslator);
        DeleteTranslator(m_assetBrowserTranslator);
    }

    void EditorQtApplication::EnableOnIdle(bool enable)
    {
        if (enable)
        {
            m_idleTimer->start();
        }
        else
        {
            m_idleTimer->stop();
        }
    }

    void EditorQtApplication::ResetIdleTimer(bool isInGameMode)
    {
        bool isActive = true;

        int timerFrequency = InactiveModeFrequency;

        if (isActive)
        {
            if (isInGameMode)
            {
                timerFrequency = GameModeIdleFrequency;
            }
            else
            {
                timerFrequency = EditorModeIdleFrequency;
            }
        }
        EnableOnIdle(isActive);
    }

    bool EditorQtApplication::eventFilter(QObject* object, QEvent* event)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
            m_pressedButtons |= reinterpret_cast<QMouseEvent*>(event)->button();
            break;
        case QEvent::MouseButtonRelease:
            m_pressedButtons &= ~(reinterpret_cast<QMouseEvent*>(event)->button());
            break;
        case QEvent::KeyPress:
            m_pressedKeys.insert(reinterpret_cast<QKeyEvent*>(event)->key());
            break;
        case QEvent::KeyRelease:
            m_pressedKeys.remove(reinterpret_cast<QKeyEvent*>(event)->key());
            break;
        default:
            break;
        }
        return QApplication::eventFilter(object, event);
    }
} // end namespace Editor

#include <Core/QtEditorApplication.moc>
