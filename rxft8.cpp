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

#include <QDebug>
#include <QDataStream>
#include <QProcess>
#include <QFileInfo>
#include <QtEndian>
#include <math.h>
#include <time.h>
#include "rxft8.h"

#define FT8_NSAMPLES 1382400 // 14.400 seconds of 16 bits @ 48 kHz
#define FT8_DURATION 15
#define DOWNSAMPLE_FACTOR 4  // 48 kHz -> 12 kHz

RXFT8::RXFT8(QObject *parent) : QObject(parent)
{
    app = QCoreApplication::instance();
    m_programName = "rxft8 v1.0";
    m_mode = "FT8";
    m_nsamples = FT8_NSAMPLES;
    m_duration = FT8_DURATION;

    // Pre-compile regex patterns for better performance
    m_rxFT8Pattern.setPattern("(^\\d{6})\\s+(-?\\d+)\\s+(-?\\d+\\.\\d+)\\s+(\\d+)\\s+(.)\\s+(\\w+)");
    m_rxCQPattern.setPattern("\\s+(CQ|CQ\\s+[A-Z]{2})\\s+(\\w+)\\s+([A-Z]{2}\\d{2})");
    m_rxGridPattern.setPattern("\\s+(\\w+)\\s+(\\w+)\\s+([A-Z]{2}\\d{2})");
    m_rxCallsignPattern.setPattern("\\d?[A-Z]{1,2}\\d{1,2}[A-Z]{1,3}");
}

void RXFT8::setCallsign(QString callsign)
{
    m_myCall = callsign.toUpper();
}

void RXFT8::setLocator(QString locator)
{
    m_myGrid = locator.toUpper();
}

void RXFT8::setFrequency(QString frequency)
{
    m_baseFrequency = frequency;
}

void RXFT8::setMulticastGroup(QString group)
{
    m_multicastGroup = group;
}

void RXFT8::setMulticastPort(QString port)
{
   m_multicastPort = port;
}

void RXFT8::setInterface(QString interface)
{
    m_interface = interface;
}

void RXFT8::timeTick()
{
    time_t seconds;
    seconds = time(NULL);

    if (!m_recTrigger && seconds % m_duration == 0)
    {
        m_time = seconds;
        m_audioData.clear();
        m_recTrigger = true;
    }
}

// Multicast socket callback
void RXFT8::udpRead()
{
    QByteArray datagram;
    unsigned char *data;

    // Process incoming datagrams
    while (m_udpSocket4.hasPendingDatagrams())
    {
        // Read audio from network datagram
        datagram.resize(m_udpSocket4.pendingDatagramSize());
        m_udpSocket4.readDatagram(datagram.data(), datagram.size());
        data = (unsigned char *)datagram.constData();

        if (m_recTrigger)
        {
            m_audioData.append((const char *)data, datagram.size());
        }

        if (m_recTrigger && m_audioData.size() >= m_nsamples)
        {
            // Calculate required buffer size for downsampled audio
            int downsampledSize = (m_duration * 12000 * 2); // 12kHz, 16-bit samples
            QByteArray pcmAudioData;
            pcmAudioData.reserve(downsampledSize);

            // Downsample from 48 to 12 kHz and prepare buffer of LE16 ints.
            // No filtering is done here since audio is already filtered.
            for (int i = 0; i < m_nsamples && i < m_audioData.size(); i += (2 * DOWNSAMPLE_FACTOR))
            {
                pcmAudioData.append(m_audioData.at(i));
                pcmAudioData.append(m_audioData.at(i+1));
            }

            // Zero-pad to expected size if needed
            while (pcmAudioData.size() < downsampledSize)
            {
                pcmAudioData.append('\x00');
                pcmAudioData.append('\x00');
            }

            // Generate filename based on mode
            m_wavFileName = QDateTime::fromSecsSinceEpoch(m_time, Qt::UTC).toString("yyMMdd_hhmmss") + ".wav";
            m_date = QDateTime::fromSecsSinceEpoch(m_time, Qt::UTC).toString("yyMMdd");

            // Validate filename (prevent path traversal)
            if (m_wavFileName.contains("..") || m_wavFileName.contains("/") || m_wavFileName.contains("\\"))
            {
                qWarning() << "Invalid filename generated:" << m_wavFileName;
                m_recTrigger = false;
                continue;
            }

            // Write WAV file
            QFile f(m_wavFileName);
            if (!f.open(QIODevice::WriteOnly))
            {
                qWarning() << "Failed to open WAV file:" << m_wavFileName;
                m_recTrigger = false;
                continue;
            }

            f.write(m_waveFileHeader);
            f.write(pcmAudioData);
            f.close();

            // Start decoder process
            QStringList args;
            args << "--ft8" << "-d" << "3" << "-L" << "0" << "-H" << "3000" << m_wavFileName;

            m_decoderProcess->start("jt9", args);
            if (!m_decoderProcess->waitForStarted(1000))
            {
                qWarning() << "Failed to start decoder process: jt9" << args.join(" ");
            }

            compute_dB(&pcmAudioData);
            m_recTrigger = false;
        }
    }
}

void RXFT8::run()
{
    m_pskReporter = new PSK_Reporter(this);
    m_pskReporter->setLocalStation(m_myCall, m_myGrid, "Dipole", m_programName);

    // Prepare the wave file header
    QDataStream ds(&m_waveFileHeader, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.writeRawData("RIFF", 4);     // Literal RIFF
    ds << quint32(360000 + 44);     // Total data length
    ds.writeRawData("WAVE", 4);     // Literal WAVE
    ds.writeRawData("fmt ", 4);     // Literal fmt
    ds << quint32(16);              // fmt header length
    ds << quint16(1);               // PCM format
    ds << quint16(1);               // Channel count
    ds << quint32(12000);           // Sample rate
    ds << quint32(24000);           // Data rate
    ds << quint16(2);               // Bytes per sample
    ds << quint16(16);              // SampleBits
    ds.writeRawData("data", 4);     // Literal data
    ds << quint32(360000);          // Data length

    m_decoderProcess = new QProcess(this);
    connect(m_decoderProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(processDecoderResults()));
    connect(m_decoderProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(decoderProcessFinished(int, QProcess::ExitStatus)));

    // Setup Multicast UDP socket for receiving audio samples
    connect(&m_udpSocket4, SIGNAL(readyRead()), this, SLOT(udpRead()));

    if (!m_udpSocket4.bind(QHostAddress::AnyIPv4, m_multicastPort.toInt(), QUdpSocket::ShareAddress))
        qWarning() << "IPv4 Socket bind failed";

    QNetworkInterface iface = QNetworkInterface::interfaceFromName(m_interface);
    if (!iface.isValid())
    {
        qWarning() << "Invalid network interface:" << m_interface;
        qWarning() << "Attempting to join multicast group on default interface";
        if (!m_udpSocket4.joinMulticastGroup(QHostAddress(m_multicastGroup)))
            qWarning() << "IPv4 Multicast join failed";
    }
    else
    {
        if (!m_udpSocket4.joinMulticastGroup(QHostAddress(m_multicastGroup), iface))
            qWarning() << "IPv4 Multicast join failed on interface:" << m_interface;
    }

    m_audioConnectionState = false;
    m_rtpinit = 0;
    m_rtpseq = 0;
    m_recTrigger = false;

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeTick()));
    timer->start(50);
}

void RXFT8::quit()
{
    emit finished();
}

void RXFT8::aboutToQuitApp()
{
    // stop threads
    // sleep(1);   // wait for threads to stop.
}

void RXFT8::processDecoderResults()
{
    QString dxCall;
    QString dxGrid;
    QString dxSnr;
    QString dxFreq;
    QString dxMode;
    QString dxTime;
    QString reportFlag;

    QFile logFile("/var/tmp/rxft8_" + m_baseFrequency + ".log");
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        qWarning() << "Failed to open log file:" << logFile.fileName();
        return;
    }
    QTextStream out(&logFile);

    // Parse decoder results
    QString result = m_decoderProcess->readAllStandardOutput();
    if (result.isEmpty())
    {
        return;
    }

    result = result.mid(0, 50);

    // Use pre-compiled regex pattern for FT8
    if(m_rxFT8Pattern.indexIn(result) != -1)
    {
        dxTime = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
        dxMode = m_mode;
        dxFreq = QString::number(m_baseFrequency.toLongLong() + m_rxFT8Pattern.cap(4).toInt());
        dxSnr = m_rxFT8Pattern.cap(2);

        dxCall.clear();
        if (m_rxFT8Pattern.cap(6) == "CQ")
        {
            // RegExp for a CQ message
            if(m_rxCQPattern.indexIn(result) != -1 && m_rxCQPattern.cap(3) != "RR73")
            {
                dxCall = m_rxCQPattern.cap(2);
                dxGrid = m_rxCQPattern.cap(3);
            }
        }
        else
        {
            // RegExp for a grid message
            if(m_rxGridPattern.indexIn(result) != -1 && m_rxGridPattern.cap(3) != "RR73")
            {
                dxCall = m_rxGridPattern.cap(2);
                dxGrid = m_rxGridPattern.cap(3);
            }
        }

        // If call is a valid callsign, forward spot to PSK Reporter
        if(!dxCall.isEmpty())
        {
            if(m_rxCallsignPattern.indexIn(dxCall) != -1)
            {
                m_pskReporter->addRemoteStation(dxCall, dxGrid, dxFreq, dxMode, dxSnr, dxTime);
                reportFlag = "*";
            }
        }
        qDebug().noquote() << result.remove(QRegExp("[\\n]")) << reportFlag;
        out << m_date + " " << result << reportFlag << "\n";
    }
}

// Cleanup after decoder has finished
void RXFT8::decoderProcessFinished(int code, QProcess::ExitStatus status)
{
    if (code != 0 || status != QProcess::NormalExit)
    {
        qWarning() << "Decoder process finished with error code:" << code << "status:" << status;
    }

    if (!QFileInfo::exists("keepwav"))
    {
        if (!QFile::remove(m_wavFileName))
        {
            qWarning() << "Failed to remove WAV file:" << m_wavFileName;
        }
    }
}

// Compute power of audio buffer
void RXFT8::compute_dB(QByteArray *data)
{
    if (data->size() < 2)
    {
        qWarning() << "Audio data too small for power computation";
        return;
    }

    const int16_t *samples = reinterpret_cast<const int16_t*>(data->constData());
    int numSamples = data->size() / 2;

    double sum = 0.0;
    for (int i = 0; i < numSamples; i++)
    {
        int16_t sample = qFromLittleEndian(samples[i]);
        sum += static_cast<double>(sample) * sample;
    }

    if (sum > 0)
    {
        m_rawPower = 10.0 * log10(sum / numSamples);
    }
    else
    {
        m_rawPower = -100.0; // Very low power indicator
    }

    QFile logFile("/var/tmp/rxft8_" + m_baseFrequency + "_pwr.log");
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        qWarning() << "Failed to open power log file:" << logFile.fileName();
        return;
    }

    QTextStream out(&logFile);
    out << m_wavFileName.mid(0,13) << QString(" %1\n").arg(m_rawPower, 0, 'f', 1, '0');
}
