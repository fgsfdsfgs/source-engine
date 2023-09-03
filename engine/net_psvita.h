//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NET_PSVITA_H
#define NET_PSVITA_H

#include <vitasdk.h>

#ifdef PLATFORM_PSVITA

#ifndef FIONBIO
#define FIONBIO SO_NONBLOCK
#endif

#ifndef FIONREAD
#define FIONREAD SO_RCVBUF
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* ioctlsocket() is only used to set non-blocking on sockets */
static inline int ioctl_psvita( int fd, int req, unsigned int *arg )
{
	if ( req == FIONBIO )
	{
		return setsockopt( fd, SOL_SOCKET, SO_NONBLOCK, arg, sizeof( *arg ) );
	}
	else if ( req == FIONREAD )
	{
		// TODO: figure out how to get the socket UID and do sceNetGetSockInfo()
		// this is only used in cl_rcon and net_ws, so who cares
		return -ENOSYS;
	}
	return -ENOSYS;
}

#undef ioctlsocket
#define ioctlsocket ioctl_psvita

#endif

#endif // NET_PSVITA_H
