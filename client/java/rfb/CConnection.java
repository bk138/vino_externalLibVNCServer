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

abstract public class CConnection extends CMsgHandler {

  public CConnection() {
    state_ = RFBSTATE_UNINITIALISED;
    secTypes = new int[maxSecTypes];
  }

  // Methods to initialise the connection

  public void setServerName(String name) {
    serverName = name;
  }

  // setStreams() sets the streams to be used for the connection.  These must
  // be set before initialiseProtocol() and processMsg() are called.  The
  // CSecurity object may call setStreams() again to provide alternative
  // streams over which the RFB protocol is sent (i.e. encrypting/decrypting
  // streams).  Ownership of the streams remains with the caller
  // (i.e. SConnection will not delete them).
  public void setStreams(rdr.InStream is_, rdr.OutStream os_) {
    is = is_;
    os = os_;
  }

  // addSecType() should be called once for each security type which the
  // client supports.  The order in which they're added is such that the
  // first one is most preferred.
  public void addSecType(int secType) {
    if (nSecTypes == maxSecTypes)
      throw new Exception("too many security types");
    secTypes[nSecTypes++] = secType;
  }

  // setShared sets the value of the shared flag which will be sent to the
  // server upon initialisation.
  public void setShared(boolean s) { shared = s; }

  // setProtocol3_3 configures whether or not the CConnection should
  // only ever support protocol version 3.3
  public void setProtocol3_3(boolean b) { useProtocol3_3 = b; }

  // initialiseProtocol() should be called once the streams and security
  // types are set.  Subsequently, processMsg() should be called whenever
  // there is data to read on the InStream.
  public void initialiseProtocol() {
    state_ = RFBSTATE_PROTOCOL_VERSION;
  }

  // processMsg() should be called whenever there is data to read on the
  // InStream.  You must have called initialiseProtocol() first.
  public void processMsg() {
    switch (state_) {

    case RFBSTATE_PROTOCOL_VERSION: processVersionMsg();       break;
    case RFBSTATE_SECURITY_TYPES:   processSecurityTypesMsg(); break;
    case RFBSTATE_SECURITY:         processSecurityMsg();      break;
    case RFBSTATE_INITIALISATION:   processInitMsg();          break;
    case RFBSTATE_NORMAL:           reader_.readMsg();        break;
    case RFBSTATE_UNINITIALISED:
      throw new Exception("CConnection.processMsg: not initialised yet?");
    default:
      throw new Exception("CConnection.processMsg: invalid state");
    }
  }

  // Methods to be overridden in a derived class

  // getCSecurity() gets the CSecurity object for the given type.  The type
  // is guaranteed to be one of the secTypes passed in to addSecType().  The
  // CSecurity object's destroy() method will be called by the CConnection
  // from its destructor.
  abstract public CSecurity getCSecurity(int secType);

  // authSuccess() is called when authentication has succeeded.
  public void authSuccess() {}

  // serverInit() is called when the ServerInit message is received.  The
  // derived class must call on to CConnection::serverInit().
  public void serverInit() {
    state_ = RFBSTATE_NORMAL;
    vlog.debug("initialisation done");
  }

  // Other methods

  public CMsgReader reader() { return reader_; }
  public CMsgWriter writer() { return writer_; }

  public rdr.InStream getInStream() { return is; }
  public rdr.OutStream getOutStream() { return os; }

  public String getServerName() { return serverName; }

  public static final int RFBSTATE_UNINITIALISED = 0;
  public static final int RFBSTATE_PROTOCOL_VERSION = 1;
  public static final int RFBSTATE_SECURITY_TYPES = 2;
  public static final int RFBSTATE_SECURITY = 3;
  public static final int RFBSTATE_INITIALISATION = 4;
  public static final int RFBSTATE_NORMAL = 5;
  public static final int RFBSTATE_INVALID = 6;

  public int state() { return state_; }

  protected void setState(int s) { state_ = s; }

  void processVersionMsg() {
    vlog.debug("reading protocol version");
    if (!cp.readVersion(is)) {
      state_ = RFBSTATE_INVALID;
      throw new Exception("reading version failed: not an RFB server?");
    }

    vlog.info("Server supports RFB protocol version "+cp.majorVersion+"."+
              cp.minorVersion);

    // The only official RFB protocol versions are currently 3.3 and 3.7
    if (!useProtocol3_3 &&
        (cp.majorVersion > 3 ||
         (cp.majorVersion == 3 && cp.minorVersion >= 7))) {
      cp.majorVersion = 3;
      cp.minorVersion = 7;
    } else if (cp.majorVersion == 3 && cp.minorVersion >= 3) {
      cp.majorVersion = 3;
      cp.minorVersion = 3;
    } else {
      String msg = ("Server gave unsupported RFB protocol version "+
                    cp.majorVersion+"."+cp.minorVersion);
      vlog.error(msg);
      state_ = RFBSTATE_INVALID;
      throw new Exception(msg);
    }

    cp.writeVersion(os);
    state_ = RFBSTATE_SECURITY_TYPES;

    vlog.info("Using RFB protocol version "+cp.majorVersion+"."+
              cp.minorVersion);
  }

  void processSecurityTypesMsg() {
    vlog.debug("processing security types message");

    int secType = SecTypes.invalid;

    if (cp.majorVersion == 3 && cp.minorVersion == 3) {

      // legacy 3.3 server may only offer "vnc authentication" or "none"

      secType = is.readU32();
      if (secType == SecTypes.invalid) {
        throwConnFailedException();

      } else if (secType == SecTypes.none || secType == SecTypes.vncAuth) {
        int j;
        for (j = 0; j < nSecTypes; j++)
          if (secTypes[j] == secType) break;
        if (j == nSecTypes)
          secType = SecTypes.invalid;
      } else {
        vlog.error("Unknown 3.3 security type "+secType);
        throw new Exception("Unknown 3.3 security type");
      }

    } else {

      // 3.7 server will offer us a list

      nServerSecTypes = is.readU8();
      if (nServerSecTypes == 0)
        throwConnFailedException();
      serverSecTypes = new int[nServerSecTypes];
      for (int i = 0; i < nServerSecTypes; i++) {
        serverSecTypes[i] = is.readU8();
        vlog.debug("Server offers security type "+
                   SecTypes.name(serverSecTypes[i])+"("+serverSecTypes[i]+")");
      }

      for (int j = 0; j < nSecTypes; j++) {
        for (int i = 0; i < nServerSecTypes; i++) {
          if (secTypes[j] == serverSecTypes[i]) {
            secType = secTypes[j];
            os.writeU8(secType);
            os.flush();
            vlog.debug("Choosing security type "+SecTypes.name(secType)+
                       "("+secType+")");
            break;
          }
        }
        if (secType != SecTypes.invalid) break;
      }
    }

    if (secType == SecTypes.invalid) {
      state_ = RFBSTATE_INVALID;
      vlog.error("No matching security types");
      throw new Exception("No matching security types");
    }

    state_ = RFBSTATE_SECURITY;
    security = getCSecurity(secType);
    processSecurityMsg();
  }

  void processSecurityMsg() {
    vlog.debug("processing security message");
    int rc = security.processMsg(this);
    if (rc == 0) {
      state_ = RFBSTATE_INVALID;
      vlog.error("Authentication failure");
      throw new AuthFailureException("Authentication failure");
    }
    if (rc == 1)
      securityCompleted();
  }

  void processInitMsg() {
    vlog.debug("reading server initialisation");
    reader_.readServerInit();
  }

  void throwConnFailedException() {
    state_ = RFBSTATE_INVALID;
    String reason = is.readString();
    throw new ConnFailedException(reason);
  }

  void securityCompleted() {
    state_ = RFBSTATE_INITIALISATION;
    reader_ = new CMsgReaderV3(this, is);
    writer_ = new CMsgWriterV3(cp, os);
    vlog.debug("Authentication success!");
    authSuccess();
    writer_.writeClientInit(shared);
  }

  rdr.InStream is;
  rdr.OutStream os;
  CMsgReader reader_;
  CMsgWriter writer_;
  boolean shared;
  protected CSecurity security;
  public static final int maxSecTypes = 8;
  int nSecTypes;
  int[] secTypes;
  int nServerSecTypes;
  int[] serverSecTypes;
  int state_;

  String serverName;

  boolean useProtocol3_3;

  static LogWriter vlog = new LogWriter("CConnection");
}
