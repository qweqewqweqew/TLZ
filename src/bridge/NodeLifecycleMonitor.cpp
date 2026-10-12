#include "bridge/NodeLifecycleMonitor.h"

#include <QSet>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

NodeLifecycleMonitor::NodeLifecycleMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &NodeLifecycleMonitor::poll);
    poll();
    m_timer->start();
}

void NodeLifecycleMonitor::pollNow()
{
    poll();
}

void NodeLifecycleMonitor::poll()
{
    const bool running = allTargetNodesRunning();
    if (running == m_nodesRunning) {
        return;
    }
    m_nodesRunning = running;
    emit nodesRunningChanged(running);
}

bool NodeLifecycleMonitor::allTargetNodesRunning()
{
#ifdef Q_OS_WIN
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    QSet<QString> names;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            names.insert(QString::fromWCharArray(entry.szExeFile).toLower());
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    const bool algorithmAlive = names.contains(QStringLiteral("copper_node.exe"))
                                || names.contains(QStringLiteral("copper_node_core.exe"));
    const bool cameraAlive = names.contains(QStringLiteral("camera_node.exe"));
    const bool backendAlive = names.contains(QStringLiteral("milling_controller.exe"));
    return algorithmAlive && cameraAlive && backendAlive;
#else
    return false;
#endif
}
