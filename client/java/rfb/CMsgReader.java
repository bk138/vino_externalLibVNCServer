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
//
// CMsgReader - class for reading RFB messages on the client side
// (i.e. messages from server to client).
//

package rfb;

abstract public class CMsgReader {

  abstract public void readServerInit();

  // readMsg() reads a message, calling the handler as appropriate.
  abstract public void readMsg();

  public rdr.InStream getInStream() { return is; }

  public int[] getImageBuf(int size) {
    if (imageBufSize < size) {
      imageBufSize = size;
      imageBuf = new int[imageBufSize];
    }
    return imageBuf;
  }

  public final int bpp() { return handler.cp.pf().bpp; }

  protected CMsgReader(CMsgHandler handler_, rdr.InStream is_) {
    handler = handler_;
    is = is_;
    decoders = new Decoder[Encodings.max+1];
  }

  protected void readSetColourMapEntries() {
    is.skip(1);
    int firstColour = is.readU16();
    int nColours = is.readU16();
    int[] rgbs = new int[nColours * 3];
    for (int i = 0; i < nColours * 3; i++)
      rgbs[i] = is.readU16();
    endMsg();
    handler.setColourMapEntries(firstColour, nColours, rgbs);
  }

  protected void readBell() {
    endMsg();
    handler.bell();
  }

  protected void readServerCutText() {
    is.skip(3);
    int len = is.readU32();
    if (len > 256*1024) {
      is.skip(len);
      vlog.error("cut text too long ("+len+" bytes) - ignoring");
      return;
    }
    byte[] buf = new byte[len];
    is.readBytes(buf, 0, len);
    endMsg();
    handler.serverCutText(new String(buf, 0, len));
  }

  protected void endMsg() {}

  protected void readFramebufferUpdateStart() {
    endMsg();
    handler.framebufferUpdateStart();
  }

  protected void readFramebufferUpdateEnd() {
    endMsg();
    handler.framebufferUpdateEnd();
  }

  protected void readRect(int x, int y, int w, int h, int encoding) {
    if ((x+w > handler.cp.width) || (y+h > handler.cp.height)) {
      vlog.error("Rect too big: "+w+"x"+h+" at "+x+","+y+" exceeds "+
                 handler.cp.width+"x"+handler.cp.height);
      throw new Exception("Rect too big");
    }

    if (w*h == 0) {
      vlog.info("Ignoring zero size rect");
      return;
    }

    handler.beginRect(x,y,w,h, encoding);

    if (encoding == Encodings.copyRect) {
      readCopyRect(x,y,w,h);
    } else {
      if (decoders[encoding] == null) {
        decoders[encoding] = Decoder.createDecoder(encoding, this);
        if (decoders[encoding] == null) {
          vlog.error("Unknown rect encoding "+encoding);
          throw new Exception("Unknown rect encoding");
        }
      }
      decoders[encoding].readRect(x,y,w,h, handler);
    }

    handler.endRect(x,y,w,h, encoding);
  }

  protected void readCopyRect(int x, int y, int w, int h) {
    int srcX = is.readU16();
    int srcY = is.readU16();
    handler.copyRect(x,y,w,h, srcX, srcY);
  }

  protected void readSetCursor(int hotspotX, int hotspotY, int w, int h) {
    int data_len = w * h;
    int mask_len = ((w+7)/8) * h;
    int[] data = new int[data_len];
    byte[] mask = new byte[mask_len];

    is.readPixels(data, data_len);
    is.readBytes(mask, 0, mask_len);

    handler.setCursor(hotspotX, hotspotY, w, h, data, mask);
  }

  CMsgHandler handler;
  rdr.InStream is;
  Decoder[] decoders;
  int[] imageBuf;
  int imageBufSize;

  static LogWriter vlog = new LogWriter("CMsgReader");
}
