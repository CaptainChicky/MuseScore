//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2020 MuseScore BVBA and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================
#include "workspacefile.h"

#include <algorithm>

#include <QFile>
#include <QBuffer>

#include "log.h"
#include "stringutils.h"

#include "thirdparty/qzip/qzipreader_p.h"
#include "thirdparty/qzip/qzipwriter_p.h"

#include "global/xmlreader.h"
#include "global/xmlwriter.h"

using namespace mu;
using namespace mu::workspace;
using namespace mu::framework;

WorkspaceFile::WorkspaceFile(const io::path& filepath)
    : m_filepath(filepath)
{}

QByteArray WorkspaceFile::readRootFile()
{
    QFile f(m_filepath.toQString());
    if (!f.open(QIODevice::ReadOnly)) {
        LOGE() << "failed open file: " << m_filepath;
        return QByteArray();
    }

    QByteArray data = f.readAll();

    QBuffer buf(&data);
    MQZipReader zip(&buf);

    std::string rootfile;
    MetaInf meta;
    if (meta.read(zip)) {
        rootfile = meta.rootfile();
    } else {
        QVector<MQZipReader::FileInfo> fis = zip.fileInfoList();
        if (!fis.isEmpty()) {
            rootfile = fis.first().filePath.toStdString();
        }
    }

    if (rootfile.empty()) {
        LOGE() << "not found root file: " << m_filepath;
        return QByteArray();
    }

    QByteArray fileData = zip.fileData(QString::fromStdString(rootfile));
    if (fileData.isEmpty()) {
        LOGE() << "failed read root file: " << m_filepath;
        return QByteArray();
    }

    return fileData;
}

bool WorkspaceFile::writeRootFile(const std::string& name, const QByteArray& file)
{
    QByteArray data;
    QBuffer buf(&data);
    MQZipWriter zip(&buf);

    MetaInf meta;
    meta.setRootfile(name);
    zip.addFile(QString::fromStdString(name), file);
    meta.write(zip);

    if (zip.status() != MQZipWriter::NoError) {
        LOGE() << "failed add mscz, zip status: " << zip.status();
        return false;
    }

    QFile f(m_filepath.toQString());
    if (!f.open(QIODevice::WriteOnly)) {
        LOGE() << "failed open file: " << m_filepath;
        return false;
    }

    if (f.write(data) == 0) {
        LOGE() << "failed write file: " << m_filepath;
        return false;
    }

    return true;
}

// === MetaInf ===

void WorkspaceFile::MetaInf::setRootfile(const ::std::string& name)
{
    m_rootfile = name;
}

std::string WorkspaceFile::MetaInf::rootfile() const
{
    return m_rootfile;
}

void WorkspaceFile::MetaInf::write(MQZipWriter& zip)
{
    QByteArray data;
    writeContainer(&data);
    zip.addFile("META-INF/container.xml", data);
}

bool WorkspaceFile::MetaInf::read(const MQZipReader& zip)
{
    QByteArray container = zip.fileData("META-INF/container.xml");
    if (container.isEmpty()) {
        LOGE() << "not found META-INF/container.xml";
        return false;
    }

    readContainer(container);
    if (m_rootfile.empty()) {
        return false;
    }

    return true;
}

void WorkspaceFile::MetaInf::readContainer(const QByteArray& data)
{
    XmlReader xml(data);
    while (xml.readNextStartElement()) {
        if ("container" != xml.tagName()) {
            xml.skipCurrentElement();
            continue;
        }

        while (xml.readNextStartElement()) {
            if ("rootfiles" != xml.tagName()) {
                xml.skipCurrentElement();
                continue;
            }

            while (xml.readNextStartElement()) {
                if ("rootfile" != xml.tagName()) {
                    xml.skipCurrentElement();
                    continue;
                }

                m_rootfile = xml.attribute("full-path");
                return;
            }
        }
    }
}

void WorkspaceFile::MetaInf::writeContainer(QByteArray* data) const
{
    IF_ASSERT_FAILED(data) {
        return;
    }

    QBuffer buffer(data);
    buffer.open(QBuffer::WriteOnly);

    XmlWriter xml(&buffer);
    xml.writeStartDocument();
    xml.writeStartElement("container");
    xml.writeStartElement("rootfiles");

    xml.writeStartElement("rootfile");
    xml.writeAttribute("full-path", m_rootfile);
    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    buffer.close();
}
