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
// PixelBufferImage is an rfb.PixelBuffer which also acts as an ImageProducer.
// Currently it only supports 8-bit colourmapped pixel format.
//

package vncviewer;

import java.awt.*;
import java.awt.image.*;

public class PixelBufferImage extends rfb.PixelBuffer implements ImageProducer
{
  public PixelBufferImage(int w, int h, java.awt.Component win) {
    setPF(new rfb.PixelFormat(32, 24, true, true, 0xff, 0xff, 0xff, 16, 8, 0));

    resize(w, h, win);

    cm = new DirectColorModel(24, 0xff << 16, 0xff << 8, 0xff);
  }

  // resize() resizes the image, preserving the image data where possible.
  public void resize(int w, int h, java.awt.Component win) {
    if (w == width() && h == height()) return;

    int rowsToCopy = h < height() ? h : height();
    int copyWidth = w < width() ? w : width();
    int oldWidth = width();
    int[] oldData = data;

    width_ = w;
    height_ = h;
    image = win.createImage(this);

    data = new int[width() * height()];

    for (int i = 0; i < rowsToCopy; i++)
      System.arraycopy(oldData, oldWidth * i,
		       data, width() * i,
		       copyWidth);
  }

  // put() causes the given rectangle to be drawn using the given graphics
  // context.
  public void put(int x, int y, int w, int h, Graphics g) {
    if (ic != null) {
      ic.setPixels(x, y, w, h, cm, data, width() * y + x, width());
      ic.imageComplete(ImageConsumer.SINGLEFRAMEDONE);
    }
    g.setClip(x, y, w, h);
    g.drawImage(image, 0, 0, null);
  }

  // fillRect(), imageRect(), maskRect() are inherited from PixelBuffer.  For
  // copyRect() we also need to tell the ImageConsumer that the pixels have
  // changed (this is done in the put() call for the others).

  public void copyRect(int x, int y, int w, int h, int srcX, int srcY) {
    super.copyRect(x, y, w, h, srcX, srcY);
    if (ic != null) {
      ic.setPixels(x, y, w, h, cm, data, width() * y + x, width());
      ic.imageComplete(ImageConsumer.SINGLEFRAMEDONE);
    }
  }

  // ImageProducer methods

  public void addConsumer(ImageConsumer c) {
    if (ic == c) return;

    vlog.debug("adding consumer "+c);

    if (ic != null)
      vlog.error("Only one ImageConsumer allowed - discarding old one");

    ic = c;
    ic.setDimensions(width(), height());
    ic.setHints(ImageConsumer.RANDOMPIXELORDER);
    // Calling ic.setColorModel(cm) seemed to help in some earlier versions of
    // the JDK, but it shouldn't be necessary because we pass the ColorModel
    // with each setPixels() call.
    ic.setPixels(0, 0, width(), height(), cm, data, 0, width());
    ic.imageComplete(ImageConsumer.SINGLEFRAMEDONE);
  }

  public void removeConsumer(ImageConsumer c) {
    System.err.println("removeConsumer "+c);
    if (ic == c) ic = null;
  }

  public boolean isConsumer(ImageConsumer c) { return ic == c; }
  public void requestTopDownLeftRightResend(ImageConsumer c) {}
  public void startProduction(ImageConsumer c) { addConsumer(c); }

  Image image;
  ImageConsumer ic;
  ColorModel cm;

  static rfb.LogWriter vlog = new rfb.LogWriter("PixelBufferImage");
}
