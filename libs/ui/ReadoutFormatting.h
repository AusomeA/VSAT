#pragma once
#include <QString>

inline QString MissionElapsedTimeText(double METSeconds) 
{
    QString METText = QString("T+ ");

    int totalSeconds = static_cast<int>(METSeconds); // will wrap around at ~ 68 years (~2 billion seconds)

    int days = totalSeconds / 86400;
    int hours = (totalSeconds % 86400) / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    return METText + QString("%1:%2:%3:%4")
                         .arg(days, 3, 10, QChar('0'))
                         .arg(hours, 2, 10, QChar('0'))
                         .arg(minutes, 2, 10, QChar('0'))
                         .arg(seconds, 2, 10, QChar('0'));
}

inline QString CountdownText(float seconds)
{
    const int totalSeconds = static_cast<int> (seconds);

    return QString("%1:%2")
        .arg(totalSeconds / 60, 2, 10, QChar('0'))
        .arg(totalSeconds % 60, 2, 10, QChar('0'));
}