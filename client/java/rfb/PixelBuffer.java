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
// PixelBuffer - note that this code is only written for the 32bpp case at the
// moment.
//

package rfb;

public class PixelBuffer {

  public PixelBuffer() {
    setPF(new PixelFormat());
  }

  public void setPF(PixelFormat pf) {
    if (pf.bpp != 32)
      throw new rfb.Exception("Internal error: bpp must be 32 in PixelBuffer");
    format = pf;
  }
  public PixelFormat getPF() { return format; }

  public final int width() { return width_; }
  public final int height() { return height_; }
  public final int area() { return width_ * height_; }

  public void fillRect(int x, int y, int w, int h, int pix) {
    for (int ry = y; ry < y + h; ry++)
      for (int rx = x; rx < x + w; rx++)
        data[ry * width_ + rx] = pix;
  }

  public void imageRect(int x, int y, int w, int h, int[] pix) {
    for (int j = 0; j < h; j++)
      System.arraycopy(pix, (w * j), data, width_ * (y + j) + x, w);
  }

  public void copyRect(int x, int y, int w, int h, int srcX, int srcY) {
    int dest = (width_ * y) + x;
    int src = (width_ * srcY) + srcX;
    int inc = width_;

    if (y > srcY) {
      src  += (h - 1) * inc;
      dest += (h - 1) * inc;
      inc = -inc;
    }
    int destEnd = dest + h * inc;

    while (dest != destEnd) {
      System.arraycopy(data, src, data, dest, w);
      src  += inc;
      dest += inc;
    }
  }

  public void maskRect(int x, int y, int w, int h, int[] pix, byte[] mask) {
    int maskBytesPerRow = (w + 7) / 8;

    for (int j = 0; j < h; j++) {
      int cy = y + j;

      if (cy < 0 || cy >= height_)
	continue;

      for (int i = 0; i < w; i++) {
	int cx = x + i;

	if (cx < 0 || cx >= width_)
	  continue;

	int byte_ = j * maskBytesPerRow + i / 8;
	int bit = 7 - i % 8;

	if ((mask[byte_] & (1 << bit)) != 0)
	  data[cy * width_ + cx] = pix[j * w + i];
      }
    }
  }

  public int[] data;

  protected PixelFormat format;
  protected int width_, height_;
}
