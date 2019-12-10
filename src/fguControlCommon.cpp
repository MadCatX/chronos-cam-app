#include "fguControlCommon.h"

#include <QtDBus/QDBusConnection>
#include <QDebug>

const QString FguControl::objectPath{"/cz/cas/fgu/control"};
const QString FguControl::serviceName{"cz.cas.fgu.control"};

void FguControl::registerMetas()
{
    qRegisterMetaType<FguControlVideoSettings>("FguControlVideoSettings");
    qDBusRegisterMetaType<FguControlVideoSettings>();
}

bool FguControl::registerService(QObject *target)
{
    auto conn = QDBusConnection::systemBus();
    if (!conn.isConnected()) {
        qDebug() << "Cannot connect to system bus";
        return false;
    }

    if (!conn.registerService(serviceName)) {
        qDebug() << "Cannot register" << serviceName << "D-Bus service";
        return false;
    }

    if (!conn.registerObject(objectPath, target)) {
        qDebug() << "Cannot register" << objectPath << "D-Bus object";
        return false;
    }

    qDebug() << serviceName << "service successfully registered";
    return true;
}
