/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include <cstdlib>

#include <QDebug>
#include <QProcess>
#include <QScopedPointer>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#include <QThread>

#ifndef DISABLE_GUI
// GUI-only includes
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSplashScreen>

#ifdef QBT_STATIC_QT
#include <QtPlugin>
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // QBT_STATIC_QT

#else
// NoGUI-only includes
#include <cstdio>
#ifdef Q_OS_UNIX
#include "unistd.h"
#endif
#endif // DISABLE_GUI

#include <signal.h>
#ifdef STACKTRACE
#ifdef Q_OS_UNIX
#include "stacktrace.h"
#else
#include "stacktrace_win.h"
#include "stacktracedialog.h"
#endif // Q_OS_UNIX
#endif //STACKTRACE

#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/misc.h"
#include "application.h"
#include "cmdoptions.h"
#include "upgrade.h"

#if defined(Q_OS_MAC)
#include <sys/sysctl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <cassert>
#endif

// Signal handlers
void sigNormalHandler(int signum);
#ifdef STACKTRACE
void sigAbnormalHandler(int signum);
#endif
// sys_signame[] is only defined in BSD
const char *sysSigName[] = {
#if defined(Q_OS_WIN)
    "", "", "SIGINT", "", "SIGILL", "", "SIGABRT_COMPAT", "", "SIGFPE", "",
    "", "SIGSEGV", "", "", "", "SIGTERM", "", "", "", "",
    "", "SIGBREAK", "SIGABRT", "", "", "", "", "", "", "",
    "", ""
#else
    "", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
    "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
    "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
    "SIGPWR", "SIGUNUSED"
#endif
};

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char *str);
#endif

void displayVersion();
bool userAgreesWithLegalNotice();
void displayBadArgMessage(const QString &message);

#if !defined(DISABLE_GUI)
void showSplashScreen();
#endif  // DISABLE_GUI

#if defined(Q_OS_MAC)
/// code from https://gist.github.com/konstantinwirz/5450970
/// returns the maximum size of argument in bytes
/// returns 0 if failed
unsigned int get_max_arguments_size() {
    int mib[2] = {CTL_KERN, KERN_ARGMAX};
    unsigned int result = 0;
    std::size_t size =  sizeof(result);
    if(sysctl(mib, 2, &result, &size, nullptr, 0) == -1)
        perror("sysctl");
    return result;
}

/// returns the command line arguments of a process with given pid
/// if failed - returns an empty vector
QString get_process_arguments(pid_t pid) {
    std::vector<std::string> arguments;
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, pid};
    size_t max_arguments_number = get_max_arguments_size();
    assert(max_arguments_number);
    char *buffer = new char[max_arguments_number];
    assert(buffer);

    if (sysctl(mib, 3, buffer, &max_arguments_number, nullptr, 0) == -1) {
        perror("sysctl");
    } else {
        // first element in buffer is argc
        const int real_arguments_number = static_cast<int>(*buffer);
        std::string word;
        // elements are '\0' separated
        for(int i=0; i<max_arguments_number; ++i) {
            if(isprint(buffer[i])) {
                word.push_back(buffer[i]);
            } else {
                if(!word.empty()) {
                    arguments.push_back(word);
                    word.clear();
                }
            }
        }
        // first arg is exec_path - skip it
        // we need the next real_arguments_number arguments
        if(arguments.size()>=real_arguments_number+1) {
            arguments = std::vector<std::string>(arguments.begin() + 1,
                                                 arguments.begin() + 1 + real_arguments_number);
        } else { // something is wrong(not enough elements in vector) - clear it
            arguments.clear();
        }
    }

    std::ostringstream args;
    std::copy(arguments.begin(), arguments.end(), std::ostream_iterator<std::string>(args, "^@"));

    return QString::fromStdString(args.str());
}
#endif

// Main
int main(int argc, char *argv[])
{
    // We must save it here because QApplication constructor may change it
    bool isOneArg = (argc == 2);

#ifdef Q_OS_MAC
    // On macOS 10.12 Sierra, Apple changed the behaviour of CFPreferencesSetValue() https://bugreports.qt.io/browse/QTBUG-56344
    // Due to this, we have to move from native plist to IniFormat
    macMigratePlists();
#endif

    try {
        // Create Application
        QString appId = QLatin1String("qBittorrent-") + Utils::Misc::getUserIDString();
        QScopedPointer<Application> app(new Application(appId, argc, argv));

#ifndef DISABLE_GUI
        // after the application object creation because we need a profile to be set already
        // for the migration
        migrateRSS();
#endif

        const QBtCommandLineParameters params = app->commandLineArgs();

        if (!params.unknownParameter.isEmpty()) {
            throw CommandLineParameterError(QObject::tr("%1 is an unknown command line parameter.",
                                                        "--random-parameter is an unknown command line parameter.")
                                                        .arg(params.unknownParameter));
        }
#ifndef Q_OS_WIN
        if (params.showVersion) {
            if (isOneArg) {
                displayVersion();
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                     .arg(QLatin1String("-v (or --version)")));
        }
#endif
        if (params.showHelp) {
            if (isOneArg) {
                displayUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(QLatin1String("-h (or --help)")));
        }

        // Set environment variable
        if (!qputenv("QBITTORRENT", QBT_VERSION))
            fprintf(stderr, "Couldn't set environment variable...\n");

#ifndef DISABLE_GUI
        if (!userAgreesWithLegalNotice())
            return EXIT_SUCCESS;
#else
        if (!params.shouldDaemonize
            && isatty(fileno(stdin))
            && isatty(fileno(stdout))
            && !userAgreesWithLegalNotice())
            return EXIT_SUCCESS;
#endif

        // Check if qBittorrent is already running for this configuration
        if (app->isRunning()) {
            qDebug("qBittorrent is already running for this user, trying to open new qBt instance.");

            QThread::msleep(300);
            bool isRunning = false;
            QStringList qBitList;
            QRegExp arg("--configuration=(.+)");

            QProcess process;
            process.setReadChannel(QProcess::StandardOutput);
            process.setProcessChannelMode(QProcess::MergedChannels);

    #if defined(Q_OS_WIN)
            process.start("wmic /OUTPUT:STDOUT process where \"name like '%qbittorrent%'\" get ProcessID /format:list"); // use wmic to get qbittorrent process id
            process.waitForFinished();

            QString list = QString(process.readAll());
            QRegExp winArg("--configuration=\"([^\"]*)\"");
            QStringList pidList = list.remove("ProcessId=").split("\r\r\n");
            pidList.removeAll(QString(""));

            foreach (QString pid, pidList) {
                if (QString::number(app->applicationPid()) == pid) continue;
                process.start("wmic /OUTPUT:STDOUT process where handle='"+pid+"' get CommandLine /format:list"); // use wmic to get qbittorrent command line
                process.waitForFinished();
                qBitList.append(QString(process.readAll()).remove("CommandLine=").remove("\r\r\n"));
            }
    #else
            process.start("sh", QStringList() << "-c" << "ps -ax | grep '[q]bittorrent' | awk '{ print $1 }'");
            process.waitForFinished();

            QString list = QString(process.readAll());
            QStringList pidList = list.split("\n");
            pidList.removeLast(); // remove empty line

            foreach (QString pid, pidList) {
                if (QString::number(app->applicationPid()) == pid) continue;
                #if defined(Q_OS_MAC)
                    QString args = get_process_arguments(pid.toInt());
                    qBitList.append(args);
                #else
                    process.start("sh", QStringList() << "-c" << "cat -v /proc/"+pid+"/cmdline");
                    process.waitForFinished();
                    qBitList.append(QString(process.readAll()));
                #endif
            }
    #endif

            // Check configuration name first if the system is Windows
    #if defined(Q_OS_WIN)
            process.start("wmic /OUTPUT:STDOUT process where handle='"+QString::number(app->applicationPid())+"' get CommandLine /format:list"); // use wmic to get qbittorrent command line
            process.waitForFinished();
            QString cmd = QString(process.readAll());
            if (cmd.contains("configuration=")) {
                arg.indexIn(cmd);
                if (arg.cap(1).at(0) != '"') {
                    throw CommandLineParameterError(QObject::tr("configuration name must be included with \"\""));
                }
            }
    #endif

            foreach (QString qb, qBitList) {
                arg.indexIn(qb);
                QString cfgArg = arg.cap(1);
                if (cfgArg != "") {
    #if defined(Q_OS_WIN)
                        if (cfgArg.at(0) != '"') {
                            throw CommandLineParameterError(QObject::tr("configuration name must be included with \"\""));
                        } else {
                            winArg.indexIn(qb);
                            cfgArg = winArg.cap(1);
                        }
    #endif
                    if (cfgArg.indexOf("^@") != -1) {
                        cfgArg.remove(cfgArg.indexOf("^@"), cfgArg.size()-cfgArg.indexOf("^@"));
                    }
                    if (cfgArg == params.configurationName) {
                        isRunning = true;
                    }
                } else {
                    if (params.configurationName == "") {
                        isRunning = true;
                    }
                }
            }

            if (params.configurationName == "") {
                app->sendParams(params.paramList());
            }

            if (isRunning) {
                return EXIT_SUCCESS;
            }
        }

#if defined(Q_OS_WIN)
        // This affects only Windows apparently and Qt5.
        // When QNetworkAccessManager is instantiated it regularly starts polling
        // the network interfaces to see what's available and their status.
        // This polling creates jitter and high ping with wifi interfaces.
        // So here we disable it for lack of better measure.
        // It will also spew this message in the console: QObject::startTimer: Timers cannot have negative intervals
        // For more info see:
        // 1. https://github.com/qbittorrent/qBittorrent/issues/4209
        // 2. https://bugreports.qt.io/browse/QTBUG-40332
        // 3. https://bugreports.qt.io/browse/QTBUG-46015

        qputenv("QT_BEARER_POLL_TIMEOUT", QByteArray::number(-1));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        // this is the default in Qt6
        app->setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#endif // Q_OS_WIN

#if defined(Q_OS_MAC)
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        QByteArray path = "/usr/local/bin:";
        path += qgetenv("PATH");
        qputenv("PATH", path.constData());

        // On OS X the standard is to not show icons in the menus
        app->setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

#ifndef DISABLE_GUI
        if (!upgrade()) return EXIT_FAILURE;
#else
        if (!upgrade(!params.shouldDaemonize
                     && isatty(fileno(stdin))
                     && isatty(fileno(stdout)))) return EXIT_FAILURE;
#endif
#ifdef DISABLE_GUI
        if (params.shouldDaemonize) {
            app.reset(); // Destroy current application
            if (daemon(1, 0) == 0) {
                app.reset(new Application(appId, argc, argv));
                if (app->isRunning()) {
                    // Another instance had time to start.
                    return EXIT_FAILURE;
                }
            }
            else {
                qCritical("Something went wrong while daemonizing, exiting...");
                return EXIT_FAILURE;
            }
        }
#else
        if (!(params.noSplash || Preferences::instance()->isSplashScreenDisabled()))
            showSplashScreen();
#endif

        signal(SIGINT, sigNormalHandler);
        signal(SIGTERM, sigNormalHandler);
#ifdef STACKTRACE
        signal(SIGABRT, sigAbnormalHandler);
        signal(SIGSEGV, sigAbnormalHandler);
#endif

        return app->exec(params.paramList());
    }
    catch (CommandLineParameterError &er) {
        displayBadArgMessage(er.messageForUser());
        return EXIT_FAILURE;
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char *str)
{
    const size_t strLen = strlen(str);
    if (write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen)) {
        auto dummy = write(STDOUT_FILENO, str, strLen);
        Q_UNUSED(dummy);
    }
}
#endif

void sigNormalHandler(int signum)
{
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char msg1[] = "Catching signal: ";
    const char msg2[] = "\nExiting cleanly\n";
    reportToUser(msg1);
    reportToUser(sysSigName[signum]);
    reportToUser(msg2);
#endif // !defined Q_OS_WIN && !defined Q_OS_HAIKU
    signal(signum, SIG_DFL);
    qApp->exit();  // unsafe, but exit anyway
}

#ifdef STACKTRACE
void sigAbnormalHandler(int signum)
{
    const char *sigName = sysSigName[signum];
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char msg[] = "\n\n*************************************************************\n"
        "Please file a bug report at http://bug.qbittorrent.org and provide the following information:\n\n"
        "qBittorrent version: " QBT_VERSION "\n\n"
        "Caught signal: ";
    reportToUser(msg);
    reportToUser(sigName);
    reportToUser("\n");
    print_stacktrace();  // unsafe
#endif

#if defined Q_OS_WIN
    StacktraceDialog dlg;  // unsafe
    dlg.setStacktraceString(QLatin1String(sigName), straceWin::getBacktrace());
    dlg.exec();
#endif

    signal(signum, SIG_DFL);
    raise(signum);
}
#endif // STACKTRACE

#if !defined(DISABLE_GUI)
void showSplashScreen()
{
    QPixmap splashImg(":/icons/skin/splash.png");
    QPainter painter(&splashImg);
    QString version = QBT_VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
    QSplashScreen *splash = new QSplashScreen(splashImg);
    splash->show();
    QTimer::singleShot(1500, splash, &QObject::deleteLater);
    qApp->processEvents();
}
#endif  // DISABLE_GUI

void displayVersion()
{
    printf("%s %s\n", qUtf8Printable(qApp->applicationName()), QBT_VERSION);
}

void displayBadArgMessage(const QString &message)
{
    QString help = QObject::tr("Run application with -h option to read about command line parameters.");
#ifdef Q_OS_WIN
    QMessageBox msgBox(QMessageBox::Critical, QObject::tr("Bad command line"),
                       message + QLatin1Char('\n') + help, QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
#else
    const QString errMsg = QObject::tr("Bad command line: ") + '\n'
        + message + '\n'
        + help + '\n';
    fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
}

bool userAgreesWithLegalNotice()
{
    Preferences *const pref = Preferences::instance();
    if (pref->getAcceptedLegal()) // Already accepted once
        return true;

#ifdef DISABLE_GUI
    const QString eula = QString("\n*** %1 ***\n").arg(QObject::tr("Legal Notice"))
        + QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.") + "\n\n"
        + QObject::tr("No further notices will be issued.") + "\n\n"
        + QObject::tr("Press %1 key to accept and continue...").arg("'y'") + '\n';
    printf("%s", qUtf8Printable(eula));

    char ret = getchar(); // Read pressed key
    if ((ret == 'y') || (ret == 'Y')) {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#else
    QMessageBox msgBox;
    msgBox.setText(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(QObject::tr("Legal notice"));
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
    QAbstractButton *agreeButton = msgBox.addButton(QObject::tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
    if (msgBox.clickedButton() == agreeButton) {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#endif // DISABLE_GUI

    return false;
}
