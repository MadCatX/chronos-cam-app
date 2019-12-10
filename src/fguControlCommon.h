#ifndef FGUCONTROLCOMMON_H
#define FGUCONTROLCOMMON_H

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>

class FguControl {
public:
    static void registerMetas();
    static bool registerService(QObject *target);

    static const QString objectPath;
    static const QString serviceName;
};

class FguControlVideoSettings {
public:
    quint32 vres;
    quint32 hres;
    double framerate;
    bool isValid;

    friend QDBusArgument & operator<<(QDBusArgument &argument, const FguControlVideoSettings &payload)
    {
        argument.beginStructure();
        argument << payload.vres;
        argument << payload.hres;
        argument << payload.framerate;
        argument << payload.isValid;
        argument.endStructure();

        return argument;
    }

    friend const QDBusArgument & operator>>(const QDBusArgument &argument, FguControlVideoSettings &payload)
    {
        argument.beginStructure();
        argument >> payload.vres;
        argument >> payload.hres;
        argument >> payload.framerate;
        argument >> payload.isValid;
        argument.endStructure();

        return argument;
    }
};
Q_DECLARE_METATYPE(FguControlVideoSettings)

#endif // FGUCONTROLCOMMON_H
