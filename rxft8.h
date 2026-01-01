/*
 * This file is part of rxft8.
 *
 * Copyright (C) 2018 by Edson Pereira, PY2SDR
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RXFT8_H
#define RXFT8_H

#include <QObject>
#include <QCoreApplication>
#include <QUdpSocket>
#include <QTime>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <QRegExp>
#include <QNetworkInterface>
#include <time.h>
#include "psk_reporter.h"

class RXFT8 : public QObject
{
    Q_OBJECT
public:
    explicit RXFT8(QObject *parent = 0);

signals:
    void finished();

public slots:
    void setCallsign(QString callsign);
    void setLocator(QString locator);
    void setFrequency(QString frequency);
    void setMulticastGroup(QString group);
    void setMulticastPort(QString port);
    void setInterface(QString interface);
    void run();
    void timeTick();
    void aboutToQuitApp();
    void quit();

private:
    QCoreApplication *app;
    QString m_myCall;
    QString m_myGrid;
    QString m_baseFrequency;
    QString m_multicastGroup;
    QString m_multicastPort;
    QString m_interface;
    QString m_programName;
    QString m_mode;
    int m_duration;
    int m_nsamples;
    QString m_date;

    QUdpSocket m_udpSocket4;
    QByteArray m_audioData;
    QFile m_audioFile;
    QByteArray m_waveFileHeader;
    QString m_wavFileName;

    QProcess *m_decoderProcess;
    QTimer *m_spectrumTimer;
    PSK_Reporter *m_pskReporter;

    QHash<QString,QString> m_decoder;

    // Pre-compiled regex pattern for FT8 parsing
    QRegExp m_rxFT8Pattern;
    QRegExp m_rxCQPattern;
    QRegExp m_rxGridPattern;
    QRegExp m_rxCallsignPattern;

    qint64 m_datagramCount;
    quint16 m_rtpseq;
    bool m_rtpinit;
    bool m_recTrigger;
    time_t m_time;
    bool m_audioConnectionState;
    float m_rawPower;

    void compute_dB(QByteArray *data);

private slots:
    void udpRead();
    void processDecoderResults();
    void decoderProcessFinished(int code, QProcess::ExitStatus status);
};

#endif // RXFT8_H
