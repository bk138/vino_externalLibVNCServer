/*
 * auth.c - deal with authentication.
 *
 * This file implements the VNC authentication protocol when setting up an RFB
 * connection.
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

void
rfbAuthInitScreen(rfbScreenInfoPtr rfbScreen)
{
#ifdef HAVE_GNUTLS
#define DH_BITS 1024

    gnutls_global_init();
  
    gnutls_anon_allocate_server_credentials(&rfbScreen->anonCredentials);

    gnutls_dh_params_init(&rfbScreen->dhParams);
    gnutls_dh_params_generate2(rfbScreen->dhParams, DH_BITS);

    gnutls_anon_set_server_dh_params(rfbScreen->anonCredentials,
				     rfbScreen->dhParams);

#undef DH_BITS
#endif /* HAVE_GNUTLS */
}

void
rfbAuthCleanupScreen(rfbScreenInfoPtr rfbScreen)
{
#ifdef HAVE_GNUTLS
    gnutls_dh_params_deinit(rfbScreen->dhParams);

    gnutls_anon_free_server_credentials(rfbScreen->anonCredentials);
  
    gnutls_global_deinit();
#endif /* HAVE_GNUTLS */
}

#ifdef HAVE_GNUTLS
static rfbBool
rfbAuthTLSHandshake(rfbClientPtr cl)
{
    static const int kx_priority[] = { GNUTLS_KX_ANON_DH, 0 };
    int              err;
    
    gnutls_init(&cl->tlsSession, GNUTLS_SERVER);

    gnutls_set_default_priority(cl->tlsSession);
    gnutls_kx_set_priority(cl->tlsSession, kx_priority);

    gnutls_credentials_set(cl->tlsSession,
			   GNUTLS_CRD_ANON,
			   cl->screen->anonCredentials);
    gnutls_transport_set_ptr(cl->tlsSession, (gnutls_transport_ptr_t) cl->sock);

    err = gnutls_handshake(cl->tlsSession);
    if (err != GNUTLS_E_SUCCESS && !gnutls_error_is_fatal(err)) {
	cl->state = RFB_TLS_HANDSHAKE;
	return FALSE;
    }

    if (err != GNUTLS_E_SUCCESS) {
	rfbErr("TLS Handshake failed: %s\n", gnutls_strerror(err));
	gnutls_deinit (cl->tlsSession);
	cl->tlsSession = NULL;
	rfbCloseClient(cl);
	return FALSE;
    }

    cl->useTLS = TRUE;

    return TRUE;
}
#endif /* HAVE_GNUTLS */

static rfbBool
rfbAuthClientAuthenticated(rfbClientPtr cl)
{
    rfbBool accepted = FALSE;

    if (cl->state == RFB_INITIALISATION &&
	cl->screen->authenticatedClientHook) {
	switch (cl->screen->authenticatedClientHook(cl)) {
	case RFB_CLIENT_ON_HOLD:
	    cl->onHold = TRUE;
	    break;
	case RFB_CLIENT_ACCEPT:
	    accepted = TRUE;
	    break;
	case RFB_CLIENT_REFUSE:
            rfbCloseClient(cl);
            rfbClientConnectionGone(cl);
	    break;
	}
     }

    return accepted;
}

/*
 * rfbAuthNewClient is called when we reach the point of authenticating
 * a new client.  If authentication isn't being used then we simply send
 * rfbNoAuth.  Otherwise we send rfbVncAuth plus the challenge.
 */

static void
rfbAuthNewClient3_7(rfbClientPtr cl)
{
    rfbSecurityTypesMsg st;
    int i;

    cl->state = RFB_SECURITY_TYPE;

    st.nSecurityTypes = cl->screen->nSecurityTypes;
    for (i = 0; i < st.nSecurityTypes; i++) {
	rfbLog("Advertising security type %d\n", cl->screen->securityTypes[i]);
	st.securityTypes[i] = cl->screen->securityTypes[i];
    }

    if (WriteExact(cl, (char *)&st, st.nSecurityTypes + 1) < 0) {
        rfbLogPerror("rfbAuthNewClient: write");
        rfbCloseClient(cl);
        return;
    }
}

static void
rfbAuthNewClient3_3(rfbClientPtr cl)
{
    char buf[4 + CHALLENGESIZE];
    int len, i;

    for (i = 0; i < cl->screen->nSecurityTypes; i++)
	if (cl->screen->securityTypes [i] == rfbVncAuth ||
	    cl->screen->securityTypes [i] == rfbNoAuth)
	    break;

    if (i == cl->screen->nSecurityTypes) {
	rfbClientConnFailed(cl, "No security type suitable for RFB 3.3 supported");
	return;
    }

    switch (cl->screen->securityTypes [i]) {
    case rfbVncAuth:
        *(uint32_t *)buf = Swap32IfLE(rfbVncAuth);
        vncRandomBytes(cl->authChallenge);
        memcpy(&buf[4], (char *)cl->authChallenge, CHALLENGESIZE);
        len = 4 + CHALLENGESIZE;
	cl->state = RFB_AUTHENTICATION;
	break;
    case rfbNoAuth:
        *(uint32_t *)buf = Swap32IfLE(rfbNoAuth);
        len = 4;
        cl->state = RFB_INITIALISATION;
	break;
    default:
	/* can't be reached */
	return;
    }

    if (WriteExact(cl, buf, len) < 0) {
        rfbLogPerror("rfbAuthNewClient: write");
        rfbCloseClient(cl);
        return;
    }

    rfbAuthClientAuthenticated(cl);
}

void
rfbAuthNewClient(rfbClientPtr cl)
{
    if (cl->minorVersion >= rfbProtocolMinorVersion7)
	rfbAuthNewClient3_7(cl);
    else
	rfbAuthNewClient3_3(cl);
}

void
rfbAuthCleanupClient(rfbClientPtr cl)
{
#ifdef HAVE_GNUTLS
    if (cl->tlsSession) {
	if (cl->sock)
	    gnutls_bye(cl->tlsSession, GNUTLS_SHUT_WR);

	gnutls_deinit(cl->tlsSession);
	cl->tlsSession = NULL;
    }
#endif /* HAVE_GNUTLS */
}

#ifdef HAVE_GNUTLS
static void
rfbAuthListAuthTypes(rfbClientPtr cl)
{
    rfbAuthTypesMsg st;
    int i;

    cl->state = RFB_AUTH_TYPE;

    st.nAuthTypes = cl->screen->nAuthTypes;
    for (i = 0; i < cl->screen->nAuthTypes; i++) {
	rfbLog("Advertising authentication type %d\n", cl->screen->authTypes[i]);
	st.authTypes[i] = cl->screen->authTypes[i];
    }

    if (WriteExact(cl, (char *)&st, cl->screen->nAuthTypes + 1) < 0) {
        rfbLogPerror("rfbAuthNewClient: write");
        rfbCloseClient(cl);
        return;
    }
}
#endif /* HAVE_GNUTLS */

void
rfbAuthProcessSecurityTypeMessage(rfbClientPtr cl)
{
    uint8_t securityType;
    int n, i;

    if ((n = ReadExact(cl, (char *)&securityType, 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbAuthProcessSecurityTypeMessage: read");
        rfbCloseClient(cl);
        return;
    }

    rfbLog("Client returned security type %d\n", securityType);

    for (i = 0; i < cl->screen->nSecurityTypes; i++)
	if (cl->screen->securityTypes[i] == securityType)
	    break;

    if (i == cl->screen->nSecurityTypes) {
	rfbErr("rfbAuthProcessSecurityTypeMessage: client returned unadvertised security type %d\n",
	       securityType);
	rfbCloseClient(cl);
	return;
    }

    switch (securityType) {
#ifdef HAVE_GNUTLS
    case rfbTLS:
	if (!rfbAuthTLSHandshake(cl))
	    return;
	rfbAuthListAuthTypes(cl);
	break;
#endif
    case rfbVncAuth:
        vncRandomBytes(cl->authChallenge);
	if (WriteExact(cl, (char *)&cl->authChallenge, CHALLENGESIZE) < 0) {
	    rfbLogPerror("rfbAuthProcessSecurityTypeMessage: write");
	    rfbCloseClient(cl);
	    return;
	}
	cl->state = RFB_AUTHENTICATION;
	break;
    case rfbNoAuth:

	if (cl->minorVersion >= rfbProtocolMinorVersion8)
	    rfbAuthPasswordChecked(cl, RFB_CLIENT_ACCEPT);
	else {
	    cl->state = RFB_INITIALISATION;
	    if (rfbAuthClientAuthenticated(cl))
		rfbProcessClientInitMessage(cl);
	}
	break;
    default:
	/* can't be reached */
	break;
    }
}

#ifdef HAVE_GNUTLS
void
rfbAuthProcessTLSHandshake(rfbClientPtr cl)
{
    int err;

    err = gnutls_handshake(cl->tlsSession);
    if (err != GNUTLS_E_SUCCESS && !gnutls_error_is_fatal(err))
	return;

    if (err != GNUTLS_E_SUCCESS) {
	rfbErr("TLS Handshake failed: %s\n", gnutls_strerror(err));
	gnutls_deinit (cl->tlsSession);
	cl->tlsSession = NULL;
	rfbCloseClient(cl);
	return;
    }

    cl->useTLS = TRUE;

    rfbAuthListAuthTypes(cl);
}
#endif /* HAVE_GNUTLS */

void
rfbAuthProcessAuthTypeMessage(rfbClientPtr cl)
{
    uint8_t authType;
    int n, i;

    if ((n = ReadExact(cl, (char *)&authType, 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbAuthProcessAuthTypeMessage: read");
        rfbCloseClient(cl);
        return;
    }

    rfbLog("Client returned authentication type %d\n", authType);

    for (i = 0; i < cl->screen->nAuthTypes; i++)
	if (cl->screen->authTypes[i] == authType)
	    break;

    if (i == cl->screen->nAuthTypes) {
	rfbErr("rfbAuthProcessAuthTypeMessage: client returned unadvertised authentication type %d\n",
	       authType);
	rfbCloseClient(cl);
	return;
    }

    switch (authType) {
    case rfbVncAuth:
        vncRandomBytes(cl->authChallenge);
	if (WriteExact(cl, (char *)&cl->authChallenge, CHALLENGESIZE) < 0) {
	    rfbLogPerror("rfbAuthProcessAuthTypeMessage: write");
	    rfbCloseClient(cl);
	    return;
	}
	cl->state = RFB_AUTHENTICATION;
	break;
    case rfbNoAuth:
        cl->state = RFB_INITIALISATION;
	if (rfbAuthClientAuthenticated(cl))
	    rfbProcessClientInitMessage(cl);
	break;
    default:
	/* can't be reached */
	break;
    }
}

/*
 * rfbAuthProcessClientMessage is called when the client sends its
 * authentication response.
 */

void
rfbAuthProcessClientMessage(rfbClientPtr cl)
{
    int n;
    uint8_t response[CHALLENGESIZE];
    enum rfbNewClientAction result;

    if ((n = ReadExact(cl, (char *)response, CHALLENGESIZE)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbAuthProcessClientMessage: read");
        rfbCloseClient(cl);
        return;
    }

    result = RFB_CLIENT_REFUSE;
    if (cl->screen->passwordCheck)
	result = cl->screen->passwordCheck(cl, (const char *)response, CHALLENGESIZE);

    rfbAuthPasswordChecked(cl, result);
}

void
rfbAuthPasswordChecked(rfbClientPtr            cl,
		       enum rfbNewClientAction result)
{
    uint32_t authResult;
    
    switch (result) {
    case RFB_CLIENT_ON_HOLD:
	cl->state = RFB_AUTH_DEFERRED;
	cl->onHold = TRUE;
	break;
    case RFB_CLIENT_ACCEPT:
	cl->onHold = FALSE;
	authResult = Swap32IfLE(rfbVncAuthOK);

	if (WriteExact(cl, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthPasswordChecked: write");
	    rfbCloseClient(cl);
	    return;
	}

	cl->state = RFB_INITIALISATION;
	rfbAuthClientAuthenticated(cl);
	break;
    default:
    case RFB_CLIENT_REFUSE:
        rfbErr("rfbAuthPasswordChecked: password check failed\n");
        authResult = Swap32IfLE(rfbVncAuthFailed);

	if (WriteExact(cl, (char *)&authResult, 4) < 0) {
	    rfbLogPerror("rfbAuthPasswordChecked: write");
	    rfbCloseClient(cl);
	    return;
	}

	if (cl->minorVersion >= rfbProtocolMinorVersion8) {
	    /* We can't really localize this string, since it has to
	     * be iso8859-1 encoded. However, the string will only be
	     * returned when we're using protocol version 3.8, and we
	     * only advertise support for 3.7, so this only gets used
	     * at all if you have a broken client.
	     */
	    const char errmsg[] = "Password incorrect";
	    uint32_t len, wireLen;

	    len = sizeof(errmsg) - 1;
	    wireLen = Swap32IfLE(len);
	    if (WriteExact(cl, (char *)&wireLen, 4) < 0 ||
		WriteExact(cl, errmsg, len) < 0) {
		rfbLogPerror("rfbAuthPasswordChecked: write");
		rfbCloseClient(cl);
		return;
	    }
	}

        rfbCloseClient(cl);
	break;
    }
}
