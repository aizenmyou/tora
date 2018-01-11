
/* BEGIN_COMMON_COPYRIGHT_HEADER
 *
 * TOra - An Oracle Toolkit for DBA's and developers
 *
 * Shared/mixed copyright is held throughout files in this product
 *
 * Portions Copyright (C) 2000-2001 Underscore AB
 * Portions Copyright (C) 2003-2005 Quest Software, Inc.
 * Portions Copyright (C) 2004-2013 Numerous Other Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation;  only version 2 of
 * the License is valid for this program.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program as the file COPYING.txt; if not, please see
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *      As a special exception, you have permission to link this program
 *      with the Oracle Client libraries and distribute executables, as long
 *      as you follow the requirements of the GNU GPL in regard to all of the
 *      software in the executable aside from Oracle client libraries.
 *
 * All trademarks belong to their respective owners.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "main/tomain.h"
#include "main/topreferences.h"
#include "widgets/tobackgroundlabel.h"
#include "core/toeditmenu.h"
#include "core/tofilemenu.h"
#include "widgets/toworkspace.h"
#include "core/toraversion.h"
#include "core/toconnectionprovider.h"
#include "core/toconnectionregistry.h"
#include "core/toconnectionoptions.h"
#include "widgets/todockbar.h"
#include "editor/tomemoeditor.h"
#include "widgets/toabout.h"
#include "tools/toworksheet.h"
#include "tools/tobrowser.h"
#include "core/toconfiguration.h"
#include "core/toglobalevent.h"
#include "core/toglobalconfiguration.h"
#include "core/utils.h"
#include "core/tologger.h"
#include "core/toconf.h"
#include "core/toupdater.h"
#include "ts_log/toostream.h"
#include "editor/tosqltext.h"
#include "editor/toworksheettext.h"
#include "widgets/tosearch.h"

#include "icons/tora.xpm"
#include "icons/up.xpm"

#include <QComboBox>
#include <QStatusBar>
#include <QMenuBar>
#include "widgets/tohelp.h"
#include "tomessage.h"
#include "tonewconnection.h"

toMain::toMain()
    : toMainWindow()
    , Workspace(toWorkSpaceSingle::Instance())
    , Connections(toConnectionRegistrySing::Instance())
    , fileMenu(toFileMenuSingle::Instance())
    , editMenu(toEditMenuSingle::Instance())      
    , Poll()
    , BackgroundLabel(new toBackgroundLabel(statusBar()))
    , loggingWidget(toLoggingWidgetSingle::Instance())
    , lastToolWidget(NULL)
{
    loggingWidget.setMaximumBlockCount(2000);
    loggingWidget.setCenterOnScroll(true);
    loggingWidget.setReadOnly(true);

    Workspace.setParent(this);
    setCentralWidget(&Workspace);

    Message = new toMessage(this);

    Poll.setBackgroundLabel(BackgroundLabel);

    // setup all QAction objects
    createActions();

    // create all menus
    createMenus();

    createToolbars();

    createStatusbar();

    createDocklets();

    setWindowTitle(TOAPPNAME " " TORAVERSION);
    setWindowIcon(QPixmap(const_cast<const char**>(tora_xpm)));

    restoreGeometry(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::MainWindowGeometry).toByteArray());
    restoreState(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::MainWindowState).toByteArray());

    //enableConnectionActions(false);

    QString defName(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::DefaultTool).toString());
    for (ToolsRegistrySing::ObjectType::iterator k = ToolsRegistrySing::Instance().begin();
            k != ToolsRegistrySing::Instance().end();
            ++k)
    {
        if (defName.isEmpty())
        {
            toConfigurationNewSingle::Instance().setOption(ToConfiguration::Main::DefaultTool, QVariant(k.key()));
            defName = k.key();
        }
        k.value()->customSetup();
    }

    connect(&Poll, SIGNAL(timeout()), this, SLOT(checkCaching()));

    // Connect this "main" window to global events dispatcher
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_addCustomMenu(QMenu*)),
            this, SLOT(addCustomMenu(QMenu*)));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_setCoordinates(int,int)),
            this, SLOT(setCoordinates(int, int)));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_createDefaultTool(void)),
            this, SLOT(createDefault()));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_setNeedCommit(toToolWidget*, bool)),
            this, SLOT(setNeedCommit(toToolWidget*, bool)));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_checkCaching()),
            this, SLOT(checkCaching()));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_showMessage(QString, bool, bool)),
            this, SLOT(showMessageImpl(QString, bool, bool)), Qt::QueuedConnection);
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_editOpenFile(QString const&)),
            this, SLOT(editOpenFile(QString const&)));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_openPreferences()),
            this, SLOT(openPreferences()));
    connect(&toWorkSpaceSingle::Instance(), SIGNAL(activeToolChaged(toToolWidget*)),
            this, SLOT(slotActiveToolChaged(toToolWidget*)));

#ifdef TORA3_SESSION
    if (toConfigurationNewSingle::Instance().restoreSession())
    {
        try
        {
            std::map<QString, QString> session;
            toConfigurationNewSingle::Instance().loadMap(
                toConfigurationNewSingle::Instance().defaultSession(), session);
            importData(session, "TOra");
        }
        TOCATCH;
    }
#endif

    show();

    createDockbars();           // keep after restoreState() and show()

    statusBar()->addPermanentWidget(BackgroundLabel, 0);
    BackgroundLabel->show();
    BackgroundLabel->setToolTip(tr("No background queries."));

    // List of all connection provider finders
    std::vector<std::string> finders = ConnectionProviderFinderFactory::Instance().keys();
    // Resulting list of all the providers found
    toProvidersList &allProviders = toProvidersListSing::Instance(); // already populated in main.cpp see splash
    Q_UNUSED(allProviders);

#ifdef QT_DEBUG
    reportTimer = new QTimer(this);
    reportTimer->setInterval(5000);
    connect(reportTimer, SIGNAL(timeout ()), this, SLOT(reportFocus()));
    reportTimer->start();
#endif

    if (Connections.isEmpty())
    {
        addConnection();
    }
}

void toMain::createActions()
{
    // ---------------------------------------- file menu - has it's own singleton class

    // ---------------------------------------- edit menu - has it's own singleton class

    // ---------------------------------------- help menu

    helpCurrentAct = new QAction(tr("C&urrent Context..."), this);
    helpCurrentAct->setShortcut(QKeySequence::HelpContents);

    helpContentsAct = new QAction(tr("&Contents..."), this);

    aboutAct = new QAction(tr("&About " TOAPPNAME "..."), this);

    aboutQtAct = new QAction(tr("About &Qt..."), this);

    // ---------------------------------------- windows menu
    windowCloseAct = new QAction(tr("C&lose"), this);
    windowCloseAllAct = new QAction(tr("Close &All"), this);

    toUpdaterSingle::Instance().check(/*force=>*/false);
}


void toMain::createMenus()
{
    menuBar()->addMenu(&fileMenu);
    connect(fileMenu.newConnAct, SIGNAL(triggered()), this, SLOT(addConnection()));
    connect(fileMenu.closeConnAct, SIGNAL(triggered()), this, SLOT(delCurrentConnection()));

    connect(&fileMenu,
            SIGNAL(triggered(QAction *)),
            this,
            SLOT(commandCallback(QAction *)));

    connect(fileMenu.recentMenu,
            SIGNAL(triggered(QAction *)),
            this,
            SLOT(recentCallback(QAction *)));

    menuBar()->addMenu(&editMenu);

    // Use only when there are any docklets registered
    if (toDocklet::docklets().count())
    {
        viewMenu = menuBar()->addMenu(tr("&View"));
        foreach(toDocklet * let, toDocklet::docklets())
        {
            viewMenu->addAction(new QAction(let->icon(),
                                            let->name(),
                                            0));
        }

        connect(viewMenu,
                SIGNAL(triggered(QAction *)),
                this,
                SLOT(viewCallback(QAction *)));
    }

    toolsMenu = menuBar()->addMenu(tr("&Tools"));
    connect(toolsMenu,
            SIGNAL(triggered(QAction *)),
            this,
            SLOT(commandCallback(QAction *)));
    ToolsRegistrySing::Instance().toolsMenu(toolsMenu);

    // windows menu handled separately by update function
    windowsMenu = menuBar()->addMenu(tr("&Window"));
    connect(windowsMenu, SIGNAL(aboutToShow()), this, SLOT(updateWindowsMenu()));

    // Refresh QAction shortcuts when tools are added/removed
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_toolWidgetAdded(toToolWidget*)),
            this, SLOT(updateWindowsMenu()));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_toolWidgetRemoved(toToolWidget*)),
            this, SLOT(updateWindowsMenu()));
    connect(&toGlobalEventSingle::Instance(), SIGNAL(s_toolWidgetsReordered()),
            this, SLOT(updateWindowsMenu()));

    connect(windowsMenu,
            SIGNAL(triggered(QAction *)),
            this,
            SLOT(windowCallback(QAction *)));

    connectionsMenu = menuBar()->addMenu(tr("&Connection"));
    connect(connectionsMenu, SIGNAL(aboutToShow()), this, SLOT(updateConnectionsMenu()));

    helpMenu = menuBar()->addMenu(tr("&Help"));

    helpMenu->addAction(helpCurrentAct);
    helpMenu->addAction(helpContentsAct);
	helpMenu->addSeparator();
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);

    connect(helpMenu,
            SIGNAL(triggered(QAction *)),
            this,
            SLOT(commandCallback(QAction *)));
}


void toMain::addCustomMenu(QMenu *menu)
{
    this->menuBar()->insertMenu(windowsMenu->menuAction(), menu);
}

void toMain::createToolbars()
{
    editToolbar = Utils::toAllocBar(this, tr("Application"));
    editToolbar->setObjectName("editToolbar");

    editToolbar->addAction(fileMenu.openAct);
    editToolbar->addAction(fileMenu.saveAct);
    editToolbar->addSeparator();

    editToolbar->addAction(editMenu.undoAct);
    editToolbar->addAction(editMenu.redoAct);
    editToolbar->addAction(editMenu.cutAct);
    editToolbar->addAction(editMenu.copyAct);
    editToolbar->addAction(editMenu.pasteAct);
    editToolbar->addSeparator();

    editToolbar->addAction(editMenu.searchReplaceAct);

    connectionToolbar = Utils::toAllocBar(this, tr("Connections"));
    connectionToolbar->setObjectName("connectionToolbar");

    connectionToolbar->addAction(fileMenu.newConnAct);
    connectionToolbar->addAction(fileMenu.closeConnAct);
    connectionToolbar->addAction(fileMenu.commitAct);
    connectionToolbar->addAction(fileMenu.rollbackAct);
    connectionToolbar->addSeparator();

    connectionToolbar->addAction(fileMenu.stopAct);
    connectionToolbar->addSeparator();

    ConnectionSelection = new QComboBox(connectionToolbar);
    ConnectionSelection->setMinimumWidth(300);
    ConnectionSelection->setFocusPolicy(Qt::NoFocus);
    connectionToolbar->addWidget(ConnectionSelection);
    ConnectionSelection->setModel(&toConnectionRegistrySing::Instance());
    connect(ConnectionSelection, SIGNAL(currentIndexChanged(int)), &toConnectionRegistrySing::Instance(), SLOT(slotViewIndexChanged(int)));
    connect(&toConnectionRegistrySing::Instance(), SIGNAL(activeConnectionChanged(int)), ConnectionSelection, SLOT(setCurrentIndex(int)));
    connect(&toConnectionRegistrySing::Instance(), SIGNAL(activeConnectionChanged(QModelIndex)), this, SLOT(connectionSelectionChanged()));

    addToolBarBreak();

    toolsToolbar = Utils::toAllocBar(this, tr("Tools"));
    toolsToolbar->setObjectName("toolsToolbar");
    ToolsRegistrySing::Instance().toolsToolbar(toolsToolbar);
}

void toMain::createStatusbar()
{
    statusBar()->showMessage(QString::null);

#if 0
// TODO: this part is waiting for QScintilla backend feature (yet unimplemented).
    SelectionLabel = new QLabel(statusBar());
    statusBar()->addPermanentWidget(SelectionLabel);
    SelectionLabel->setMinimumWidth(90);
    SelectionLabel->setText("Sel: Normal");
#endif

    toEditorTypeButtonSingle::Instance().setDisabled(true);
    statusBar()->addPermanentWidget(&toEditorTypeButtonSingle::Instance());

    toHighlighterTypeButtonSingle::Instance().setDisabled(true);
    statusBar()->addPermanentWidget(&toHighlighterTypeButtonSingle::Instance());

    RowLabel = new QLabel(statusBar());
    statusBar()->addPermanentWidget(RowLabel);
    RowLabel->setMinimumWidth(60);

    ColumnLabel = new QLabel(statusBar());
    statusBar()->addPermanentWidget(ColumnLabel);
    ColumnLabel->setMinimumWidth(60);

    QToolButton *dispStatus = new toPopupButton(statusBar(), "dispStatus");
    dispStatus->setIcon(QPixmap(const_cast<const char**>(up_xpm)));
    statusBar()->addPermanentWidget(dispStatus, 0);
    statusMenu = new QMenu(dispStatus);
    dispStatus->setMenu(statusMenu);
    dispStatus->setPopupMode(QToolButton::MenuButtonPopup);
    connect(statusMenu,
            SIGNAL(aboutToShow()),
            this,
            SLOT(updateStatusMenu()));
    connect(statusMenu,
            SIGNAL(triggered(QAction*)),
            this,
            SLOT(statusCallback(QAction*)));
    connect(dispStatus,
            SIGNAL(pressed()),
            dispStatus,
            SLOT(showMenu()));
}


void toMain::createDocklets()
{
    foreach(toDocklet * let, toDocklet::docklets())
    addDockWidget(Qt::LeftDockWidgetArea, let);
}


// must call this after restoreState()

void toMain::createDockbars()
{
    leftDockbar = new toDockbar(Qt::LeftToolBarArea,
                                tr("Left Dockbar"),
                                this);
    addToolBar(Qt::LeftToolBarArea, leftDockbar);
    leftDockbar->hide();

    rightDockbar = new toDockbar(Qt::RightToolBarArea,
                                 tr("Right Dockbar"),
                                 this);
    addToolBar(Qt::RightToolBarArea, rightDockbar);
    rightDockbar->hide();

    // toDockbar keeps it's own settings, but just in case something
    // goes wrong, or a new setup, add any visible docklets to the
    // dockbar.

    foreach(toDocklet * let, toDocklet::docklets())
    {
        if (let->isVisible())
            moveDocklet(let, dockWidgetArea(let));

        connect(let,
                SIGNAL(dockletLocationChanged(toDocklet *, Qt::DockWidgetArea)),
                this,
                SLOT(moveDocklet(toDocklet *, Qt::DockWidgetArea)));
    }

    leftDockbar->restoreState(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::LeftDockbarState).toByteArray());
    rightDockbar->restoreState(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::RightDockbarState).toByteArray());
}

void toMain::updateWindowsMenu(void)
{
    // i'm lazy and this beats the hell out of tracking all the
    // windowsMenu actions and adding/removing each.
    windowsMenu->clear();
    QList<toToolWidget*> tools = toWorkSpaceSingle::Instance().toolWindowList();
    windowCloseAct->setDisabled(tools.empty());
    windowCloseAllAct->setDisabled(tools.empty());

    windowsMenu->addAction(windowCloseAct);
    windowsMenu->addAction(windowCloseAllAct);
    windowsMenu->addSeparator();

    toToolWidget *currentTool = toWorkSpaceSingle::Instance().currentTool();
    int index = 0;
    Q_FOREACH(toToolWidget *tool, tools)
    {
        QAction *action = tool->activationAction();
        windowsMenu->addAction(action);
        action->setChecked(tool == currentTool);
        if (index < 9)
        {
            action->setText( QString("&") + QString::number(index + 1) + QString(" ") + tool->windowTitle());
            //caption = "&" + QString::number(index + 1) + "  " + caption;
            action->setShortcut(Qt::CTRL + Qt::Key_1 + index++);
        }
    }
}

void toMain::updateConnectionsMenu(void)
{
    try
    {
        toConnection &conn = toConnectionRegistrySing::Instance().currentConnection();
        conn.connectionsMenu(connectionsMenu);
    }
    catch (...)
    {

    }
}


void toMain::windowCallback(QAction *action)
{
    // action's parent is the window widget. get parent and raise it.
    if (action == NULL || action->parentWidget() == NULL)
        return;

    if (action == windowCloseAllAct)
    {
        toWorkSpaceSingle::Instance().closeAllToolWidgets();
    }
    else if (action == windowCloseAct)
    {
        toToolWidget *currentTool = toWorkSpaceSingle::Instance().currentTool();
        toWorkSpaceSingle::Instance().closeToolWidget(currentTool);
    }
    else
    {
        toToolWidget *requestedTool = dynamic_cast<toToolWidget*>(action->parent());
        Q_ASSERT_X(requestedTool, qPrintable(__QHERE__), "QAction - invalid parent");
        toWorkSpaceSingle::Instance().setCurrentTool(requestedTool);
    }
}


void toMain::recentCallback(QAction *action)
{
    if (!action)
        return;

    toEditWidget *edit = NULL;
    QWidget *currWidget = qApp->focusWidget();
    while (currWidget && !edit)
    {
        edit = dynamic_cast<toEditWidget *>(currWidget);
        currWidget = currWidget->parentWidget();
    }

    if (edit)
        edit->editOpen(action->toolTip());
    else
        this->editOpenFile(action->toolTip());
}

void toMain::statusCallback(QAction *action)
{
    new toMemoEditor(this, action->toolTip());
}

void toMain::viewCallback(QAction *action)
{
    toDocklet *let = toDocklet::docklet(action->text());
    if (!let)
        return;

    let->close();
    if (leftDockbar->contains(let))
    {
        leftDockbar->removeDocklet(let);
        return;
    }
    if (rightDockbar->contains(let))
    {
        rightDockbar->removeDocklet(let);
        return;
    }

    addDockWidget(Qt::LeftDockWidgetArea, let);
#if QT_VERSION >= 0x040400
    restoreDockWidget(let);
#else
    let->show();
#endif
}


void toMain::moveDocklet(toDocklet *let, Qt::DockWidgetArea area)
{
    if (area == Qt::LeftDockWidgetArea)
    {
        rightDockbar->removeDocklet(let);
        leftDockbar->addDocklet(let);
    }

    if (area == Qt::RightDockWidgetArea)
    {
        leftDockbar->removeDocklet(let);
        rightDockbar->addDocklet(let);
    }
}


void toMain::commandCallback(QAction *action)
{
    QWidget *focus = qApp->focusWidget();

    toEditWidget::FlagSetStruct editFlags;
    toEditWidget *edit = toEditWidget::findEdit(focus);

    if (action == fileMenu.openAct && !this->Connections.isEmpty())
    {
        if (edit && editFlags.Open)
            edit->editOpen();
        else
            this->editOpenFile(QString::null);
    }
    if (action == fileMenu.commitAct)
    {
        try
        {
            toWorksheet *w = dynamic_cast<toWorksheet*>(lastToolWidget);
            toBrowser *b = dynamic_cast<toBrowser*>(lastToolWidget);
            Q_ASSERT_X(w || b, qPrintable(__QHERE__), "Commit on wrong tool");
            if (w)
                w->commitChanges();
            //else
            //	b->commitChanges();
        }
        TOCATCH;
    }
    else if (action == fileMenu.rollbackAct)
    {
        try
        {
            toWorksheet *w = dynamic_cast<toWorksheet*>(lastToolWidget);
            Q_ASSERT_X(w, qPrintable(__QHERE__), "Rollback on wrong tool");
            w->rollbackChanges();
        }
        TOCATCH;
    }
    else if (action == fileMenu.stopAct)
    {
        try
        {
            toConnection &conn = toConnectionRegistrySing::Instance().currentConnection();
            conn.cancelAll();
            // Change the current override cursor back to a normal
            // cursor. This has no effect if there's no current
            // override, so this should not corrupt the cursor stack
            // in qApplication.
            qApp->changeOverrideCursor(Qt::ArrowCursor);
        }
        TOCATCH;
    }
    else if (action == fileMenu.refreshAct)
    {
        try
        {
            toConnectionRegistrySing::Instance().currentConnection().getCache().rereadCache();
        }
        TOCATCH;
        checkCaching();
    }
    else if (action == fileMenu.currentAct)
        ConnectionSelection->setFocus();
    else if (action == fileMenu.quitAct)
        close();
    else if (action == helpCurrentAct)
        toHelp::displayHelp();
    else if (action == helpContentsAct)
        toHelp::displayHelp(QString::fromLatin1("toc.html"));
    else if (action == aboutAct)
    {
        toAbout about(this, "About " TOAPPNAME, true);
        about.exec();
    }
    else if (action == aboutQtAct)
        QApplication::aboutQt();
#ifdef TORA3_SESSION
    else if (action == openSessionAct)
        loadSession();
    else if (action == saveSessionAct)
        saveSession();
    else if (action == restoreSessionAct)
    {
        try
        {
            std::map<QString, QString> session;
            toConfigurationNewSingle::Instance().loadMap(
                toConfigurationNewSingle::Instance().defaultSession(), session);
            importData(session, "TOra");
        }
        TOCATCH;
    }
    else if (action == closeSessionAct)
        closeSession();
#endif
}

void toMain::addConnection(void)
{
    try
    {
        toNewConnection newConnection(this);

        toConnection *conn = NULL;

        if (newConnection.exec())
            conn = newConnection.connection();

        if (conn)
        {
            Connections.addConnection(conn);
            // New connection was added - create a default tool for it
            createDefault();
        }
    }
    TOCATCH
}

void toMain::setNeedCommit(toToolWidget *tool, bool needCommit)
{
    if (tool == NULL)
    {
        fileMenu.commitAct->setDisabled(true);
        fileMenu.rollbackAct->setDisabled(true);
        fileMenu.stopAct->setDisabled(true);
        return;
    }

    toConnection const& conn = tool->connection();
    int pos = ConnectionSelection->currentIndex();

#pragma message WARN("Set need commit on connection here")
    QString dsc = conn.description();
    if (needCommit)
        dsc += QString::fromLatin1(" *");
    ConnectionSelection->setCurrentIndex(pos);
    ConnectionSelection->setItemText(pos, dsc);

    fileMenu.commitAct->setEnabled(needCommit);
    fileMenu.rollbackAct->setEnabled(needCommit);
}

bool toMain::delCurrentConnection(void)
{
    toConnection &conn = Connections.currentConnection();

    if (!conn.closeWidgets())
        return false;

    Connections.removeConnection(&conn);
    return true;
}

void toMain::closeEvent(QCloseEvent *event)
{
    toWorkSpaceSingle::Instance().closeAllToolWidgets();
    if ( toWorkSpaceSingle::Instance().currentTool() != NULL) // at least one tool window refused to be closed
    {
        event->ignore();        // stop widget refused
        return;
    }

    while (!Connections.isEmpty())
    {
        if (!delCurrentConnection())
        {
            event->ignore();
            return;
        }
    }

#ifdef TORA3_SESSION
    std::map<QString, QString> session;
    exportData(session, "TOra");
    try
    {
        toConfigurationNewSingle::Instance().saveMap(
            toConfigurationNewSingle::Instance().defaultSession(),
            session);
    }
    TOCATCH;
#endif
    toConfigurationNewSingle::Instance().setOption(ToConfiguration::Main::MainWindowGeometry, QVariant(saveGeometry()));
    toConfigurationNewSingle::Instance().setOption(ToConfiguration::Main::MainWindowState, QVariant(saveState()));

    toConfigurationNewSingle::Instance().setOption(ToConfiguration::Main::LeftDockbarState, QVariant(leftDockbar->saveState()));
    toConfigurationNewSingle::Instance().setOption(ToConfiguration::Main::RightDockbarState, QVariant(rightDockbar->saveState()));

    toConfigurationNewSingle::Instance().saveAll();
    event->accept();
}

void toMain::createDefault(void)
{
    QString defName(toConfigurationNewSingle::Instance().option(ToConfiguration::Main::DefaultTool).toString());
    toTool *DefaultTool = NULL;

    for (ToolsRegistrySing::ObjectType::iterator i = ToolsRegistrySing::Instance().begin();
            i != ToolsRegistrySing::Instance().end();
            ++i)
    {
        if (defName.isEmpty() || defName == i.key())
        {
            DefaultTool = i.value();
            break;
        }
    }

    if (DefaultTool)
        DefaultTool->createWindow();
}

void toMain::setCoordinates(int line, int col)
{
    QString str = tr("Row:") + " ";
    str += QString::number(line);
    RowLabel->setText(str);
    str = tr("Col:") + " ";
    str += QString::number(col);
    ColumnLabel->setText(str);
}

//void toMain::editSQL(const QString &str)
//{
//    if (!SQLEditor.isNull() && ToolsRegistrySing::Instance().contains(SQLEditor))
//    {
//        ToolsRegistrySing::Instance().value(SQLEditor)->createWindow();
//        emit sqlEditor(str);
//    }
//}

void toMain::updateStatusMenu(void)
{
    statusMenu->clear();
    foreach(QString const& message, StatusMessages)
    {
        QAction *s = new QAction(statusMenu);
        if (message.length() > 75)
            s->setText(message.left(75) + "...");
        else
            s->setText(message);
        s->setToolTip(message.left(75));
        statusMenu->addAction(s);
    }
}

void toMain::connectionSelectionChanged(void)
{
    // Handle the situation when there are no connections open
    if (toConnectionRegistrySing::Instance().isEmpty())
    {
        for (ToolsRegistrySing::ObjectType::iterator i = ToolsRegistrySing::Instance().begin(); i != ToolsRegistrySing::Instance().end(); ++i)
        {
            (*i)->enableAction(false);
        }
        fileMenu.closeConnAct->setDisabled(true);
        return;
    }

    fileMenu.closeConnAct->setEnabled(true);

    toConnection const& conn = toConnectionRegistrySing::Instance().currentConnection();
    for (ToolsRegistrySing::ObjectType::iterator i = ToolsRegistrySing::Instance().begin(); i != ToolsRegistrySing::Instance().end(); ++i)
    {
        (*i)->enableAction(conn);
    }
}

void toMain::editOpenFile(const QString &file)
{
    toWorksheet *sheet = 0;
//    toEditWidget *Edit = toEditMenuSingle::Instance().editWidget();
//    if(Edit)
//        sheet = dynamic_cast<toWorksheet *>(Edit);

    if (!sheet)
    {
        toTool *pTool = ToolsRegistrySing::Instance().value("00010SQL Editor");
        if (pTool)
        {
            QWidget *win = pTool->createWindow();
            if (win)
                sheet = dynamic_cast<toWorksheet *>(win);
        }
        else
            printf("Couldn't find sql worksheet.\n");
    }

    if (!sheet)
        return;

    sheet->editor()->editOpen(file);
    sheet->setFocus();
}

void toMain::openPreferences()
{
    toPreferences::displayPreferences(this);
}

toDockbar* toMain::dockbar(toDocklet *let)
{
    if (rightDockbar->contains(let))
        return rightDockbar;
    return leftDockbar;
}

void toMain::showMessageImpl(QString str, bool save, bool log)
{
    using namespace ToConfiguration;
    if (!str.isEmpty())
    {
        int sec = toConfigurationNewSingle::Instance().option(Global::StatusMessageInt).toInt();
        if (save || sec == 0)
            statusBar()->showMessage(str.simplified());
        else
            statusBar()->showMessage(str.simplified(), sec * 1000);

        if (log)
        {
            StatusMessages.append(str);
            int HistorySize = toConfigurationNewSingle::Instance().option(Global::HistorySizeInt).toInt();
            if (StatusMessages.size() > HistorySize)
                StatusMessages.takeFirst();
            if (!toConfigurationNewSingle::Instance().option(Global::MessageStatusbarBool).toBool())
                displayMessage();
        }

        if (!save)
        {
            statusBar()->setToolTip(str);
        }
    }
}

void toMain::slotActiveToolChaged(toToolWidget *tool)
{
    // NOTE: a call to hasTransaction gets blocked until bg query finishes
    // TODO: implement non-blocking version of hasTransaction
    //setNeedCommit(tool, tool ? tool->hasTransaction() : false);
    lastToolWidget = tool;
}

void toMain::newVersionAvalable()
{

}

#ifdef QT_DEBUG
void toMain::reportFocus()
{
    QWidget *focus = qApp->focusWidget();
    TLOG(9, toDecorator, __HERE__) << (focus ? focus->metaObject()->className() : QString("NULL"))
                                   << '(' << (focus ? focus->objectName() : QString("NULL")) << ')'
                                   << std::endl;
}
#endif

void toMain::checkCaching(void)
{
    int num = 0;
    foreach(toConnection * conn, Connections.connections())
    {
        if (conn->getCache().cacheRefreshRunning())
            num++;
    }
    if (num == 0)
    {
        Poll.stop();
    }
    else
    {
        Poll.start(100);
    }
}

#ifdef TORA3_SESSION
void toMain::exportData(std::map<QString, QString> &data, const QString &prefix)
{
    try
    {

        int id = 1;
        std::map<toConnection *, int> connMap;
        {
            foreach(toConnection * i, Connections)
            {
                QString key = prefix + ":Connection:" + QString::number(id);
                if (toConfigurationNewSingle::Instance().savePassword())
                    data[key + ":Password"] = toObfuscate(i->password());
                data[key + ":User"] = i->user();
                data[key + ":Host"] = i->host();

                QString options;
                foreach (QString const & o, i->options())
                {
                    options += "," + o;
                }
                data[key + ":Options"] = options.mid(1);
                Strip extra , in beginning

                data[key + ":Database"] = (*i)->database();
                data[key + ":Provider"] = (*i)->provider();
                connMap[i] = id;
                id++;
            }
        }
        id = 1;
        for (int i = 0; i < workspace()->subWindowList().count(); i++)
        {
            toToolWidget *tool = dynamic_cast<toToolWidget *>(workspace()->subWindowList().at(i));

            if (tool)
            {
                QString key = prefix + ":Tools:" + QString::number(id);
                tool->exportData(data, key);
                data[key + ":Type"] = tool->tool().key();
                data[key + ":Connection"] = QString::number(connMap[&tool->connection()]);
                id++;
            }
        }

        toTemplateProvider::exportAllData(data, prefix + ":Templates");
    }
    TOCATCH
}

void toMain::importData(std::map<QString, QString> &data, const QString &prefix)
{
    if (data[prefix + ":State"] == QString::fromLatin1("Maximized"))
        showMaximized();
    else if (data[prefix + ":State"] == QString::fromLatin1("Minimized"))
        showMinimized();
    else
    {
        int width = data[prefix + ":Width"].toInt();
        if (width == 0)
        {
            TOMessageBox::warning(toMainWidget(),
                                  tr("Invalid session file"), tr("The session file is not valid, can't read it."));
            return ;
        }
        else
            setGeometry(data[prefix + ":X"].toInt(),
                        data[prefix + ":Y"].toInt(),
                        width,
                        data[prefix + ":Height"].toInt());
        showNormal();
    }

    std::map<int, toConnection *> connMap;

    int id = 1;
    std::map<QString, QString>::iterator i;
    while ((i = data.find(prefix + ":Connection:" + QString::number(id) + ":Database")) != data.end())
    {
        QString key = prefix + ":Connection:" + QString::number(id);
        QString database = (*i).second;
        QString user = data[key + ":User"];
        QString host = data[key + ":Host"];
        QString schema = data[key + ":Schema"];

        QStringList optionlist = data[key + ":Options"].split(",");
        std::set<QString> options;
        for (int j = 0; j < optionlist.count(); j++)
            if (!optionlist[j].isEmpty())
                options.insert(optionlist[j]);

        QString password = Utils::toUnobfuscate(data[key + ":Password"]);
        QString provider = data[key + ":Provider"];
        bool ok = true;
        if (toConfigurationNewSingle::Instance().defaultPassword() == password)
        {
            password = QInputDialog::getText(this,
                                             tr("Input password"),
                                             tr("Enter password for %1").arg(database),
                                             QLineEdit::Password,
                                             "",
                                             &ok);
        }
        if (ok)
        {
            try
            {
                toConnection *conn = new toConnection(provider.toLatin1(), user, password, host, database, schema, "", options);
                if (conn)
                {
                    conn = addConnection(conn, false);
                    connMap[id] = conn;
                }
            }
            TOCATCH
        }
        id++;
    }

    id = 1;
    while ((i = data.find(prefix + ":Tools:" + QString::number(id).toLatin1() + ":Type")) != data.end())
    {
        QString key = (*i).second.toLatin1();
        int connid = data[prefix + ":Tools:" + QString::number(id).toLatin1() + ":Connection"].toInt();
        std::map<int, toConnection *>::iterator j = connMap.find(connid);
        if (j != connMap.end())
        {
            toTool *pTool = ToolsRegistrySing::Instance().value(key);
            if (pTool)
            {
                QWidget *widget = pTool->toolWindow(workspace(), *((*j).second));
                const QPixmap *icon = pTool->toolbarImage();
                if (icon)
                    widget->setWindowIcon(*icon);
                widget->show();
                if (widget)
                {
                    toToolWidget *tw = dynamic_cast<toToolWidget *>(widget);
                    if (tw)
                    {
                        toToolCaption(tw, pTool->name());
                        tw->importData(data, prefix + ":Tools:" + QString::number(id));
                        toolWidgetAdded(tw);
                    }
                }
            }
        }
        id++;
    }

    toTemplateProvider::importAllData(data, prefix + ":Templates");
    updateWindowsMenu();
}
#endif

#ifdef TORA3_SESSION
void toMain::saveSession(void)
{
    QString fn = toSaveFilename(QString::null, QString::fromLatin1("*.tse"), this);
    if (!fn.isEmpty())
    {
        std::map<QString, QString> session;
        exportData(session, "TOra");
        try
        {
            toConfigurationNewSingle::Instance().saveMap(fn, session);
        }
        TOCATCH
    }
}

void toMain::loadSession(void)
{
    QString filename = toOpenFilename(QString::fromLatin1("*.tse"), this);
    if (!filename.isEmpty())
    {
        try
        {
            std::map<QString, QString> session;
            toConfigurationNewSingle::Instance().loadMap(filename, session);
            importData(session, "TOra");
        }
        TOCATCH
    }
}

void toMain::closeSession(void)
{
    std::map<QString, QString> session;
    exportData(session, "TOra");
    try
    {
        toConfigurationNewSingle::Instance().saveMap(toConfigurationNewSingle::Instance().defaultSession(), session);
    }
    TOCATCH

    while (workspace()->subWindowList().count() > 0 && workspace()->subWindowList().at(0))
        if (workspace()->subWindowList().at(0) &&
                !workspace()->subWindowList().at(0)->close())
            return ;

    while (Connections.end() != Connections.begin())
    {
        if (!delCurrentConnection())
            return ;
    }
}
#endif

void toMain::displayMessage(void)
{
    if (StatusMessages.size() < 1)
        return;

    Message->appendText(*(--StatusMessages.end()));
}

/** Handle events from toEditWidget subclasses */
void toMain::receivedFocus(toEditWidget *widget)
{
    if (/*toWorksheetText *sheet = */dynamic_cast<toWorksheetText *>(widget))
    {
        RowLabel->setText("?");
        ColumnLabel->setText("?");
    }
}

/** Handle events from toEditWidget subclasses */
void toMain::lostFocus(toEditWidget *widget)
{
    RowLabel->setText(QString::null);
    ColumnLabel->setText(QString::null);
}
