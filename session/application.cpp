/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "application.h"
#include "sessionadaptor.h"
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QDir>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv),
      m_moduleManager(new ModuleManager)
{
    new SessionAdaptor(this);
    // connect to D-Bus and register as an object:
    QDBusConnection::sessionBus().registerService(QSL("org.panda.Session"));
    QDBusConnection::sessionBus().registerObject(QSL("/Session"), this);

    createConfigDirectory();

    // init env
    qputenv("XDG_CURRENT_DESKTOP", "Panda");
    qputenv("XDG_CONFIG_HOME", QString("%1/.config").arg(QDir::homePath()).toLocal8Bit());
    qputenv("XDG_CACHE_HOME", QString("%1/.cache").arg(QDir::homePath()).toLocal8Bit());
    qputenv("XDG_DESKTOP_DIR", QString("%1/Desktop").arg(QDir::homePath()).toLocal8Bit());
    qputenv("QT_PLATFORM_PLUGIN", "panda");
    qputenv("QT_QPA_PLATFORMTHEME", "panda");

    // ref: https://www.intel.com/content/www/us/en/programmable/support/support-resources/knowledge-base/solutions/rd04042012_367.html
    unsetenv("SESSION_MANAGER");

    initLanguage();
    initCursor();
    initScreenScaleFactors();

    if (!syncDBusEnvironment()) {
        // Startup error
        qDebug() << "Could not sync environment to dbus.";
    }

    QTimer::singleShot(100, m_moduleManager, &ModuleManager::startup);
}

int Application::runSync(const QString& program, const QStringList &args, const QStringList &env)
{
    QProcess p;
    if (!env.isEmpty())
        p.setEnvironment(QProcess::systemEnvironment() << env);
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.start(program, args);
    p.waitForFinished(-1);
    if (p.exitCode()) {
        qWarning() << program << args << "exited with code" << p.exitCode();
    }
    return p.exitCode();
}

void Application::createConfigDirectory()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    if (!QDir().mkpath(configDir))
        qDebug() << "Could not create config directory XDG_CONFIG_HOME: " << configDir;
}

void Application::initLanguage()
{
    // en_US
    QString str = "en_US.UTF-8";

    const auto lcValues = {
        "LANG", "LC_NUMERIC", "LC_TIME", "LC_MONETARY", "LC_MEASUREMENT", "LC_COLLATE", "LC_CTYPE"
    };

    for (auto lc : lcValues) {
        const QString value = str;
        if (!value.isEmpty()) {
            qputenv(lc, value.toUtf8());
        }
    }

    const QString value = "en_US";
    if (!value.isEmpty()) {
        qputenv("LANGUAGE", value.toUtf8());
    }
}

void Application::initScreenScaleFactors()
{
    QSettings settings(QSettings::UserScope, "panda", "theme");
    qreal scaleFactor = settings.value("PixelRatio", 1.0).toReal();

    qputenv("QT_SCREEN_SCALE_FACTORS", QByteArray::number(scaleFactor));
    qputenv("QT_SCALE_FACTOR", QByteArray::number(scaleFactor));
    qputenv("GDK_SCALE", QByteArray::number(scaleFactor, 'g', 0));
    qputenv("GDK_DPI_SCALE", QByteArray::number(1.0 / scaleFactor, 'g', 3));
}

void Application::initCursor()
{
    QByteArray path("/usr/share/icons");
    path.replace("$XCURSOR_PATH", qgetenv("XCURSOR_PATH"));
    qputenv("XCURSOR_PATH", path);

    qputenv("XCURSOR_THEME", "breeze_cursors");
}

bool Application::syncDBusEnvironment()
{
    int exitCode;
    // At this point all environment variables are set, let's send it to the DBus session server to update the activation environment
    if (!QStandardPaths::findExecutable(QStringLiteral("dbus-update-activation-environment")).isEmpty()) {
        exitCode = runSync(QStringLiteral("dbus-update-activation-environment"), { QStringLiteral("--systemd"), QStringLiteral("--all") });
    }

    return exitCode == 0;
}
