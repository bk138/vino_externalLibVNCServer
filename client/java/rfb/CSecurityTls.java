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

  public CSecurityTls(Socket sock_) {
      sock = sock_;
  }

  public int processMsg(CConnection cc) {
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

	  return MSG_AUTH_TYPES;
      } catch (java.io.IOException e) {
	  tlog.error("TLS handshake failed " + e.toString());
	  return MSG_ERROR;
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
      return "TLS Encryption";
  }

  Socket sock;

  static LogWriter tlog = new LogWriter("TLS");
}
