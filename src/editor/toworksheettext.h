
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

#pragma once

#include "core/toconfiguration.h"
#include "core/toeditorconfiguration.h"
#include "editor/tosqltext.h"

class toComplPopup;
class toWorksheet;
class QFileSystemWatcher;

class toWorksheetText : public toSqlText
{
        Q_OBJECT;
        Q_ENUMS(EditorTypeEnum);
        typedef toSqlText super;
        friend class toComplPopup;
    public:

        enum EditorTypeEnum
        {
            SciTe   = 100
#ifdef TORA_EXPERIMENTAL
            , Custom  = 200
            , Emacs   = 300
#endif
        };

        /** Create a new editor.
         * @param parent Parent of widget.
         * @param name Name of widget.
         */
        toWorksheetText(toWorksheet *worksheet, QWidget *parent, const char *name = NULL);

        virtual ~toWorksheetText();

	void setHighlighter(toSqlText::HighlighterTypeEnum) override;

        // Override QScintilla (display custom toComplPopup window)
        void autoCompleteFromAPIs() override;

        /** Get filename of current file in editor.
         * @return Filename of editor.
         */
        QString const& filename(void) const;

        /** Open a file for editing.
         * @param file File to open for editing.
         */
        void openFilename(const QString &file);

        /** Set the current filename of the file in editor.
         * @param str String containing filename.
         */
        void setFilename(const QString &file);

        bool editOpen(const QString &suggestedFile = QString::null) override;
        bool editSave(bool askfile) override;

    public slots:
        void setEditorType(int);

        void handleBookmark();
        void gotoPrevBookmark();
        void gotoNextBookmark();


        // Insert chosen text
        void completeFromAPI(QListWidgetItem * item);

        void positionChanged(int row, int col);

    protected slots:
        void setCaretAlpha();

        //! \brief Handle file external changes (3rd party modifications)
        void m_fsWatcher_fileChanged(const QString & filename);

    signals:
        // emitted when a new file is opened
        void fileOpened(void);
        void fileOpened(QString file);
        void fileSaved(QString file);

    protected:
        /*! \brief Override QScintilla event handler to display code completion popup */
        void keyPressEvent(QKeyEvent * e) override;

        /*! \brief Guess what should be used for code completion
        in this time.
        When SQL parser can decide the editor is in FOO.bar state
        it will suggest "bar" related columns etc.
        When SQL parser couldn't find any suggestion it will list
        keywords/functions from templates/completion.api list.
        \param partial a QString reference with starting char sequence
        */
        QStringList getCompletionList(QString &partial);

        void completeWithText(QString const&);

        void focusInEvent(QFocusEvent *e) override;
        void focusOutEvent(QFocusEvent *e) override;

        void fsWatcherClear();

#ifdef TORA3_SESSION
        /** Export data to a map.
         * @param data A map that can be used to recreate the data of a chart.
         * @param prefix Prefix to add to the map.
         */
        virtual void exportData(std::map<QString, QString> &data, const QString &prefix);
        /** Import data
         * @param data Data to read from a map.
         * @param prefix Prefix to read data from.
         */
        virtual void importData(std::map<QString, QString> &data, const QString &prefix);
#endif

    private:
        EditorTypeEnum editorType;
        toComplPopup* popup;

    protected:
        toWorksheet *m_worksteet; // parent Workseet tool
        QsciAbstractAPIs* m_complAPI;
        QTimer* m_complTimer;

        QString m_filename;

        //! Watch for file (if any) changes from external apps
        QFileSystemWatcher* m_fsWatcher;

        //! \brief A handler for current line highlighting - margin
        // FIXME: disabled due repainting issues
        // int m_currentLineMarginHandle;
        //! \brief A handler for bookrmarks - line highlighted
        int m_bookmarkHandle;
        //! \brief A handler for bookrmarks - margin
        int m_bookmarkMarginHandle;
        //! \brief Bookrmarks handler list used for navigation (next/prev)
        QList<int> m_bookmarks;

        bool m_completeEnabled, m_completeDelayed;

        OptionObserver<ToConfiguration::Editor::CaretLineBool> m_caretVisible;
        OptionObserver<ToConfiguration::Editor::CaretLineAlphaInt> m_caretAlpha;
};

/**
 * Subclass toToggleButton and iterate over values of HighlighterTypeEnum
 */
class toEditorTypeButton : public toToggleButton
{
        Q_OBJECT;
    public:
        toEditorTypeButton(QWidget *parent, const char *name = 0);
        toEditorTypeButton();
};

// this one will be usually parented by QStatusBar
typedef Loki::SingletonHolder<toEditorTypeButton, Loki::CreateUsingNew, Loki::NoDestroy> toEditorTypeButtonSingle;
