/* Copyright (C) 2002-2003 RealVNC Ltd.  All Rights Reserved.
 *    
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package rfb;

import java.util.ArrayList;
import java.net.*;
import javax.net.ssl.*;

public class CSecurityTls extends CSecurity {

  public CSecurityTls(Socket sock_, int authType_, PasswdGetter pg) {
      sock = sock_;
      authType = authType_;
      handshakeComplete = false;
      
      switch (authType) {
      case rfb.SecTypes.none:
	  authSecurity = new rfb.CSecurityNone();
	  break;
      case rfb.SecTypes.vncAuth:
	  authSecurity = new rfb.CSecurityVncAuth(pg);
	  break;
      default:
	  throw new rfb.Exception("Unsupported authType?");
      }
  }

  public int processMsg(CConnection cc) {
      if (!handshakeComplete) {
	  try {
	      SSLSocketFactory sslfactory;
	      SSLSocket        sslsock;

	      sslfactory = (SSLSocketFactory)SSLSocketFactory.getDefault();
	      sslsock = (SSLSocket)sslfactory.createSocket (sock,
							    sock.getInetAddress().getHostName(),
							    sock.getPort(),
							    true);

	      setAnonDHKeyExchangeEnabled(sslsock);
	  
	      /* Not neccessary - just ensures that we know what cipher
	       * suite we are using for the output of toString()
	       */
	      sslsock.startHandshake();

	      tlog.debug("Completed handshake with server " + sslsock.toString ());

	      cc.setStreams(new rdr.JavaInStream(sslsock.getInputStream()),
			    new rdr.JavaOutStream(sslsock.getOutputStream()));

	      handshakeComplete = true;

	      return 2;
	  } catch (java.io.IOException e) {
	      tlog.error("TLS handshake failed " + e.toString());
	      return 0;
	  }
      } else {
	  return authSecurity.processMsg(cc);
      }
  }

  private static void setAnonDHKeyExchangeEnabled(SSLSocket sock) {
        String[]  supported;
        ArrayList enabled = new ArrayList();

        supported = sock.getSupportedCipherSuites();
	
        for (int i = 0; i < supported.length; i++)
            if (supported [i].matches(".*DH_anon.*"))
                enabled.add(supported [i]);
	
        sock.setEnabledCipherSuites((String[])enabled.toArray(new String[0]));
  }

  public String getDescription() {
      switch (authType) {
      case rfb.SecTypes.none:
	  return "No Authentication With TLS Encryption";
      case rfb.SecTypes.vncAuth:
	  return "Password Authentication With TLS Encryption";
      default:
	  throw new rfb.Exception("Unsupported authType?");
      }
  }

  Socket sock;
  int authType;
  CSecurity authSecurity;
  boolean handshakeComplete;

  static LogWriter tlog = new LogWriter("TLS");
}
