
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

#include "core/toconfiguration.h"
#include "core/utils.h"
#include "core/toconf.h"
#include "ts_log/ts_log_utils.h"

#include <QtCore/QObject>
#include <QtCore/QLocale>
#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtCore/QDate>

toConfiguration::toConfiguration(QObject *parent)
    : QObject(parent)
    , m_settings(TOORGNAME, TOAPPNAME)
{
    setQSettingsEnv();
}

toConfiguration::~toConfiguration()
{
}

void toConfiguration::registerConfigContext(QString const& contextName, QMetaEnum const& fields, ToConfiguration::ConfigContext const* context)
{
    Q_ASSERT_X( !m_contextMap.contains(contextName), qPrintable(__QHERE__), qPrintable(QString("Context %1 is already registered").arg(contextName)));
    m_contextMap.insert(contextName, fields);

    for (int idx = 0; idx < fields.keyCount(); idx++)
    {
        int intVal = fields.value(idx);
        QString strVal = fields.key(idx);
        Q_ASSERT_X( !m_configMap.contains(intVal), qPrintable(__QHERE__), qPrintable(QString("Context %1 value %2(%3) is already registered").arg(contextName).arg(strVal).arg(intVal)));
        m_configMap.insert(intVal, QVariant());
        m_configContextPtrMap.insert(intVal, context);

        Q_ASSERT_X( !m_optionToEnumMap.contains(strVal), qPrintable(__QHERE__), qPrintable(QString("Context %1 value %2(%3) is already registered").arg(contextName).arg(strVal).arg(intVal)));
        m_optionToEnumMap.insert(strVal, intVal);
        m_enumToOptionMap.insert(intVal, strVal);
    }

}

void toConfiguration::registerConfigObserver(toConfigOptionObserver *o)
{
    m_observers[o->m_option].insert(o);
}

void toConfiguration::unRegisterConfigObserver(toConfigOptionObserver *o)
{
    m_observers[o->m_option].remove(o);
}

QVariant toConfiguration::option(int optionKey)
{
    Q_ASSERT_X( m_configMap.contains(optionKey), qPrintable(__QHERE__), qPrintable(QString("Unknown enum: %1").arg(optionKey)));
    // If config option was not loaded yet load it
    ToConfiguration::ConfigContext const* ctx = NULL;
    QVariant defaultValue;
    if (m_configMap.value(optionKey).isNull())
    {
        ctx = m_configContextPtrMap.value(optionKey);
        defaultValue = ctx->defaultValue(optionKey);	// defaultValue also determines datatype
        {
            m_settings.beginGroup("preferences");
            m_settings.beginGroup(ctx->name());

            QVariant prefValue = m_settings.value(m_enumToOptionMap.value(optionKey));
            if (!prefValue.isNull() && prefValue.canConvert(defaultValue.type()) && prefValue.convert(defaultValue.type()))
            {
                m_configMap[optionKey] = prefValue;
            }
            // We are looking for some complex (non-scalar) type stored in configuration
            else if (defaultValue.type() == QVariant::UserType)
            {
                QVariant v;
                m_settings.beginGroup(m_enumToOptionMap.value(optionKey));
                ctx->loadUserType(m_settings, v, optionKey);
                m_settings.endGroup();
                m_configMap[optionKey] = v;
            }
            m_settings.endGroup();
            m_settings.endGroup();
        }
    }
    // If config option was not loaded yet load it (use either Tora 2.x value or defaultValue)
    if (m_configMap.value(optionKey).isNull())
    {
        QVariant oldValue= ctx->toraIIValue(optionKey);
        if (!oldValue.isNull() && oldValue.canConvert(defaultValue.type()) && oldValue.convert(defaultValue.type()))
            m_configMap[optionKey] = oldValue;
        else
            m_configMap[optionKey] = defaultValue;
    }
    return m_configMap.value(optionKey);
}

QVariant toConfiguration::option(QString const& o)
{
    if (!m_optionToEnumMap.contains(o))
        throw OptionNotFound();
    int opt = m_optionToEnumMap.value(o);
    return option(opt);
}

void toConfiguration::saveAll()
{
    m_settings.beginGroup("preferences");

    Q_FOREACH(QString key, m_optionToEnumMap.keys())
    {
        int optionKey = m_optionToEnumMap.value(key);
        ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(optionKey);
        QVariant defaultValue = ctx->defaultValue(optionKey);	// defaultValue also determines datatype
        QVariant prefValue = m_settings.value(key);
        QVariant currValue = m_configMap.value(optionKey);

        // Let's pretend for a while that defaultValue's type is a String (while it is a date)
        // Avoid storing Dates in QVariant's binary format. Rather store them as ISODate.
        // This format is used by default when converting Strings back into Date
        if (currValue.type() == QVariant::Date && currValue.type() == QVariant::Date)
        {
        	currValue = QVariant(currValue.toDate().toString(Qt::ISODate));
        	defaultValue = QVariant(defaultValue.toDate().toString(Qt::ISODate));
        }

        // Store new value
        if (!currValue.isNull() && currValue.canConvert(defaultValue.type()) && currValue.convert(defaultValue.type()) && currValue != defaultValue)
        {
            m_settings.beginGroup(ctx->name());
            m_settings.setValue(key, currValue);
            m_settings.endGroup();
            continue;
        }
        // New value equals to defaltValue (remove it from registry);
        else if (!currValue.isNull() && currValue.canConvert(defaultValue.type()) && currValue.convert(defaultValue.type()) && currValue == defaultValue)
        {
            m_settings.beginGroup(ctx->name());
            m_settings.remove(key);
            m_settings.endGroup();
            continue;
        }
        else if ( !currValue.isNull() && currValue.type() == QVariant::UserType)
        {
            m_settings.beginGroup(ctx->name());
            m_settings.beginGroup(m_enumToOptionMap.value(optionKey));
            ctx->saveUserType(m_settings, currValue, optionKey);
            m_settings.endGroup();
            m_settings.endGroup();
            continue;
        }
        if (!prefValue.isNull() && prefValue.canConvert(defaultValue.type()) && prefValue.convert(defaultValue.type()) && prefValue != defaultValue)
        {
            m_settings.beginGroup(ctx->name());
            m_settings.setValue(key, prefValue);
            m_configMap[optionKey] = prefValue;
            m_settings.endGroup();
            continue;
        }
    }
    m_settings.endGroup();
    m_settings.sync();
}

// a static one
void toConfiguration::setQSettingsEnv()
{
    if (QCoreApplication::organizationName().isEmpty())
        QCoreApplication::setOrganizationName(TOORGNAME);
    if (QCoreApplication::organizationDomain().isEmpty())
        QCoreApplication::setOrganizationDomain(TOORGDOMAIN);
    if (QCoreApplication::applicationName().isEmpty())
        QCoreApplication::setApplicationName(TOAPPNAME);
}

QString toConfiguration::sharePath()
{
#ifdef Q_OS_WIN32
    QString appl = QCoreApplication::applicationDirPath();
    appl += QDir::separator();
    appl += "i18n";
    appl += QDir::separator();
    return appl;
#else
    return QString::fromLatin1(SHARE_DIR);
#endif
}

template<>
void toConfiguration::setOption <QVariant>(int option, QVariant const& newVal)
{
#ifdef QT_DEBUG
    ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(option);
    QVariant val = ctx->defaultValue(option);
    Q_ASSERT_X(newVal.type() == val.type(), qPrintable(__QHERE__), qPrintable(QString("Option enum: %1 oldtype: %2 newtype %3")
               .arg(option)
               .arg(val.typeName())
               .arg(newVal.typeName())
                                                                             ));
#endif
    m_configMap[option] = newVal;
    notifyOservers(option, newVal);
}

template<> TORA_EXPORT
void toConfiguration::setOption <QString>(int option, QString const& newVal)
{
#ifdef QT_DEBUG
    ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(option);
    QVariant val = ctx->defaultValue(option);
    Q_ASSERT_X(QVariant::String == val.type(), qPrintable(__QHERE__), qPrintable(QString("Option enum: %1 oldtype: %2 newtype QString")
               .arg(option)
               .arg(val.typeName())
                                                                                ));
#endif
    m_configMap[option] = QVariant(newVal);
    notifyOservers(option, newVal);
}

template<> TORA_EXPORT
void toConfiguration::setOption <int>(int option, int const&newVal)
{
#ifdef QT_DEBUG
    ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(option);
    QVariant val = ctx->defaultValue(option);
    Q_ASSERT_X(QVariant::Int == val.type(), qPrintable(__QHERE__), qPrintable(QString("Option enum: %1 oldtype: %2 newtype int")
               .arg(option)
               .arg(val.typeName())
                                                                             ));
#endif
    m_configMap[option] = QVariant(newVal);
    notifyOservers(option, newVal);
}

template<> TORA_EXPORT
void toConfiguration::setOption <bool>(int option, bool const&newVal)
{
#ifdef QT_DEBUG
    ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(option);
    QVariant val = ctx->defaultValue(option);
    Q_ASSERT_X(QVariant::Bool == val.type(), qPrintable(__QHERE__), qPrintable(QString("Option enum: %1 oldtype: %2 newtype bool")
               .arg(option)
               .arg(val.typeName())
                                                                              ));
#endif
    m_configMap[option] = QVariant(newVal);
    notifyOservers(option, newVal);
}

template<> TORA_EXPORT
void toConfiguration::setOption <QDate>(int option, QDate const&newVal)
{
#ifdef QT_DEBUG
    ToConfiguration::ConfigContext const* ctx = m_configContextPtrMap.value(option);
    QVariant val = ctx->defaultValue(option);
    Q_ASSERT_X(QVariant::Date == val.type(), qPrintable(__QHERE__), qPrintable(QString("Option enum: %1 oldtype: %2 newtype QDate")
               .arg(option)
               .arg(val.typeName())
                                                                              ));
#endif
    m_configMap[option] = QVariant(newVal);
    notifyOservers(option, newVal);
}

void toConfiguration::logUnknownOption(QString const&)
{

}

void toConfiguration::notifyOservers(int option, QVariant const&value) const
{
    QSet<toConfigOptionObserver*> const& observers = m_observers.value(option);
    foreach(toConfigOptionObserver *o, observers)
    {
        o->notify(value);
    }
}

toConfigOptionObserver::operator bool() const
{
    return m_value.toBool();
}

toConfigOptionObserver::operator int() const
{
    return m_value.toInt();
}
toConfigOptionObserver::operator QString() const
{
    return m_value.toString();
}

toConfigOptionObserver::operator QDate() const
{
    return m_value.toDate();
}
