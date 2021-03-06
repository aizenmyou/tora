
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

#include "core/toconnection.h"

#include <list>
#include <map>
#include <algorithm>

#include <QWidget>
#include <QtCore/QString>

class toTreeWidget;
class toEventQuery;
class toPieChart;
class toResultBar;
class QSplitter;

class toWaitEvents : public QWidget
{
        Q_OBJECT;

        QSplitter * splitter;
#ifdef TORA_EXPERIMENTAL
        toResultBar *Delta;
        toResultBar *DeltaTimes;
        toPieChart *AbsolutePie;
        toPieChart *DeltaPie;
#endif
        toTreeWidget *Types;
        toEventQuery *Query;

        bool First;
        bool ShowTimes;
        QString Now;
        std::list<QString> Labels;
        time_t LastTime;
        std::list<double> LastCurrent;
        std::list<double> LastTimes;
        std::list<double> Current;
        std::list<double> CurrentTimes;
        std::list<double> Relative;
        std::list<double> RelativeTimes;
        std::list<bool> Enabled;

        int Session;

        std::map<QString, bool> HideMap;

        void setup(int session);
    public:
        toWaitEvents(QWidget *parent, const char *name);
        toWaitEvents(int session, QWidget *parent, const char *name);
        ~toWaitEvents();

        void setSession(int session);

        virtual void exportData(std::map<QString, QString> &data, const QString &prefix);
        virtual void importData(std::map<QString, QString> &data, const QString &prefix);
    public slots:
        virtual void connectionChanged(void);
        virtual void changeSelection(void);
        void slotPoll(toEventQuery*);
        void slotQueryDone(toEventQuery*);
        void slotErrorHanler(toEventQuery*, toConnection::exception const &);
        virtual void refresh(void);
#if 0
        virtual void start(void);
        virtual void stop(void);
#endif
        virtual void changeType(int);
};

