/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2005, Thomas Bernard
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

#ifndef MINIUPNP_UTIL_H_
#define MINIUPNP_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#define snprintf _snprintf
#endif
#include <miniwget.h>
#include <miniupnpc.h>
#include <upnpcommands.h>

/* protofix() checks if protocol is "UDP" or "TCP"
 * returns NULL if not */
const char *protofix(const char *proto);
void DisplayInfos(struct UPNPUrls *urls,
                  struct IGDdatas *data);

void GetConnectionStatus(struct UPNPUrls *urls,
                         struct IGDdatas *data);

void ListRedirections(struct UPNPUrls *urls,
                      struct IGDdatas *data);

bool SetRedirectAndTest(struct UPNPUrls *urls,
                        struct IGDdatas *data,
                        const char *iaddr,
                        const char *iport,
                        const char *eport,
                        const char *proto);

bool TestRedirect(struct UPNPUrls *urls,
                  struct IGDdatas *data,
                  const char *iaddr,
                  const char *iport,
                  const char *eport,
                  const char *proto);

bool RemoveRedirect(struct UPNPUrls *urls,
                    struct IGDdatas *data,
                    const char *eport,
                    const char *proto);

/* EOF */
#endif
