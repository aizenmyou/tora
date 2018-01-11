
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

#include "editor/toworksheettext.h"
#include "tools/toworksheet.h"
#include "editor/tocomplpopup.h"
#include "core/toconnection.h"
#include "core/toconnectiontraits.h"
#include "core/tologger.h"
#include "core/toglobalevent.h"
#include "shortcuteditor/shortcutmodel.h"

#include <QtCore/QFileSystemWatcher>
#include <QListWidget>
#include <QDir>

#include "core/toeditorconfiguration.h"

using namespace ToConfiguration;

toWorksheetText::toWorksheetText(toWorksheet *worksheet, QWidget *parent, const char *name)
    : toSqlText(parent, name)
    , editorType(SciTe)
    , popup(new toComplPopup(this))
    , m_worksteet(worksheet)
    , m_complAPI(NULL)
    , m_complTimer(new QTimer(this))
    , m_fsWatcher(new QFileSystemWatcher(this))
    , m_bookmarkHandle(QsciScintilla::markerDefine(QsciScintilla::Background))
    , m_bookmarkMarginHandle(QsciScintilla::markerDefine(QsciScintilla::RightTriangle))
    , m_completeEnabled(toConfigurationNewSingle::Instance().option(Editor::CodeCompleteBool).toBool())
    , m_completeDelayed((toConfigurationNewSingle::Instance().option(Editor::CodeCompleteDelayInt).toInt() > 0))
{
    FlagSet.Open = true;

    if (m_completeEnabled && !m_completeDelayed)
    {
        QsciScintilla::setAutoCompletionThreshold(1); // start when a single leading word's char is typed
        QsciScintilla::setAutoCompletionUseSingle(QsciScintilla::AcusExplicit);
        QsciScintilla::setAutoCompletionSource(QsciScintilla::AcsAll); // AcsAll := AcsAPIs | AcsDocument
    }
    QsciScintilla::setAutoIndent(true);

    setCaretAlpha();
    connect(&m_caretVisible, SIGNAL(valueChanged(QVariant const&)), this, SLOT(setCaretAlpha()));
    connect(&m_caretAlpha, SIGNAL(valueChanged(QVariant const&)), this, SLOT(setCaretAlpha()));
    connect(m_fsWatcher, SIGNAL(fileChanged(const QString&)), this, SLOT(m_fsWatcher_fileChanged(const QString&)));

    // handle "max text width" mark
    if (toConfigurationNewSingle::Instance().option(Editor::UseMaxTextWidthMarkBool).toBool())
    {
        QsciScintilla::setEdgeColumn(toConfigurationNewSingle::Instance().option(Editor::MaxTextWidthMarkInt).toInt());
        // TODO setEdgeColor(DefaultAnalyzer.getColor(toSyntaxAnalyzer::CurrentLineMarker).darker(150));
        QsciScintilla::setEdgeMode(QsciScintilla::EdgeLine);
    }
    else
        QsciScintilla::setEdgeMode(QsciScintilla::EdgeNone);


    //connect (this, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(positionChanged(int, int)));
    connect( m_complTimer, SIGNAL(timeout()), this, SLOT(autoCompleteFromAPIs()) );

    connect(&toEditorTypeButtonSingle::Instance(),
            SIGNAL(toggled(int)),
            this,
            SLOT(setEditorType(int)));

    popup->hide();
    connect(popup->list(),
            SIGNAL(itemClicked(QListWidgetItem*)),
            this,
            SLOT(completeFromAPI(QListWidgetItem*)));
    connect(popup->list(),
            SIGNAL(itemActivated(QListWidgetItem*)),
            this,
            SLOT(completeFromAPI(QListWidgetItem*)));
}

toWorksheetText::~toWorksheetText()
{
}

void toWorksheetText::setHighlighter(toSqlText::HighlighterTypeEnum e)
{
    super::setHighlighter(e);
    if (super::lexer())
    {
        m_complAPI = super::lexer()->apis();
    }
    else
    {
        m_complAPI = NULL;
    }
}

void toWorksheetText::keyPressEvent(QKeyEvent * e)
{
    long currPosition = currentPosition();
    long nextPosition = SendScintilla(QsciScintilla::SCI_POSITIONAFTER, currPosition);
    // handle editor shortcuts with TAB
    // It uses qscintilla lowlevel API to handle "word under cursor"
    // This code is taken from sqliteman.com
    if (e->key() == Qt::Key_Tab && toConfigurationNewSingle::Instance().option(Editor::UseEditorShortcutsBool).toBool())
    {
        long start = SendScintilla(SCI_WORDSTARTPOSITION, currPosition, true);
        long end = SendScintilla(SCI_WORDENDPOSITION, currPosition, true);
        QString key(wordAtPosition(currPosition, true));
        EditorShortcutsMap shorts(toConfigurationNewSingle::Instance().option(Editor::EditorShortcutsMap).toMap());
        if (shorts.contains(key))
        {
            setSelection(start, end);
            removeSelectedText();
            insert(shorts.value(key).toString());
            currPosition = SendScintilla(SCI_GETCURRENTPOS);
            SendScintilla(SCI_SETEMPTYSELECTION, currPosition + shorts.value(key).toByteArray().length());
            e->accept();
            return;
        }
    }
    else if (m_completeEnabled && e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_Space)
    {
        autoCompleteFromDocument();
        e->accept();
        return;
    }
    else if (m_completeEnabled && e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_T)
    {
        autoCompleteFromAPIs();
        e->accept();
        return;
    }
    super::keyPressEvent(e);
}

void toWorksheetText::positionChanged(int row, int col)
{
    using namespace ToConfiguration;
    using cc = toScintilla::CharClassify::cc;
    using ChClassEnum = toScintilla::CharClassify;

    long currPosition, nextPosition;
    wchar_t currChar, nextChar;
    cc currClass, nextClass;
    
    if (col <= 0)
        goto no_complete;

    if (m_completeEnabled == false || m_completeEnabled == false)
        goto no_complete;

    currPosition = currentPosition();
    nextPosition = SendScintilla(QsciScintilla::SCI_POSITIONAFTER, currPosition);

    currChar = getWCharAt(currPosition);
    nextChar = getWCharAt(nextPosition);

    currClass = CharClass(currChar);
    nextClass = CharClass(nextChar);

    TLOG(0, toNoDecorator, __HERE__) << currChar << std::endl;

    if (currChar == 0)
        goto no_complete;

    if ((currClass == ChClassEnum::ccWord || currClass == ChClassEnum::ccPunctuation) &&
            (nextClass == CharClassify::ccWord || nextClass == CharClassify::ccPunctuation))
        goto no_complete;

    // Cursor is not at EOL, not before any word character
    if (currClass != ChClassEnum::ccWord && currClass != ChClassEnum::ccSpace)
        goto no_complete;

    for(int i=1, c=col; i<3 && c; i++, c--)
    {
        currPosition = SendScintilla(QsciScintilla::SCI_POSITIONBEFORE, currPosition);
        currChar = getWCharAt(currPosition);
        if (currChar == L'.')
        {
            m_complTimer->start(toConfigurationNewSingle::Instance().option(Editor::CodeCompleteDelayInt).toInt());
            return;
        }
        if (currClass != CharClassify::ccWord)
            break;
    }

// FIXME: disabled due repainting issues
//    current line marker (margin arrow)
//    markerDeleteAll(m_currentLineMarginHandle);
//    markerAdd(row, m_currentLineMarginHandle);

no_complete:
    m_complTimer->stop();
}

void toWorksheetText::setCaretAlpha()
{
    // highlight caret line
    if ((bool)m_caretVisible)
    {
        QsciScintilla::setCaretLineVisible(true);
        // This is only required until transparency fixes in QScintilla go into stable release
        //QsciScintilla::SendScintilla(QsciScintilla::SCI_SETCARETLINEBACKALPHA, QsciScintilla::SC_ALPHA_NOALPHA);
        QsciScintilla::SendScintilla(QsciScintilla::SCI_SETCARETLINEBACKALPHA, (int)m_caretAlpha);
    } else {
        QsciScintilla::setCaretLineVisible(false);
    }
}
// the QScintilla way of autocomletition
#if 0
void toWorksheetText::autoCompleteFromAPIs()
{
    m_complTimer->stop(); // it's a must to prevent infinite reopening
    {
        toScintilla::autoCompleteFromAPIs();
        return;
    }
}
#endif

// the Tora way of autocomletition
void toWorksheetText::autoCompleteFromAPIs()
{
    m_complTimer->stop(); // it's a must to prevent infinite reopening

    Utils::toBusy busy;
    toConnection &connection = toConnection::currentConnection(this);

    TLOG(0, toTimeStart, __HERE__) << "Start" << std::endl;
    int position = currentPosition();
    toSqlText::Word schemaWord, tableWord;
    tableAtCursor(schemaWord, tableWord);

    QString schema = connection.getTraits().unQuote(schemaWord.text());          // possibly unquote schema name
    QString table = tableWord.start() > position ? QString() : tableWord.text(); // possible tableAtCursor matched some distant work (behind spaces)

    TLOG(0, toTimeDelta, __HERE__) << "Table at indexb: " << '"' << schemaWord.text() << '"' << ':' << '"' << tableWord.text() << '"' << std::endl;

    if (!schemaWord.text().isEmpty())
        setSelection(schemaWord.start(), position);
    else if (!tableWord.text().isEmpty())
        setSelection(tableWord.start(), position);
    else
        return;

    QStringList compleList;
    if (schema.isEmpty())
    	compleList = connection.getCache().completeEntry(toToolWidget::currentSchema(this), table);
    else
    	compleList = connection.getCache().completeEntry(schema, table);

    TLOG(0, toTimeDelta, __HERE__) << "Complete entry" << std::endl;

    if (compleList.size() <= 100) // Do not waste CPU on sorting huge completition list TODO: limit the amount of returned entries
    	compleList.sort();
    TLOG(0, toTimeDelta, __HERE__) << "Sort" << std::endl;

    if (compleList.isEmpty())
    {
    	this->SendScintilla(SCI_SETEMPTYSELECTION, position);
        return;
    }

    if (compleList.count() == 1 /*&& compleList.first() == partial*/)
    {
        completeWithText(compleList.first());
    }
    else
    {
        long position, posx, posy;
        int curCol, curRow;
        this->getCursorPosition(&curRow, &curCol);
        position = this->SendScintilla(SCI_GETCURRENTPOS);
        posx = this->SendScintilla(SCI_POINTXFROMPOSITION, 0, position);
        posy = this->SendScintilla(SCI_POINTYFROMPOSITION, 0, position) +
               this->SendScintilla(SCI_TEXTHEIGHT, curRow);
        QPoint p(posx, posy);
        p = mapToGlobal(p);
        popup->move(p);
        QListWidget *list = popup->list();
        list->clear();
        list->addItems(compleList);

        // if there's no current selection, select the first
        // item. that way arrow keys work as intended.
        QList<QListWidgetItem *> selected = list->selectedItems();
        if (selected.size() < 1 && list->count() > 0)
        {
            list->item(0)->setSelected(true);
            list->setCurrentItem(list->item(0));
        }

        popup->show();
        popup->setFocus();
        TLOG(0, toTimeTotal, __HERE__) << "End" << std::endl;
    }
}

void toWorksheetText::completeFromAPI(QListWidgetItem* item)
{
    if (item)
    {
        completeWithText(item->text());
    }
    popup->hide();
}

void toWorksheetText::completeWithText(QString const& text)
{
    long pos = currentPosition();
    int start = SendScintilla(SCI_WORDSTARTPOSITION, pos, true);
    int end = SendScintilla(SCI_WORDENDPOSITION, pos, true);
    // The text might be already selected by tableAtCursor
    if (!hasSelectedText())
    {
        setSelection(start, end);
    }
    removeSelectedText();
    insert(text);
    SendScintilla(SCI_SETCURRENTPOS,
                  SendScintilla(SCI_GETCURRENTPOS) +
                  text.length());
    pos = SendScintilla(SCI_GETCURRENTPOS);
    SendScintilla(SCI_SETSELECTIONSTART, pos, true);
    SendScintilla(SCI_SETSELECTIONEND, pos, true);
}

QString const& toWorksheetText::filename(void) const
{
    return m_filename;
}

void toWorksheetText::setFilename(const QString &filename)
{
    m_filename = filename;
}

void toWorksheetText::openFilename(const QString &file)
{
#pragma message WARN("TODO/FIXME: clear markers!")
    fsWatcherClear();

    QString data = Utils::toReadFile(file);
    setText(data);
    setFilename(file);
    setModified(false);
    toGlobalEventSingle::Instance().addRecentFile(file);

    m_fsWatcher->addPath(file);

    Utils::toStatusMessage(tr("File opened successfully"), false, false);
}

bool toWorksheetText::editOpen(const QString &suggestedFile)
{
    int ret = 1;
    if (isModified())
    {
        // grab focus so user can see file and decide to save
        setFocus(Qt::OtherFocusReason);

        ret = TOMessageBox::information(this,
                                            tr("Save changes?"),
                                            tr("The editor has been changed, do you want to save them\n"
                                               "before opening a new file?"),
                                            tr("&Save"), tr("&Discard"), tr("New worksheet"), 0);
        if (ret < 2)
            return false;
        else if (ret == 0)
            if (!editSave(false))
                return false;
    }

    QString fname;
    if (!suggestedFile.isEmpty())
        fname = suggestedFile;
    else
        fname = Utils::toOpenFilename(QString::null, this);

    if (!fname.isEmpty())
    {
        try
        {
            if (ret == 2)
                toGlobalEventSingle::Instance().editOpenFile(fname);
            else
            {
                openFilename(fname);
                emit fileOpened();
                emit fileOpened(fname);
            }
            return true;
        }
        TOCATCH
    }
    return false;
}

bool toWorksheetText::editSave(bool askfile)
{
    fsWatcherClear();
    bool ret = false;

    QString fn;
    QFileInfo file(filename());
    if (!filename().isEmpty() && file.exists() && file.isWritable())
        fn = file.absoluteFilePath();

    if (!filename().isEmpty() && fn.isEmpty() && file.dir().exists())
        fn = file.absoluteFilePath();

    if (askfile || fn.isEmpty())
        fn = Utils::toSaveFilename(fn, QString::null, this);

    if (!fn.isEmpty() && Utils::toWriteFile(fn, text()))
    {
        toGlobalEventSingle::Instance().addRecentFile(fn);
        setFilename(fn);
        setModified(false);
        emit fileSaved(fn);

        m_fsWatcher->addPath(fn);
        ret = true;
    }
    return ret;
}

void toWorksheetText::setEditorType(int)
{

}

void toWorksheetText::handleBookmark()
{
    int curline, curcol;
    getCursorPosition (&curline, &curcol);

    if (m_bookmarks.contains(curline))
    {
        markerDelete(curline, m_bookmarkHandle);
        markerDefine(curline, m_bookmarkMarginHandle);
        m_bookmarks.removeAll(curline);
    }
    else
    {
        markerAdd(curline, m_bookmarkHandle);
        markerAdd(curline, m_bookmarkMarginHandle);
        m_bookmarks.append(curline);
    }
    qSort(m_bookmarks);
}

void toWorksheetText::gotoPrevBookmark()
{
    int curline, curcol;
    getCursorPosition (&curline, &curcol);
    --curline;

    int newline = -1;
    foreach(int i, m_bookmarks)
    {
        if (curline < i)
            break;
        newline = i;
    }
    if (newline >= 0)
        setCursorPosition(newline, 0);
}

void toWorksheetText::gotoNextBookmark()
{
    int curline, curcol;
    getCursorPosition (&curline, &curcol);
    ++curline;

    int newline = -1;
    foreach(int i, m_bookmarks)
    {
        if (curline > i)
            continue;
        newline = i;
        break;
    }
    if (newline >= 0)
        setCursorPosition(newline, 0);
}

QStringList toWorksheetText::getCompletionList(QString &partial)
{
    TLOG(0, toTimeStart, __HERE__) << "Start" << std::endl;
    int curline, curcol;
    getCursorPosition (&curline, &curcol);
    QString word = wordAtLineIndex(curline, curcol);
    TLOG(0, toTimeDelta, __HERE__) << "Word at index: " << word << std::endl;
    QStringList retval = toConnection::currentConnection(this).getCache().completeEntry("" , word);
    TLOG(0, toTimeDelta, __HERE__) << "Complete entry" << std::endl;
    QStringList retval2;
    {
        //QWidget * parent = parentWidget();
        //QWidget * parent2 = parent->parentWidget();
        //if (toWorksheetEditor *editor = dynamic_cast<toWorksheetEditor*>(parentWidget()))
        //	if(toWorksheet *worksheet = dynamic_cast<toWorksheet*>(editor))
        //		retval2 = toConnection::currentConnection(this).getCache().completeEntry(worksheet->currentSchema()+'.' ,word);
        retval2 = toConnection::currentConnection(this).getCache().completeEntry(toToolWidget::currentSchema(this), word);
    }

    if (retval2.size() <= 100) // Do not waste CPU on sorting huge completition list TODO: limit the amount of returned entries
        retval2.sort();
    TLOG(0, toTimeDelta, __HERE__) << "Sort" << std::endl;
    Q_FOREACH(QString t, retval)
    {
        //TLOG(0, toNoDecorator, __HERE__) << " Tab: " << t << std::endl;
    }
    TLOG(0, toTimeTotal, __HERE__) << "End" << std::endl;
    return retval2;
}

void toWorksheetText::focusInEvent(QFocusEvent *e)
{
    toEditorTypeButtonSingle::Instance().setEnabled(true);
    toEditorTypeButtonSingle::Instance().setValue(editorType);
    super::focusInEvent(e);
}

void toWorksheetText::focusOutEvent(QFocusEvent *e)
{
    toEditorTypeButtonSingle::Instance().setDisabled(true);
    super::focusOutEvent(e);
}

void toWorksheetText::m_fsWatcher_fileChanged(const QString & filename)
{
    m_fsWatcher->blockSignals(true);
    setFocus(Qt::OtherFocusReason);
    if (QMessageBox::question(this, tr("External File Modification"),
                              tr("File %1 was modified by an external application. Reload (your changes will be lost)?").arg(filename),
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    {
        return;
    }

    try
    {
        openFilename(filename);
    }
    TOCATCH;

    m_fsWatcher->blockSignals(false);
}

void toWorksheetText::fsWatcherClear()
{
    QStringList l(m_fsWatcher->files());
    if (!l.empty())
        m_fsWatcher->removePaths(l);
}

#ifdef TORA3_SESSION
void toWorksheetText::exportData(std::map<QString, QString> &data, const QString &prefix)
{
    data[prefix + ":Filename"] = Filename;
    data[prefix + ":Text"] = text();
    int curline, curcol;
    getCursorPosition (&curline, &curcol);
    data[prefix + ":Column"] = QString::number(curcol);
    data[prefix + ":Line"] = QString::number(curline);
    if (isModified())
        data[prefix + ":Edited"] = "Yes";
}

void toWorksheetText::importData(std::map<QString, QString> &data, const QString &prefix)
{
    QString txt = data[prefix + ":Text"];
    if (txt != text())
        setText(txt);
    Filename = data[prefix + ":Filename"];
    setCursorPosition(data[prefix + ":Line"].toInt(), data[prefix + ":Column"].toInt());
    if (data[prefix + ":Edited"].isEmpty())
        setModified(false);
}
#endif

toEditorTypeButton::toEditorTypeButton(QWidget *parent, const char *name)
    : toToggleButton(ENUM_REF(toWorksheetText, EditorTypeEnum), parent, name)
{
}

toEditorTypeButton::toEditorTypeButton()
    : toToggleButton(ENUM_REF(toWorksheetText, EditorTypeEnum), NULL)
{
}
