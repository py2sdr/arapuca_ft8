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

#ifndef PSK_REPORTER_H
#define PSK_REPORTER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostInfo>
#include <QTimer>
#include <QDateTime>
#include <QQueue>

class PSK_Reporter : public QObject
{
    Q_OBJECT

public:
    explicit PSK_Reporter(QObject *parent = 0);
    void setLocalStation(QString call, QString grid, QString antenna, QString programInfo);
    void addRemoteStation(QString call, QString grid, QString freq, QString mode, QString snr, QString time);

signals:

public slots:
    void sendReport();

private slots:
    void dnsLookupResult(QHostInfo info);

private:
    QString m_header_h;
    QString m_rxInfoDescriptor_h;
    QString m_txInfoDescriptor_h;
    QString m_randomId_h;
    QString m_linkId_h;

    QString m_rxCall;
    QString m_rxGrid;
    QString m_rxAnt;
    QString m_progId;

    QHostAddress m_pskReporterAddress;

    QQueue< QHash<QString,QString> > m_spotQueue;

    QUdpSocket *m_udpSocket;

    QTimer *reportTimer;

    int m_sequenceNumber;
};

#endif // PSK_REPORTER_H
