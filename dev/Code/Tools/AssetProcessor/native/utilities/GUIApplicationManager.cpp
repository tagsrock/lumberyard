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
#include "native/utilities/GUIApplicationManager.h"
#include "native/utilities/AssetUtils.h"
#include "native/AssetManager/assetProcessorManager.h"
#include "native/connection/connectionManager.h"
#include "native/utilities/IniConfiguration.h"
#include "native/utilities/ApplicationServer.h"
#include "native/resourcecompiler/rccontroller.h"
#include "native/FileServer/fileServer.h"
#include "native/AssetManager/assetScanner.h"
#include "native/shadercompiler/shadercompilerManager.h"
#include "native/shadercompiler/shadercompilerModel.h"
#include "native/AssetManager/AssetRequestHandler.h"
#include "native/utilities/ByteArrayStream.h"
#include <functional>
#include <QProcess>
#include <QThread>
#include <QApplication>
#include <QStringList>
#include <QStyleFactory>

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <AzQtComponents/Components/StylesheetPreprocessor.h>
#include <AzCore/base.h>
#include <AzCore/debug/trace.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>

#if defined(EXTERNAL_CRASH_REPORTING)
#include <CrashHandler.h>
#endif

#define STYLE_SHEET_VARIABLES_PATH_DARK "Editor/Styles/EditorStylesheetVariables_Dark.json"
#define STYLE_SHEET_VARIABLES_PATH_LIGHT "Editor/Styles/EditorStylesheetVariables_Light.json"
#define GLOBAL_STYLE_SHEET_PATH "Editor/Styles/EditorStylesheet.qss"
#define ASSET_PROCESSOR_STYLE_SHEET_PATH "Editor/Styles/AssetProcessor.qss"

using namespace AssetProcessor;

namespace
{
    void RemoveTemporaries()
    {
        // get currently running app
        QString appDir;
        QString file;
        AssetUtilities::ComputeApplicationInformation(appDir, file);
        QDir appDirectory(appDir);
        QString applicationName = appDirectory.filePath(file);
        QFileInfo moduleFileInfo(file);
        QDir binaryDir = moduleFileInfo.absoluteDir();
        // strip extension
        QString applicationBase = file.left(file.indexOf(moduleFileInfo.suffix()) - 1);
        // add wildcard filter
        applicationBase.append("*_tmp");
        // set to qt
        binaryDir.setNameFilters(QStringList() << applicationBase);
        binaryDir.setFilter(QDir::Files);
        // iterate all matching
        foreach(QString tempFile, binaryDir.entryList())
        {
            binaryDir.remove(tempFile);
        }
    }
    QString LoadStyleSheet(QDir rootDir, QString styleSheetPath, bool darkSkin)
    {
        QFile styleSheetVariablesFile;
        if (darkSkin)
        {
            styleSheetVariablesFile.setFileName(rootDir.filePath(STYLE_SHEET_VARIABLES_PATH_DARK));
        }
        else
        {
            styleSheetVariablesFile.setFileName(rootDir.filePath(STYLE_SHEET_VARIABLES_PATH_LIGHT));
        }

        AzQtComponents::StylesheetPreprocessor styleSheetProcessor(nullptr);
        if (styleSheetVariablesFile.open(QFile::ReadOnly))
        {
            styleSheetProcessor.ReadVariables(styleSheetVariablesFile.readAll());
        }

        QFile styleSheetFile(rootDir.filePath(styleSheetPath));
        if (styleSheetFile.open(QFile::ReadOnly))
        {
            return styleSheetProcessor.ProcessStyleSheet(styleSheetFile.readAll());
        }

        return QString();
    }
}


GUIApplicationManager::GUIApplicationManager(int argc, char** argv, QObject* parent)
    : BatchApplicationManager(argc, argv, parent)
{
}


GUIApplicationManager::~GUIApplicationManager()
{
    Destroy();
}



ApplicationManager::BeforeRunStatus GUIApplicationManager::BeforeRun()
{
    ApplicationManager::BeforeRunStatus status = BatchApplicationManager::BeforeRun();
    if (status != ApplicationManager::BeforeRunStatus::Status_Success)
    {
        return status;
    }

    // The build process may leave behind some temporaries, try to delete them
    RemoveTemporaries();

    QDir devRoot;
    AssetUtilities::ComputeEngineRoot(devRoot);

#if defined(EXTERNAL_CRASH_REPORTING)
    InitCrashHandler("AssetProcessor", devRoot.absolutePath().toStdString());
#endif

    AssetProcessor::MessageInfoBus::Handler::BusConnect();
    AssetProcessor::AssetRegistryNotificationBus::Handler::BusConnect();
    AssetUtilities::UpdateBranchToken();

    QString bootstrapPath = devRoot.filePath("bootstrap.cfg");
    m_fileWatcher.addPath(bootstrapPath);

    QObject::connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &GUIApplicationManager::FileChanged);

    return ApplicationManager::BeforeRunStatus::Status_Success;
}

void GUIApplicationManager::Destroy()
{
    AssetProcessor::AssetRegistryNotificationBus::Handler::BusDisconnect();
    AssetProcessor::MessageInfoBus::Handler::BusDisconnect();
    BatchApplicationManager::Destroy();

    DestroyIniConfiguration();
    DestroyFileServer();
    DestroyShaderCompilerManager();
    DestroyShaderCompilerModel();
}


bool GUIApplicationManager::Run()
{
    qRegisterMetaType<AZ::u32>("AZ::u32");
    qRegisterMetaType<AZ::Uuid>("AZ::Uuid");

    QDir systemRoot = GetSystemRoot();

    QDir::addSearchPath("STYLESHEETIMAGES", systemRoot.filePath("Editor/Styles/StyleSheetImages"));

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QString appStyleSheet = LoadStyleSheet(systemRoot, GLOBAL_STYLE_SHEET_PATH, true);
    qApp->setStyleSheet(appStyleSheet);

    m_mainWindow = new MainWindow(this);
    QString windowStyleSheet = LoadStyleSheet(systemRoot, ASSET_PROCESSOR_STYLE_SHEET_PATH, true);
    m_mainWindow->setStyleSheet(windowStyleSheet);

    // CheckForRegistryProblems can pop up a dialog, so we need to check after
    // we initialize the stylesheet
    bool showErrorMessageOnRegistryProblem = true;
    RegistryCheckInstructions registryCheckInstructions = CheckForRegistryProblems(m_mainWindow, showErrorMessageOnRegistryProblem);
    if (registryCheckInstructions != RegistryCheckInstructions::Continue)
    {
        if (registryCheckInstructions == RegistryCheckInstructions::Restart)
        {
            Restart();
        }

        return false;
    }
    
    bool startHidden = QApplication::arguments().contains("--start-hidden", Qt::CaseInsensitive);

    if (!startHidden)
    {
        m_mainWindow->show();
    }
    else
    {
        // Qt / Windows has issues if the main window isn't shown once
        // so we show it then hide it
        m_mainWindow->show();

        // Have a delay on the hide, to make sure that the show is entirely processed
        // first
        QTimer::singleShot(0, m_mainWindow, &QWidget::hide);
    }

    QAction* quitAction = new QAction(QObject::tr("Quit"), m_mainWindow);
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    m_mainWindow->addAction(quitAction);
    m_mainWindow->connect(quitAction, SIGNAL(triggered()), this, SLOT(QuitRequested()));

    QAction* refreshAction = new QAction(QObject::tr("Refresh Stylesheet"), m_mainWindow);
    refreshAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    m_mainWindow->addAction(refreshAction);
    m_mainWindow->connect(refreshAction, &QAction::triggered, [systemRoot, this]()
        {
            QString appStyleSheet = LoadStyleSheet(systemRoot, GLOBAL_STYLE_SHEET_PATH, true);
            qApp->setStyleSheet(appStyleSheet);

            QString windowStyleSheet = LoadStyleSheet(systemRoot, ASSET_PROCESSOR_STYLE_SHEET_PATH, true);
            m_mainWindow->setStyleSheet(windowStyleSheet);
        });

    QObject::connect(this, &GUIApplicationManager::ShowWindow, m_mainWindow, &MainWindow::ShowWindow);


    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        QAction* showAction = new QAction(QObject::tr("Show"), m_mainWindow);
        QObject::connect(showAction, SIGNAL(triggered()), m_mainWindow, SLOT(ShowWindow()));
        QAction* hideAction = new QAction(QObject::tr("Hide"), m_mainWindow);
        QObject::connect(hideAction, SIGNAL(triggered()), m_mainWindow, SLOT(hide()));

        QMenu* trayIconMenu = new QMenu();
        trayIconMenu->addAction(showAction);
        trayIconMenu->addAction(hideAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(quitAction);

        m_trayIcon = new QSystemTrayIcon(m_mainWindow);
        m_trayIcon->setContextMenu(trayIconMenu);
        m_trayIcon->setToolTip(QObject::tr("Asset Processor"));
        m_trayIcon->setIcon(QIcon(":/AssetProcessor.png"));
        m_trayIcon->show();
        QObject::connect(m_trayIcon, &QSystemTrayIcon::activated, m_mainWindow, [&](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::DoubleClick)
                {
                    m_mainWindow->setVisible(!m_mainWindow->isVisible());
                }
            });

        if (startHidden)
        {
            m_trayIcon->showMessage(
                QCoreApplication::translate("Tray Icon", "Lumberyard Asset Processor has started"),
                QCoreApplication::translate("Tray Icon", "The Lumberyard Asset Processor monitors raw project assets and converts those assets into runtime-ready data."),
                QSystemTrayIcon::Information, 3000);
        }
    }

    connect(this, &GUIApplicationManager::AssetProcessorStatusChanged, m_mainWindow, &MainWindow::OnAssetProcessorStatusChanged);

    if (!Activate())
    {
        return false;
    }

    m_mainWindow->Activate();

    connect(GetAssetScanner(), &AssetProcessor::AssetScanner::AssetScanningStatusChanged, this, [this](AssetScanningStatus status)
    {
        if (status == AssetScanningStatus::Started)
        {
            AssetProcessor::AssetProcessorStatusEntry entry(AssetProcessor::AssetProcessorStatus::Scanning_Started);
            m_mainWindow->OnAssetProcessorStatusChanged(entry);
        }
    });
    connect(GetRCController(), &AssetProcessor::RCController::ActiveJobsCountChanged, this, &GUIApplicationManager::OnActiveJobsCountChanged);
    connect(this, &GUIApplicationManager::ConnectionStatusMsg, this, &GUIApplicationManager::ShowTrayIconMessage);

    qApp->setQuitOnLastWindowClosed(false);

    QTimer::singleShot(0, this, [this]()
    {
        if (!PostActivate())
        {
            emit QuitRequested();
            m_startedSuccessfully = false;
        }
    });

    m_duringStartup = false;

    int resultCode =  qApp->exec(); // this blocks until the last window is closed.

    if (m_trayIcon)
    {
        m_trayIcon->hide();
        delete m_trayIcon;
    }

    m_mainWindow->SaveLogPanelState();

    AZ::SerializeContext* context;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialize context");
    QDir projectCacheRoot;
    AssetUtilities::ComputeProjectCacheRoot(projectCacheRoot);
    m_localUserSettings.Save(projectCacheRoot.filePath("AssetProcessorUserSettings.xml").toUtf8().data(), context);
    m_localUserSettings.Deactivate();

    if (NeedRestart())
    {
        bool launched = Restart();
        if (!launched)
        {
            return false;
        }
    }

    delete m_mainWindow;

    Destroy();

    return !resultCode && m_startedSuccessfully;
}

void GUIApplicationManager::NegotiationFailed()
{
    QString message = QCoreApplication::translate("error", "An attempt to connect to the game or editor has failed. The game or editor appears to be running from a different folder. Please restart the asset processor from the correct branch.");
    QMetaObject::invokeMethod(this, "ShowMessageBox", Qt::QueuedConnection, Q_ARG(QString, QString("Negotiation Failed")), Q_ARG(QString, message), Q_ARG(bool, false));
}

void GUIApplicationManager::ProxyConnectFailed()
{
    QString message = QCoreApplication::translate("error", "Proxy Connect Disabled!\n\rPlease make sure that the Proxy IP does not loop back to this same Asset Processor.");
    QMetaObject::invokeMethod(this, "ShowMessageBox", Qt::QueuedConnection, Q_ARG(QString, QString("Proxy Connection Failed")), Q_ARG(QString, message), Q_ARG(bool, false));
}

void GUIApplicationManager::ShowMessageBox(QString title,  QString msg, bool isCritical)
{
    if (!m_messageBoxIsVisible)
    {
        // Only show the message box if it is not visible
        m_messageBoxIsVisible = true;
        QMessageBox msgBox;
        msgBox.setWindowTitle(title);
        msgBox.setText(msg);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        if (isCritical)
        {
            msgBox.setIcon(QMessageBox::Critical);
        }
        msgBox.exec();
        m_messageBoxIsVisible = false;
    }
}

bool GUIApplicationManager::OnError(const char* window, const char* message)
{
    // if we're in a worker thread, errors must not pop up a dialog box
    if (AssetProcessor::GetThreadLocalJobId() != 0)
    {
        // just absorb the error, do not perform default op
        return true;
    }

    // If we're the main thread, then consider showing the message box directly.  
    // note that all other threads will PAUSE if they emit a message while the main thread is showing this box
    // due to the way the trace system EBUS is mutex-protected.
    Qt::ConnectionType connection = Qt::DirectConnection;

    if (QThread::currentThread() != qApp->thread())
    {
        connection = Qt::QueuedConnection;
    }
    QMetaObject::invokeMethod(this, "ShowMessageBox", connection, Q_ARG(QString, QString("Error")), Q_ARG(QString, QString(message)), Q_ARG(bool, true));

    return true;
}

bool GUIApplicationManager::Activate()
{
    AZ::SerializeContext* context;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialize context");
    QDir projectCacheRoot;
    AssetUtilities::ComputeProjectCacheRoot(projectCacheRoot);
    m_localUserSettings.Load(projectCacheRoot.filePath("AssetProcessorUserSettings.xml").toUtf8().data(), context);
    m_localUserSettings.Activate(AZ::UserSettings::CT_LOCAL);
    
    InitIniConfiguration();
    InitFileServer();

    //activate the base stuff.
    if (!BatchApplicationManager::Activate())
    {
        return false;
    }
    
    InitShaderCompilerModel();
    InitShaderCompilerManager();

    return true;
}

bool GUIApplicationManager::PostActivate()
{
    if (!BatchApplicationManager::PostActivate())
    {
        return false;
    }

#if !defined(FORCE_PROXY_MODE)
    GetAssetScanner()->StartScan();
#endif
    return true;
}

void GUIApplicationManager::CreateQtApplication()
{
    // Qt actually modifies the argc and argv, you must pass the real ones in as ref so it can.
    m_qApp = new QApplication(m_argc, m_argv);
}

void GUIApplicationManager::FileChanged(QString path)
{
    QDir devRoot = ApplicationManager::GetSystemRoot();
    QString bootstrapPath = devRoot.filePath("bootstrap.cfg");
    if (QString::compare(AssetUtilities::NormalizeFilePath(path), bootstrapPath, Qt::CaseInsensitive) == 0)
    {
        //Check and update the game token if the bootstrap file get modified
        if (!AssetUtilities::UpdateBranchToken())
        {
            QMetaObject::invokeMethod(this, "FileChanged", Qt::QueuedConnection, Q_ARG(QString, path));
            return; // try again later
        }
        //if the bootstrap file changed,checked whether the game name changed
        QString gameName = AssetUtilities::ReadGameNameFromBootstrap();
        if (!gameName.isEmpty())
        {
            if (QString::compare(gameName, ApplicationManager::GetGameName(), Qt::CaseInsensitive) != 0)
            {
                //The gamename have changed ,should restart the assetProcessor
                QMetaObject::invokeMethod(this, "Restart", Qt::QueuedConnection);
            }

            if (m_connectionManager)
            {
                m_connectionManager->UpdateWhiteListFromBootStrap();
            }
        }
    }
}

void GUIApplicationManager::InitConnectionManager()
{
    BatchApplicationManager::InitConnectionManager();

    using namespace std::placeholders;
    using namespace AzFramework::AssetSystem;
    using namespace AzToolsFramework::AssetSystem;

    m_connectionManager->ReadProxyServerInformation();

    //File Server related
    m_connectionManager->RegisterService(FileOpenRequest::MessageType(), std::bind(&FileServer::ProcessOpenRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileCloseRequest::MessageType(), std::bind(&FileServer::ProcessCloseRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileReadRequest::MessageType(), std::bind(&FileServer::ProcessReadRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileWriteRequest::MessageType(), std::bind(&FileServer::ProcessWriteRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileSeekRequest::MessageType(), std::bind(&FileServer::ProcessSeekRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileTellRequest::MessageType(), std::bind(&FileServer::ProcessTellRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileIsReadOnlyRequest::MessageType(), std::bind(&FileServer::ProcessIsReadOnlyRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathIsDirectoryRequest::MessageType(), std::bind(&FileServer::ProcessIsDirectoryRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileSizeRequest::MessageType(), std::bind(&FileServer::ProcessSizeRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileModTimeRequest::MessageType(), std::bind(&FileServer::ProcessModificationTimeRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileExistsRequest::MessageType(), std::bind(&FileServer::ProcessExistsRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileFlushRequest::MessageType(), std::bind(&FileServer::ProcessFlushRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathCreateRequest::MessageType(), std::bind(&FileServer::ProcessCreatePathRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(PathDestroyRequest::MessageType(), std::bind(&FileServer::ProcessDestroyPathRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileRemoveRequest::MessageType(), std::bind(&FileServer::ProcessRemoveRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileCopyRequest::MessageType(), std::bind(&FileServer::ProcessCopyRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FileRenameRequest::MessageType(), std::bind(&FileServer::ProcessRenameRequest, m_fileServer, _1, _2, _3, _4));
    m_connectionManager->RegisterService(FindFilesRequest::MessageType(), std::bind(&FileServer::ProcessFindFileNamesRequest, m_fileServer, _1, _2, _3, _4));

    QObject::connect(m_connectionManager, SIGNAL(connectionAdded(uint, Connection*)), m_fileServer, SLOT(ConnectionAdded(unsigned int, Connection*)));
    QObject::connect(m_connectionManager, SIGNAL(ConnectionDisconnected(unsigned int)), m_fileServer, SLOT(ConnectionRemoved(unsigned int)));
    QObject::connect(m_iniConfiguration, &IniConfiguration::ProxyInfoChanged, m_connectionManager, &ConnectionManager::SetProxyInformation);

    QObject::connect(m_fileServer, SIGNAL(AddBytesReceived(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesReceived(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesSent(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesSent(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesRead(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesRead(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddBytesWritten(unsigned int, qint64, bool)), m_connectionManager, SLOT(AddBytesWritten(unsigned int, qint64, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddOpenRequest(unsigned int, bool)), m_connectionManager, SLOT(AddOpenRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCloseRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCloseRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddOpened(unsigned int, bool)), m_connectionManager, SLOT(AddOpened(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddClosed(unsigned int, bool)), m_connectionManager, SLOT(AddClosed(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddReadRequest(unsigned int, bool)), m_connectionManager, SLOT(AddReadRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddWriteRequest(unsigned int, bool)), m_connectionManager, SLOT(AddWriteRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddTellRequest(unsigned int, bool)), m_connectionManager, SLOT(AddTellRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddSeekRequest(unsigned int, bool)), m_connectionManager, SLOT(AddSeekRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddIsReadOnlyRequest(unsigned int, bool)), m_connectionManager, SLOT(AddIsReadOnlyRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddIsDirectoryRequest(unsigned int, bool)), m_connectionManager, SLOT(AddIsDirectoryRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddSizeRequest(unsigned int, bool)), m_connectionManager, SLOT(AddSizeRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddModificationTimeRequest(unsigned int, bool)), m_connectionManager, SLOT(AddModificationTimeRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddExistsRequest(unsigned int, bool)), m_connectionManager, SLOT(AddExistsRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddFlushRequest(unsigned int, bool)), m_connectionManager, SLOT(AddFlushRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCreatePathRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCreatePathRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddDestroyPathRequest(unsigned int, bool)), m_connectionManager, SLOT(AddDestroyPathRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddRemoveRequest(unsigned int, bool)), m_connectionManager, SLOT(AddRemoveRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddCopyRequest(unsigned int, bool)), m_connectionManager, SLOT(AddCopyRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddRenameRequest(unsigned int, bool)), m_connectionManager, SLOT(AddRenameRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(AddFindFileNamesRequest(unsigned int, bool)), m_connectionManager, SLOT(AddFindFileNamesRequest(unsigned int, bool)));
    QObject::connect(m_fileServer, SIGNAL(UpdateConnectionMetrics()), m_connectionManager, SLOT(UpdateConnectionMetrics()));
    
    m_connectionManager->RegisterService(ShowAssetProcessorRequest::MessageType(),
        std::bind([this](unsigned int /*connId*/, unsigned int /*type*/, unsigned int /*serial*/, QByteArray /*payload*/)
    {
        Q_EMIT ShowWindow();
    }, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
    );
}

void GUIApplicationManager::InitIniConfiguration()
{
    m_iniConfiguration = new IniConfiguration();
    m_iniConfiguration->readINIConfigFile();
    m_iniConfiguration->parseCommandLine();
}

void GUIApplicationManager::DestroyIniConfiguration()
{
    if (m_iniConfiguration)
    {
        delete m_iniConfiguration;
        m_iniConfiguration = nullptr;
    }
}

void GUIApplicationManager::InitFileServer()
{
    m_fileServer = new FileServer();
    m_fileServer->SetSystemRoot(GetSystemRoot());
}

void GUIApplicationManager::DestroyFileServer()
{
    if (m_fileServer)
    {
        delete m_fileServer;
        m_fileServer = nullptr;
    }
}

void GUIApplicationManager::InitShaderCompilerManager()
{
    m_shaderCompilerManager = new ShaderCompilerManager();
    
    //Shader compiler stuff
    m_connectionManager->RegisterService(AssetUtilities::ComputeCRC32Lowercase("ShaderCompilerProxyRequest"), std::bind(&ShaderCompilerManager::process, m_shaderCompilerManager, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    QObject::connect(m_shaderCompilerManager, SIGNAL(sendErrorMessageFromShaderJob(QString, QString, QString, QString)), m_shaderCompilerModel, SLOT(addShaderErrorInfoEntry(QString, QString, QString, QString)));

    
}

void GUIApplicationManager::DestroyShaderCompilerManager()
{
    if (m_shaderCompilerManager)
    {
        delete m_shaderCompilerManager;
        m_shaderCompilerManager = nullptr;
    }
}

void GUIApplicationManager::InitShaderCompilerModel()
{
    m_shaderCompilerModel = new ShaderCompilerModel();
}

void GUIApplicationManager::DestroyShaderCompilerModel()
{
    if (m_shaderCompilerModel)
    {
        delete m_shaderCompilerModel;
        m_shaderCompilerModel = nullptr;
    }
}

IniConfiguration* GUIApplicationManager::GetIniConfiguration() const
{
    return m_iniConfiguration;
}

FileServer* GUIApplicationManager::GetFileServer() const
{
    return m_fileServer;
}
ShaderCompilerManager* GUIApplicationManager::GetShaderCompilerManager() const
{
    return m_shaderCompilerManager;
}
ShaderCompilerModel* GUIApplicationManager::GetShaderCompilerModel() const
{
    return m_shaderCompilerModel;
}

void GUIApplicationManager::ShowTrayIconMessage(QString msg)
{
    if (m_trayIcon && m_mainWindow && !m_mainWindow->isVisible())
    {
        m_trayIcon->showMessage(
            QCoreApplication::translate("Tray Icon", "Lumberyard Asset Processor"),
            QCoreApplication::translate("Tray Icon", msg.toUtf8().data()),
            QSystemTrayIcon::Information, 3000);
    }
}

bool GUIApplicationManager::Restart()
{
    bool launched = QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments());
    if (!launched)
    {
        QMessageBox::critical(nullptr,
            QCoreApplication::translate("application", "Unable to launch Asset Processor"),
            QCoreApplication::translate("application", "Unable to launch Asset Processor"));
    }

    return launched;
}

#include <native/utilities/GUIApplicationManager.moc>
