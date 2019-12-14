/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -a fguControlAdaptor -i fguControlCommon.h -c FguControlAdaptor cz.cas.fgu.control.xml
 *
 * qdbusxml2cpp is Copyright (C) 2015 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef FGUCONTROLADAPTOR_H
#define FGUCONTROLADAPTOR_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
#include "fguControlCommon.h"
QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE

/*
 * Adaptor class for interface cz.cas.fgu.control
 */
class FguControlAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cz.cas.fgu.control")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"cz.cas.fgu.control\">\n"
"    <method name=\"set_video_settings\">\n"
"      <arg direction=\"in\" type=\"i\" name=\"vres\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"hres\"/>\n"
"      <arg direction=\"in\" type=\"d\" name=\"framerate\"/>\n"
"      <arg direction=\"out\" type=\"(iidb)\" name=\"settings\"/>\n"
"      <annotation value=\"FguControlVideoSettings\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"    <method name=\"get_video_settings\">\n"
"      <arg direction=\"out\" type=\"(iidb)\" name=\"settings\"/>\n"
"      <annotation value=\"FguControlVideoSettings\" name=\"org.qtproject.QtDBus.QtTypeName.Out0\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
public:
    FguControlAdaptor(QObject *parent);
    virtual ~FguControlAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    FguControlVideoSettings get_video_settings();
    FguControlVideoSettings set_video_settings(int vres, int hres, double framerate);
Q_SIGNALS: // SIGNALS
};

#endif