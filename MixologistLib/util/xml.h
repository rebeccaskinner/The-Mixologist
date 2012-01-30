/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-7, Robert Fernie
 *
 *  This file is part of the Mixologist.
 *
 *  The Mixologist is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  The Mixologist is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the Mixologist; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/


#ifndef UTIL_XML_H
#define UTIL_XML_H

#include <QDomDocument>
#include <QIODevice>

/**********************************************************************************
 * QT's XML handling is extremely generalized, over generalized for what we need, making code ugly and awkward.
 * This util is to make dealing with XML with QT a little less painful and a little more high level.
 **********************************************************************************/

namespace XmlUtil {

    /* Opens the specified share xml file, and populates xml and rootNode.
       If expectedRootName is not what is found, will return false.
       The source can be provided with or without the user's directory pre-pended.
       If with, it must be supplied with native separators.
       If without, then the user's directory will be automatically added. */
    bool openXml(QIODevice &source, QDomDocument &xml, QDomElement &rootNode, const QString &expectedRootName, QIODevice::OpenModeFlag readOrWrite);
    bool openXml(const QString &file, QDomDocument &xml, QDomElement &rootNode, const QString &expectedRootName, QIODevice::OpenModeFlag readOrWrite);

    /* Writes xml to the xml file, replacing any existing contents.
       The source can be provided with or without the user's directory pre-pended.
       If with, it must be supplied with native separators.
       If without, then the user's directory will be automatically added. */
    bool writeXml(const QString &file, QDomDocument &xml);

    /* Creates an element elementName in xml, but does not attach it to any node.  If elementText is supplied, that will be set, otherwise it will be an empty element. */
    QDomElement createElement(QDomDocument &xml, const QString &elementName, const QString &elementText = QString());

    /* Changes the targetNode's text to equal newText. Returns a null node on failure or a non-text targetNode. */
    bool changeText(QDomNode &targetNode, const QString &newText);

}
#endif
