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
// SecTypes.java - constants for the various security types.
//

package rfb;

public class SecTypes {
  public static final int invalid = 0;
  public static final int none = 1;
  public static final int vncAuth = 2;
  public static final int TLS = 18;

  public static String name(int num) {
    switch (num) {
    case none:    return "None";
    case vncAuth: return "VncAuth";
    case TLS:     return "TLS";
    default:      return "[unknown secType]";
    }
  }
  public static int num(String name) {
    if (name.equalsIgnoreCase("None"))    return none;
    if (name.equalsIgnoreCase("VncAuth")) return vncAuth;
    if (name.equalsIgnoreCase("TTLS"))    return TLS;
    return invalid;
  }
  //std::list<int> parseSecTypes(const char* types);
}
