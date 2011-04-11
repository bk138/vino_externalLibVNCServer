/*
 * sockets.c - deal with TCP sockets.
 *
 * This code should be independent of any changes in the RFB protocol.  It just
 * deals with the X server scheduling stuff, calling rfbNewClientConnection and
 * rfbProcessClientMessage to actually deal with the protocol.  If a socket
 * needs to be closed for any reason then rfbCloseClient should be called, and
 * this in turn will call rfbClientConnectionGone.  To make an active
 * connection out, call rfbConnect - note that this does _not_ call
 * rfbNewClientConnection.
 *
 * This file is divided into two types of function.  Those beginning with
 * "rfb" are specific to sockets using the RFB protocol.  Those without the
 * "rfb" prefix are more general socket routines (which are used by the http
 * code).
 *
 * Thanks to Karl Hakimian for pointing out that some platforms return EAGAIN
 * not EWOULDBLOCK.
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <rfb/rfb.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef WIN32
#pragma warning (disable: 4018 4761)
#define close closesocket
#define read(sock,buf,len) recv(sock,buf,len,0)
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ETIMEDOUT WSAETIMEDOUT
#define write(sock,buf,len) send(sock,buf,len,0)
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include <net/if.h>  // IFF_UP

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#else
#include "ifaddr/ifaddrs.h"
#endif

#ifdef RFC2553
#define ADDR_FAMILY_MEMBER ss_family
#else
#define ADDR_FAMILY_MEMBER sa_family
#endif

#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval 
{
   long int tv_sec,tv_usec;
}
;
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <errno.h>

/*#ifndef WIN32
int max(int i,int j) { return(i<j?j:i); }
#endif
*/

int rfbMaxClientWait = 20000;   /* time (ms) after which we decide client has
                                   gone away - needed to stop us hanging */

/*
 * rfbInitSockets sets up the TCP sockets to listen for RFB
 * connections.  It does nothing if called again.
 */

static void rfbInitListenSock(rfbScreenInfoPtr rfbScreen);

void
rfbInitSockets(rfbScreenInfoPtr rfbScreen)
{
    if (rfbScreen->socketInitDone)
	return;

    rfbScreen->socketInitDone = TRUE;

    if (rfbScreen->inetdSock != -1) {
	const int one = 1;

#ifndef WIN32
	if (fcntl(rfbScreen->inetdSock, F_SETFL, O_NONBLOCK) < 0) {
	    rfbLogPerror("fcntl");
	    return;
	}
#endif

	if (setsockopt(rfbScreen->inetdSock, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&one, sizeof(one)) < 0) {
	    rfbLogPerror("setsockopt");
	    return;
	}

    	FD_ZERO(&(rfbScreen->allFds));
    	FD_SET(rfbScreen->inetdSock, &(rfbScreen->allFds));
    	rfbScreen->maxFd = rfbScreen->inetdSock;
	return;
    }

    rfbInitListenSock(rfbScreen);
}

static void
rfbInitListenSock(rfbScreenInfoPtr rfbScreen)
{
    char *netIface = (char*)rfbScreen->netIface;
    int i;

    if(netIface == NULL || if_nametoindex(netIface) == 0) {
      if(netIface != NULL)
        rfbLog("WARNING: This (%s) a invalid network interface, set to all\n", netIface);
      netIface = NULL;
    }

    if(rfbScreen->autoPort) {
      rfbLog("Autoprobing TCP port in (%s) network interface\n",
            netIface != NULL ? netIface : "all");

      for (i = 5900; i < 6000; i++) {
        if (ListenOnTCPPort(rfbScreen, i, netIface)) {
          rfbScreen->rfbPort = i;
          break;
        }
      }

      if (i >= 6000) {
        rfbLogPerror("Failure autoprobing");
        return;
      }

      rfbLog("Autoprobing selected port %d\n", rfbScreen->rfbPort);

      FD_ZERO(&rfbScreen->allFds);
      for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
        FD_SET(rfbScreen->rfbListenSock[i], &rfbScreen->allFds);
        rfbScreen->maxFd = rfbScreen->rfbListenSock[i];
      }
    }
    else if(rfbScreen->rfbPort > 0) {
      rfbLog("Listening for VNC connections on TCP port %d in (%s) network interface\n", 
            rfbScreen->rfbPort, netIface != NULL ? netIface : "all");

      if (!ListenOnTCPPort(rfbScreen, rfbScreen->rfbPort, netIface)) {
        rfbLogPerror("ListenOnTCPPort");
        return;
      }

      FD_ZERO(&rfbScreen->allFds);
      for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
        FD_SET(rfbScreen->rfbListenSock[i], &rfbScreen->allFds);
        rfbScreen->maxFd = rfbScreen->rfbListenSock[i];
      }
    }
}

void
rfbSetAutoPort(rfbScreenInfoPtr rfbScreen, rfbBool autoPort)
{
    if (rfbScreen->autoPort == autoPort)
        return;

    rfbScreen->autoPort = autoPort;

    if (!rfbScreen->socketInitDone)
	return;

    if (rfbScreen->rfbListenSockTotal > 0) {
        int i;

        for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
           FD_CLR(rfbScreen->rfbListenSock[i], &rfbScreen->allFds);
           close(rfbScreen->rfbListenSock[i]);
           rfbScreen->rfbListenSock[i] = -1;
        }
        rfbScreen->rfbListenSockTotal = 0;
    }

    rfbInitListenSock(rfbScreen);
}

void
rfbSetPort(rfbScreenInfoPtr rfbScreen, int port)
{
    if (rfbScreen->rfbPort == port)
        return;

    rfbScreen->rfbPort = port;

    if (!rfbScreen->socketInitDone || rfbScreen->autoPort)
	return;

    if (rfbScreen->rfbListenSockTotal > 0) {
        int i;

        for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
           FD_CLR(rfbScreen->rfbListenSock[i], &rfbScreen->allFds);
           close(rfbScreen->rfbListenSock[i]);
           rfbScreen->rfbListenSock[i] = -1;
        }
        rfbScreen->rfbListenSockTotal = 0;
    }

    rfbInitListenSock(rfbScreen);
}

void
rfbProcessNewConnection(rfbScreenInfoPtr rfbScreen, int insock)
{
    const int one = 1;
    int sock = -1;
    
    if((sock = accept(insock, NULL, NULL)) < 0) {
      rfbLogPerror("rfbCheckFds: accept");
      return;
    }

    if(sock < 0) {
      rfbLogPerror("rfbCheckFds: accept");
      return;
    }

#ifndef WIN32
    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
	rfbLogPerror("rfbCheckFds: fcntl");
	close(sock);
	return;
    }
#endif

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
	rfbLogPerror("rfbCheckFds: setsockopt");
	close(sock);
	return;
    }

    rfbNewClient(rfbScreen,sock);
}

/*
 * rfbCheckFds is called from ProcessInputEvents to check for input on the RFB
 * socket(s).  If there is input to process, the appropriate function in the
 * RFB server code will be called (rfbNewClientConnection,
 * rfbProcessClientMessage, etc).
 */

void
rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec)
{
    int nfds;
    int n;
    fd_set fds;
    struct timeval tv;
    rfbClientIteratorPtr i;
    rfbClientPtr cl;

    if (!rfbScreen->inetdInitDone && rfbScreen->inetdSock != -1) {
	rfbNewClientConnection(rfbScreen,rfbScreen->inetdSock); 
	rfbScreen->inetdInitDone = TRUE;
    }

    memcpy(&fds, &rfbScreen->allFds, sizeof(fd_set));
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    nfds = select(rfbScreen->maxFd + 1, &fds, NULL, NULL /* &fds */, &tv);
    if (nfds == 0) {
	return;
    }
    if (nfds < 0) {
#ifdef WIN32
		errno = WSAGetLastError();
#endif
	if (errno != EINTR)
		rfbLogPerror("rfbCheckFds: select");
	return;
    }
    printf("DUMP: nfds = %d\n", nfds);
    for(n=0; n < rfbScreen->rfbListenSockTotal; n++) {
        if (rfbScreen->rfbListenSock[n] != -1 && FD_ISSET(rfbScreen->rfbListenSock[n], &fds)) {
          rfbProcessNewConnection(rfbScreen, rfbScreen->rfbListenSock[n]);
          FD_CLR(rfbScreen->rfbListenSock[n], &fds);
          if (--nfds == 0)
            return;
        }
    }

    i = rfbGetClientIterator(rfbScreen);
    while((cl = rfbClientIteratorNext(i))) {
      if (cl->onHold)
	continue;
      if (FD_ISSET(cl->sock, &fds) && FD_ISSET(cl->sock, &(rfbScreen->allFds)))
	rfbProcessClientMessage(cl);
    }
    rfbReleaseClientIterator(i);
}

void
rfbCloseClient(cl)
     rfbClientPtr cl;
{
    rfbAuthCleanupClient(cl);

    LOCK(cl->updateMutex);
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    if (cl->sock != -1)
#endif
      {
	FD_CLR(cl->sock,&(cl->screen->allFds));
	if(cl->sock==cl->screen->maxFd)
	  while(cl->screen->maxFd>0
		&& !FD_ISSET(cl->screen->maxFd,&(cl->screen->allFds)))
	    cl->screen->maxFd--;
	shutdown(cl->sock,SHUT_RDWR);
	close(cl->sock);
	cl->sock = -1;
      }
    UNLOCK(cl->updateMutex);
}


#ifdef HAVE_GNUTLS
static int
ReadExactOverTLS(rfbClientPtr cl, char* buf, int len, int timeout)
{
    while (len > 0) {
	int n;

	n = gnutls_record_recv(cl->tlsSession, buf, len);
	if (n == 0) {
	    UNLOCK(cl->outputMutex);
	    return 0;

	} else if (n < 0) {
	    if (n == GNUTLS_E_INTERRUPTED || n == GNUTLS_E_AGAIN)
		continue;

	    UNLOCK(cl->outputMutex);
	    return -1;
	}

	buf += n;
	len -= n;
    }

    return 1;
}
#endif /* HAVE_GNUTLS */

/*
 * ReadExact reads an exact number of bytes from a client.  Returns 1 if
 * those bytes have been read, 0 if the other end has closed, or -1 if an error
 * occurred (errno is set to ETIMEDOUT if it timed out).
 */

int
ReadExactTimeout(rfbClientPtr cl, char* buf, int len, int timeout)
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;

#ifdef HAVE_GNUTLS
    if (cl->useTLS)
	return ReadExactOverTLS(cl, buf, len, timeout);
#endif

    while (len > 0) {
        n = read(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            return 0;

        } else {
#ifdef WIN32
			errno = WSAGetLastError();
#endif
	    if (errno == EINTR)
		continue;

            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return n;
            }

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            n = select(sock+1, &fds, NULL, &fds, &tv);
            if (n < 0) {
                rfbLogPerror("ReadExact: select");
                return n;
            }
            if (n == 0) {
                errno = ETIMEDOUT;
                return -1;
            }
        }
    }
    return 1;
}

int ReadExact(rfbClientPtr cl,char* buf,int len)
{
  return(ReadExactTimeout(cl,buf,len,rfbMaxClientWait));
}

#ifdef HAVE_GNUTLS
static int
WriteExactOverTLS(rfbClientPtr cl, const char* buf, int len)
{
    LOCK(cl->outputMutex);

    while (len > 0) {
	int n;

	n = gnutls_record_send(cl->tlsSession, buf, len);
	if (n == 0) {
	    UNLOCK(cl->outputMutex);
	    return 0;

	} else if (n < 0) {
	    if (n == GNUTLS_E_INTERRUPTED || n == GNUTLS_E_AGAIN)
		continue;

	    UNLOCK(cl->outputMutex);
	    return -1;
	}

	buf += n;
	len -= n;
    }

    UNLOCK(cl->outputMutex);

    return 1;
}
#endif /* HAVE_GNUTLS */

/*
 * WriteExact writes an exact number of bytes to a client.  Returns 1 if
 * those bytes have been written, or -1 if an error occurred (errno is set to
 * ETIMEDOUT if it timed out).
 */

int
WriteExact(rfbClientPtr cl, const char* buf, int len)
{
    int sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;
    int totalTimeWaited = 0;

#ifdef HAVE_GNUTLS
    if (cl->useTLS)
	return WriteExactOverTLS(cl, buf, len);
#endif

    LOCK(cl->outputMutex);
    while (len > 0) {
        n = write(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            rfbErr("WriteExact: write returned 0?\n");
            return 0;

        } else {
#ifdef WIN32
			errno = WSAGetLastError();
#endif
	    if (errno == EINTR)
		continue;

            if (errno != EWOULDBLOCK && errno != EAGAIN) {
	        UNLOCK(cl->outputMutex);
                return n;
            }

            /* Retry every 5 seconds until we exceed rfbMaxClientWait.  We
               need to do this because select doesn't necessarily return
               immediately when the other end has gone away */

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            n = select(sock+1, NULL, &fds, NULL /* &fds */, &tv);
            if (n < 0) {
                rfbLogPerror("WriteExact: select");
                UNLOCK(cl->outputMutex);
                return n;
            }
            if (n == 0) {
                totalTimeWaited += 5000;
                if (totalTimeWaited >= rfbMaxClientWait) {
                    errno = ETIMEDOUT;
                    UNLOCK(cl->outputMutex);
                    return -1;
                }
            } else {
                totalTimeWaited = 0;
            }
        }
    }
    UNLOCK(cl->outputMutex);
    return 1;
}

rfbBool
ListenOnTCPPort(rfbScreenInfoPtr rfbScreen, int port, const char *netIface)
{
  int sock   = -1;
  int *psock = NULL;
  int *ptot  = NULL;
  struct ifaddrs *myaddrs = NULL; 
  struct ifaddrs *ifa     = NULL;

  if(rfbScreen == NULL)
    return FALSE;
  
  psock = rfbScreen->rfbListenSock;
  ptot  = &rfbScreen->rfbListenSockTotal;
  *ptot = 0;

  if(netIface == NULL || strlen(netIface) == 0)
  {
#ifdef ENABLE_IPV6
    int sock6 = -1;
    struct sockaddr_in6 s6;

    memset(&s6, 0, sizeof(s6));
    s6.sin6_family = AF_INET6;
    s6.sin6_port   = htons(port);
    s6.sin6_addr   = in6addr_any;

    sock6 = NewSocketListenTCP ((struct sockaddr*)&s6, sizeof(s6));
    rfbLog("Listening IPv6://[::]:%d\n", port);
#endif

      struct sockaddr_in s4;

      memset(&s4, 0, sizeof(s4));
      s4.sin_family      = AF_INET;
      s4.sin_port        = htons(port);
      s4.sin_addr.s_addr = htonl(INADDR_ANY);

      sock = NewSocketListenTCP ((struct sockaddr*)&s4, sizeof(s4));
      rfbLog("Listening IPv4://0.0.0.0:%d\n", port);

#ifdef ENABLE_IPV6
    if(sock6 > 0) {
       psock[*ptot] = sock6;
      *ptot        += 1;
    }
#endif
    if(sock > 0) {
       psock[*ptot] = sock;
      *ptot        += 1;
    }

    if (*ptot)
      return TRUE;

    /* no need to log sock/sock6, both are -1 here */
    rfbLog("Problems in NewSocketListenTCP()\n");
    return FALSE;
  }

  if(getifaddrs(&myaddrs) != 0) {
    rfbLogPerror("getifaddrs\n");
    return FALSE;
  }

  for (ifa = myaddrs; ifa != NULL && *ptot < RFB_MAX_SOCKETLISTEN; ifa = ifa->ifa_next) {
    char buf[64] = { 0, };

    if (ifa->ifa_addr == NULL || (ifa->ifa_flags & IFF_UP) == 0) 
      continue;

    if (ifa->ifa_addr->ADDR_FAMILY_MEMBER == AF_INET) {
      struct sockaddr_in *s4 = (struct sockaddr_in*)ifa->ifa_addr;
      s4->sin_port           = htons(port);

      if (inet_ntop(s4->sin_family, (struct sockaddr*)&s4->sin_addr, buf, sizeof(buf)) == NULL) {
        rfbLog("%s: inet_ntop failed!\n", ifa->ifa_name);
        continue;
      }
      else if(!strcmp(ifa->ifa_name, netIface)) {
        rfbLog("Listening IPv4://%s:%d\n", buf, port);
        sock = NewSocketListenTCP((struct sockaddr*)s4, INET_ADDRSTRLEN);
      }
    }
#ifdef ENABLE_IPV6            
    if (ifa->ifa_addr->ADDR_FAMILY_MEMBER == AF_INET6) {
      struct sockaddr_in6 *s6 = (struct sockaddr_in6*)ifa->ifa_addr;
      s6->sin6_port           = htons(port);

      if (inet_ntop(ifa->ifa_addr->ADDR_FAMILY_MEMBER, (struct sockaddr*)&s6->sin6_addr, buf, sizeof(buf)) == NULL) {
        rfbLog("%s: inet_ntop failed!\n", ifa->ifa_name);
        continue; 
      }
      else if(!strcmp(ifa->ifa_name, netIface)) {
        rfbLog("Listening IPv6://%s:%d\n", buf, port);
        sock = NewSocketListenTCP((struct sockaddr*)s6, INET6_ADDRSTRLEN);
      }
    }
#endif       

    if(sock > 0) {
       psock[*ptot] = sock;
      *ptot        += 1;
       sock         = -1;
    }
  }

  freeifaddrs(myaddrs);

  return TRUE;
}

int
NewSocketListenTCP(struct sockaddr *addr, socklen_t len)
{
    int sock = -1;
    int one  = 1;

    if ((sock = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
      return -1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
      close(sock);
      return -1;
    }

#ifdef ENABLE_IPV6
    if (addr->sa_family == AF_INET6) {
#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
      setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&one, sizeof(one));
      /* we cannot really check for errors here */
#endif
    }
#endif

    if (bind(sock, addr, len) < 0) {
      close(sock);
      return -1;
    }

    if (listen(sock, 5) < 0) {
      close(sock);
      return -1;
    }

    return sock;
}

rfbBool
rfbSetNetworkInterface(rfbScreenInfoPtr rfbScreen, const char *netIface)
{
  int i;

  if (!rfbScreen->socketInitDone || !rfbScreen->socketInitDone)
    return FALSE;

  if(rfbScreen->rfbListenSockTotal > 0) {
    for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
       FD_CLR(rfbScreen->rfbListenSock[i], &(rfbScreen->allFds));
       close(rfbScreen->rfbListenSock[i]);
       rfbScreen->rfbListenSock[i] = -1;
    }
    rfbScreen->rfbListenSockTotal = 0;
  }

  if(netIface != NULL && strlen(netIface) > 0 && if_nametoindex(netIface) > 0) {
     rfbScreen->netIface = netIface;
  }
  else {
     rfbScreen->netIface = NULL;
     if(netIface != NULL)
        rfbLog("WARNING: This (%s) a invalid network interface, set to all\n", netIface);
  }

  rfbLog("Re-binding socket to listen for VNC connections on TCP port %d in (%s) interface\n",
         rfbScreen->rfbPort, rfbScreen->netIface != NULL ? rfbScreen->netIface : "all");
 
  if (!ListenOnTCPPort(rfbScreen, rfbScreen->rfbPort, rfbScreen->netIface)) {
    rfbLogPerror("ListenOnTCPPort");
    return FALSE;
  }

  for(i=0; i < rfbScreen->rfbListenSockTotal; i++) {
    FD_SET(rfbScreen->rfbListenSock[i], &(rfbScreen->allFds));
    rfbScreen->maxFd = max(rfbScreen->rfbListenSock[i], rfbScreen->maxFd);
  }
 
  return TRUE;
}

