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

package vncviewer;

import java.awt.*;

class ServerDialog extends vncviewer.Dialog {

  public ServerDialog(OptionsDialog options_,
                      AboutDialog about_, String defaultServerName) {
    super(true);
    setTitle("VNC Viewer: Connection Details");
    options = options_;
    about = about_;
    Panel p1 = new Panel();
    label = new Label("VNC server:");
    p1.add(label);
    entry = new TextField(30);
    if (defaultServerName != null) entry.setText(defaultServerName);
    p1.add(entry);
    add("Center", p1);

    Panel p2 = new Panel();
    p2.setLayout(new FlowLayout(FlowLayout.RIGHT));
    aboutButton = new Button("About...");
    optionsButton = new Button("Options...");
    okButton = new Button("OK");
    cancelButton = new Button("Cancel");
    p2.add(aboutButton);
    p2.add(optionsButton);
    p2.add(okButton);
    p2.add(cancelButton);
    add("South", p2);

    pack();
  }

  synchronized public boolean action(Event event, Object arg) {
    if (event.target == okButton || event.target == entry) {
      ok = true;
      endDialog();
    } else if (event.target == cancelButton) {
      ok = false;
      endDialog();
    } else if (event.target == optionsButton) {
      options.showDialog();
    } else if (event.target == aboutButton) {
      about.showDialog();
    }
    return true;
  }

  Label label;
  TextField entry;
  Button aboutButton, optionsButton, okButton, cancelButton;
  OptionsDialog options;
  AboutDialog about;
}
