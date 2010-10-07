/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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


/* Insides of MixologistLib interface.
 * only prints stuff out at the moment
 */

#include "interface/types.h"
#include <iostream>
#include <sstream>
#include <iomanip>


/**********************************************************************
 * NOTE NOTE NOTE ...... XXX
 * BUG in MinGW .... %hhx in sscanf overwrites 32bits, instead of 8bits.
 * this means that scanf(.... &(data[15])) is running off the
 * end of the buffer, and hitting data[15-18]...
 * To work around this bug we are reading into proper int32s
 * and then copying the data over...
 *
**********************************************************************/


std::ostream &operator<<(std::ostream &out, const FileInfo &info) {
    out << "FileInfo: path: " << info.paths.at(0).toStdString();
    out << std::endl;
    out << " Size: " << info.size;
    out << std::endl;
    out << "Hash: " << info.hash;
    return out;
}


