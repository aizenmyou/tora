//***************************************************************************
/*
 * TOra - An Oracle Toolkit for DBA's and developers
 * Copyright (C) 2003 Quest Software, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *      As a special exception, you have permission to link this program
 *      with the Oracle Client libraries and distribute executables, as long
 *      as you follow the requirements of the GNU GPL in regard to all of the
 *      software in the executable aside from Oracle client libraries.
 *
 *      Specifically you are not permitted to link this program with the
 *      Qt/UNIX, Qt/Windows or Qt Non Commercial products of TrollTech.
 *      And you are not permitted to distribute binaries compiled against
 *      these libraries without written consent from Quest Software, Inc.
 *      Observe that this does not disallow linking to the Qt Free Edition.
 *
 * All trademarks belong to their respective owners.
 *
 ****************************************************************************/

#ifndef TOPROJECT_H
#define TOPROJECT_H

#include "totemplate.h"

#include <map>

class toProjectTemplateItem : public toTemplateItem {
  QString Filename;
  int Order;
  void setup(const QString &name,bool open);
  int order(bool asc);
public:
  toProjectTemplateItem(toTemplateItem *item,QListViewItem *after,QString name,bool open=true);
  toProjectTemplateItem(QListView *item,QString name=QString::null,bool open=true);

  virtual QString key(int col,bool asc) const;

  const QString &filename(void) const
  { return Filename; }
  void setFilename(const QString &file);
  bool project(void);
  virtual void selected(void);
  toProjectTemplateItem *previousSibling(void);

  virtual QWidget *selectedWidget(QWidget *parent);
};

class toProject : public QVBox {
  Q_OBJECT
  
  toProjectTemplateItem *Root;
  QToolButton *DelFile;
  QListView *Project;
  std::map<QListViewItem *,toProjectTemplateItem *> ItemMap;

  void update(toProjectTemplateItem *sourceparent,toResultViewItem *parent);
  QString generateSQL(toProjectTemplateItem *item);
public:
  toProject(toProjectTemplateItem *top,QWidget *parent);
  ~toProject();

  void selectItem(toProjectTemplateItem *);
public slots:
  void update(void);
  void addFile(void);
  void delFile(void); 
  void newProject(void);
  void saveProject(void);
  void generateSQL(void);
  void selectionChanged(void);
  void moveDown(void);
  void moveUp(void);
};

class toProjectTemplate : public QObject, public toTemplateProvider {
  Q_OBJECT

  toProjectTemplateItem *Root;
  std::map<QCString,QString> Import;

  QToolButton *AddFile;
  QToolButton *DelFile;

  toProject *Details;
public:
  toProjectTemplate(void)
    : QObject(NULL,"projecttemplate"),toTemplateProvider("project")
  { Details=NULL; }

  virtual void exportData(std::map<QCString,QString> &data,const QCString &prefix);
  virtual void importData(std::map<QCString,QString> &data,const QCString &prefix);

  virtual void insertItems(QListView *parent,QToolBar *toolbar);
  virtual void removeItems(QListViewItem *item);

  virtual toProject *selectedWidget(QWidget *parent);

  virtual toProjectTemplateItem *root(void)
  { return Root; }

  friend class toProject;
public slots:
  void addFile();
  void delFile();
  void changeItem(QListViewItem *item);
};

#endif
