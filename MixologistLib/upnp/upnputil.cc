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

#include "upnp/upnputil.h"
/***
 * #define DEBUGUPNPUTIL
 **/

/* protofix() checks if protocol is "UDP" or "TCP"
 * returns NULL if not */
const char *protofix(const char *proto) {
    static const char proto_tcp[4] = { 'T', 'C', 'P', 0};
    static const char proto_udp[4] = { 'U', 'D', 'P', 0};
    int i, b;
    for(i=0, b=1; i<4; i++)
        b = b && (   (proto[i] == proto_tcp[i])
                     || (proto[i] == (proto_tcp[i] | 32)) );
    if(b)
        return proto_tcp;
    for(i=0, b=1; i<4; i++)
        b = b && (   (proto[i] == proto_udp[i])
                     || (proto[i] == (proto_udp[i] | 32)) );
    if(b)
        return proto_udp;
    return 0;
}

void DisplayInfos(struct UPNPUrls *urls,
                  struct IGDdatas *data) {
    char externalIPAddress[16];
    char connectionType[64];
    char status[64];
    char lastconnerror[64];
    unsigned int uptime;
    unsigned int brUp, brDown;
    UPNP_GetConnectionTypeInfo(urls->controlURL,
                               //data->first.servicetype,
                               data->servicetype,
                               connectionType);
#ifdef DEBUGUPNPUTIL
    if(connectionType[0])
        printf("Connection Type : %s\n", connectionType);
    else
        printf("GetConnectionTypeInfo failed.\n");
#endif
    UPNP_GetStatusInfo(urls->controlURL,
                       //data->first.servicetype,
                       data->servicetype,
                       status, &uptime, lastconnerror);
#ifdef DEBUGUPNPUTIL
    printf("Status : %s, uptime=%u LastConnError %s\n", status, uptime, lastconnerror);
    printf("MaxBitRateDown : %u bps   MaxBitRateUp %u bps\n", brDown, brUp);
#endif
    UPNP_GetLinkLayerMaxBitRates(urls->controlURL_CIF,
                                 data->servicetype, //data->servicetype,
                                 &brDown, &brUp);
    UPNP_GetExternalIPAddress(urls->controlURL,
                              //data->first.servicetype,
                              data->servicetype,
                              externalIPAddress);
#ifdef DEBUGUPNPUTIL
    if(externalIPAddress[0])
        printf("ExternalIPAddress = %s\n", externalIPAddress);
    else
        printf("GetExternalIPAddress failed.\n");
#endif
}

void GetConnectionStatus(struct UPNPUrls *urls,
                         struct IGDdatas *data) {
    unsigned int bytessent, bytesreceived, packetsreceived, packetssent;
    DisplayInfos(urls, data);
    bytessent = UPNP_GetTotalBytesSent(urls->controlURL_CIF, data->servicetype);//data->CIF.servicetype);
    bytesreceived = UPNP_GetTotalBytesReceived(urls->controlURL_CIF, data->servicetype);//data->CIF.servicetype);
    packetssent = UPNP_GetTotalPacketsSent(urls->controlURL_CIF, data->servicetype);//data->CIF.servicetype);
    packetsreceived = UPNP_GetTotalPacketsReceived(urls->controlURL_CIF, data->servicetype);//data->CIF.servicetype);
#ifdef DEBUGUPNPUTIL
    printf("Bytes:   Sent: %8u\tRecv: %8u\n", bytessent, bytesreceived);
    printf("Packets: Sent: %8u\tRecv: %8u\n", packetssent, packetsreceived);
#endif
}

void ListRedirections(struct UPNPUrls *urls,
                      struct IGDdatas *data) {
    int r;
    int i = 0;
    char index[6];
    char intClient[16];
    char intPort[6];
    char extPort[6];
    char protocol[4];
    char desc[80];
    char enabled[6];
    char rHost[64];
    char duration[16];
    /*unsigned int num=0;
        UPNP_GetPortMappingNumberOfEntries(urls->controlURL, data->first.servicetype, &num);
    printf("PortMappingNumberOfEntries : %u\n", num);*/
    do {
        snprintf(index, 6, "%d", i);
        rHost[0] = '\0';
        enabled[0] = '\0';
        duration[0] = '\0';
        desc[0] = '\0';
        extPort[0] = '\0';
        intPort[0] = '\0';
        intClient[0] = '\0';
        r = UPNP_GetGenericPortMappingEntry(urls->controlURL,
                                            //data->first.servicetype,
                                            data->servicetype,
                                            index, extPort, intClient, intPort,
                                            protocol, desc, enabled,
                                            rHost, duration);
        if(r==0)
#ifdef DEBUGUPNPUTIL
            printf("%02d - %s %s->%s:%s\tenabled=%s leaseDuration=%s\n"
                   "     desc='%s' rHost='%s'\n",
                   i, protocol, extPort, intClient, intPort,
                   enabled, duration,
                   desc, rHost);
#endif
        i++;
    } while(r==0);
}

/* Test function
 * 1 - get connection type
 * 2 - get extenal ip address
 * 3 - Add port mapping
 * 4 - get this port mapping from the IGD */
bool SetRedirectAndTest(struct UPNPUrls *urls,
                        struct IGDdatas *data,
                        const char *iaddr,
                        const char *iport,
                        const char *eport,
                        const char *proto) {
    char externalIPAddress[16];
    char intClient[16];
    char intPort[6];
    //  char leaseDuration[] = "3600"; /* 60 mins */
    int r;
    int ok = 1;

    if(!iaddr || !iport || !eport || !proto) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "Wrong arguments\n");
#endif
        return 0;
    }
    proto = protofix(proto);
    if(!proto) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "invalid protocol\n");
#endif
        return 0;
    }

    UPNP_GetExternalIPAddress(urls->controlURL,
                              //data->first.servicetype,
                              data->servicetype,
                              externalIPAddress);
#ifdef DEBUGUPNPUTIL
    if(externalIPAddress[0])
        printf("ExternalIPAddress = %s\n", externalIPAddress);
    else
        printf("GetExternalIPAddress failed.\n");
#endif
    // Unix at the moment!
    /* Starting from miniupnpc version 1.2, lease duration parameter is gone */
    r = UPNP_AddPortMapping(urls->controlURL, data->servicetype,//data->first.servicetype,
                            eport, iport, iaddr, "Mixologist", proto, NULL);
#ifdef DEBUGUPNPUTIL
    if(r==0) {
        printf("AddPortMapping(%s, %s, %s) failed\n", eport, iport, iaddr);
        //this seems to trigger for unknown reasons sometimes.
        //rely on Checking it afterwards...
        //should check IP address then!
        //ok = 0;
    }
#endif
    UPNP_GetSpecificPortMappingEntry(urls->controlURL,
                                     //data->first.servicetype,
                                     data->servicetype,
                                     eport, proto,
                                     intClient, intPort);
#ifdef DEBUGUPNPUTIL
    if(intClient[0])
        printf("InternalIP:Port = %s:%s\n", intClient, intPort);
    else {
        printf("GetSpecificPortMappingEntry failed.\n");
        ok = 0;
    }
#endif
    if ((strcmp(iaddr, intClient) != 0) || (strcmp(iport, intPort) != 0)) {
#ifdef DEBUGUPNPUTIL
        printf("PortMappingEntry to wrong location! FAILED\n");
        printf("IP1:\"%s\"\n", iaddr);
        printf("IP2:\"%s\"\n", intClient);
        printf("PORT1:\"%s\"\n", iport);
        printf("PORT2:\"%s\"\n", intPort);
#endif
        ok = 0;
    }

#ifdef DEBUGUPNPUTIL
    printf("external %s:%s is redirected to internal %s:%s\n",
           externalIPAddress, eport, intClient, intPort);
#endif
    if (ok) {
#ifdef DEBUGUPNPUTIL
        printf("uPnP Forward/Mapping Succeeded\n");
#endif
    } else {
#ifdef DEBUGUPNPUTIL
        printf("uPnP Forward/Mapping Failed\n");
#endif
    }

    return ok;
}

bool TestRedirect(struct UPNPUrls *urls,
                  struct IGDdatas *data,
                  const char *iaddr,
                  const char *iport,
                  const char *eport,
                  const char *proto) {
    char intClient[16];
    char intPort[6];
    int ok = 1;

    if(!iaddr || !iport || !eport || !proto) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "Wrong arguments\n");
#endif
        return 0;
    }
    proto = protofix(proto);
    if(!proto) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "invalid protocol\n");
#endif
        return 0;
    }

    UPNP_GetSpecificPortMappingEntry(urls->controlURL,
                                     //data->first.servicetype,
                                     data->servicetype,
                                     eport, proto,
                                     intClient, intPort);
    if(intClient[0]) {
#ifdef DEBUGUPNPUTIL
        printf("uPnP Check: InternalIP:Port = %s:%s\n", intClient, intPort);
#endif
    } else {
#ifdef DEBUGUPNPUTIL
        printf("GetSpecificPortMappingEntry failed.\n");
#endif
        ok = 0;
    }

#ifdef DEBUGUPNPUTIL
    printf("uPnP Check: External port %s is redirected to internal %s:%s\n",
           eport, intClient, intPort);

    if (ok) {
        printf("uPnP Check: uPnP Forward/Mapping still Active\n");
    } else {
        printf("uPnP Check: Forward/Mapping has been Dropped\n");
    }
#endif
    return ok;
}



bool
RemoveRedirect(struct UPNPUrls *urls,
               struct IGDdatas *data,
               const char *eport,
               const char *proto) {
    if(!proto || !eport) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "invalid arguments\n");
#endif
        return 0;
    }
    proto = protofix(proto);
    if(!proto) {
#ifdef DEBUGUPNPUTIL
        fprintf(stderr, "protocol invalid\n");
#endif
        return 0;
    }
    //UPNP_DeletePortMapping(urls->controlURL, data->first.servicetype, eport, proto, NULL);
    UPNP_DeletePortMapping(urls->controlURL, data->servicetype, eport, proto, NULL);
    return 1;
}


/* EOF */
