/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-2008, Robert Fernie
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

#ifndef BASE64_CODE_H
#define BASE64_CODE_H

#include <string>

std::string convertToBase64(std::string input);
std::string convertFromBase64(std::string input);


uint32_t DataLenFromBase64(std::string input);
std::string convertDataToBase64(unsigned char *data, uint32_t dlen);
bool convertDataFromBase64(std::string input, unsigned char *data, uint32_t *dlen);


#endif

