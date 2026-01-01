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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include "rxft8.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("PY2SDR FT8 Receiver");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption callsign(QStringList() << "c" << "callsign", "Station callsign", "callsign", "");
    QCommandLineOption locator(QStringList() << "l" << "locator", "Grid locator", "locator", "");
    QCommandLineOption frequency(QStringList() << "f" << "freq", "RX Frequency in Hz", "freq", "");
    QCommandLineOption multicastGroup(QStringList() << "g" << "group", "Multicast group", "group", "");
    QCommandLineOption multicastPort(QStringList() << "p" << "port", "Multicast port", "port", "");
    QCommandLineOption interface(QStringList() << "i" << "interface", "Network interface", "interface", "");

    parser.addOption(callsign);
    parser.addOption(locator);
    parser.addOption(frequency);
    parser.addOption(multicastGroup);
    parser.addOption(multicastPort);
    parser.addOption(interface);
    parser.process(a);

    // Check callsign
    if (parser.value(callsign).isEmpty())
    {
        qInfo().noquote() << "Invalid callsign";
        parser.showHelp();
        a.quit();
    }

    // Check grid locator
    if (parser.value(locator).isEmpty())
    {
        qInfo().noquote() << "Invalid QTH locator";
        parser.showHelp();
        a.quit();
    }

    // Check frequency
    if (parser.value(frequency).isEmpty())
    {
        qInfo().noquote() << "Invalid frequency";
        parser.showHelp();
        a.quit();
    }

    // Check multicast group
    if (parser.value(multicastGroup).isEmpty())
    {
        qInfo().noquote() << "Invalid multicast group";
        parser.showHelp();
        a.quit();
    }

    // Check multicast port
    if (parser.value(multicastPort).isEmpty())
    {
        qInfo().noquote() << "Invalid multicast port";
        parser.showHelp();
        a.quit();
    }

    // Check interface
    if (parser.value(interface).isEmpty())
    {
        qInfo().noquote() << "Invalid network interface";
        parser.showHelp();
        a.quit();
    }

    RXFT8 rxft8;
    rxft8.setCallsign(parser.value(callsign));
    rxft8.setLocator(parser.value(locator));
    rxft8.setFrequency(parser.value(frequency));
    rxft8.setMulticastGroup(parser.value(multicastGroup));
    rxft8.setMulticastPort(parser.value(multicastPort));
    rxft8.setInterface(parser.value(interface));

    QObject::connect(&rxft8, SIGNAL(finished()), &a, SLOT(quit()));
    QObject::connect(&a, SIGNAL(aboutToQuit()), &rxft8, SLOT(aboutToQuitApp()));

    QTimer::singleShot(10, &rxft8, SLOT(run()));

    return a.exec();
}

