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

class OptionsDialog extends vncviewer.Dialog {

  public OptionsDialog(OptionsDialogCallback cb_) { 
    super(false);
    cb = cb_;
    setTitle("VNC Viewer: Connection Options");

    mainGB = new GridBagLayout();
    setLayout(mainGB);
    mainGBC = new GridBagConstraints();
    mainGBC.gridwidth = GridBagConstraints.REMAINDER;
    mainGBC.anchor = GridBagConstraints.WEST;
    mainGBC.ipadx = 2;
    mainGBC.ipady = 2;

    startPanel("Format and Encoding:");
    autoSelect = addCheckbox("Auto select");
    fullColour = addCheckbox("Full colour");
    encodingGroup = new CheckboxGroup();
    zrle = addRadioCheckbox("ZRLE", encodingGroup);
    hextile = addRadioCheckbox("Hextile", encodingGroup);
    raw = addRadioCheckbox("Raw", encodingGroup);

    startPanel("Inputs:");
    viewOnly = addCheckbox("View only (ignore mouse & keyboard)");
    acceptClipboard = addCheckbox("Accept clipboard from server");
    sendClipboard = addCheckbox("Send clipboard to server");

    startPanel("Misc:");
    shared = addCheckbox("Shared (don't disconnect other viewers)");
    useLocalCursor = addCheckbox("Render cursor locally");

    Panel pb = new Panel();
    okButton = new Button("OK");
    pb.add(okButton);
    cancelButton = new Button("Cancel");
    pb.add(cancelButton);
    add(pb);

    pack();
  }

  public void initDialog() {
    if (cb != null) cb.setOptions();
    zrle.setEnabled(!autoSelect.getState());
    hextile.setEnabled(!autoSelect.getState());
    raw.setEnabled(!autoSelect.getState());
  }

  void startPanel(String title) {
    panelGB = new GridBagLayout();
    panel = new Panel(panelGB);
    mainGB.setConstraints(panel, mainGBC);
    add(panel);

    panelGBC = new GridBagConstraints();
    panelGBC.gridwidth = GridBagConstraints.REMAINDER;
    panelGBC.anchor = GridBagConstraints.WEST;
    panelGBC.ipadx = 2;
    panelGBC.ipady = 2;
    Label l = new Label(title);
    panelGB.setConstraints(l, panelGBC);
    panel.add(l);
  }

  Checkbox addCheckbox(String str) {
    Checkbox c = new Checkbox(str);
    panelGB.setConstraints(c, panelGBC);
    panel.add(c);
    return c;
  }

  Checkbox addRadioCheckbox(String str, CheckboxGroup group) {
    Checkbox c = new Checkbox(str, group, false);
    panelGB.setConstraints(c, panelGBC);
    panel.add(c);
    return c;
  }

  public boolean action(Event event, Object arg) {
    if (event.target == okButton) {
      ok = true;
      if (cb != null) cb.getOptions();
      endDialog();
    } else if (event.target == cancelButton) {
      ok = false;
      endDialog();
    } else if (event.target == autoSelect) {
      zrle.setEnabled(!autoSelect.getState());
      hextile.setEnabled(!autoSelect.getState());
      raw.setEnabled(!autoSelect.getState());
    }
    return true;
  }

  OptionsDialogCallback cb;
  GridBagLayout mainGB;
  GridBagConstraints mainGBC;
  GridBagLayout panelGB;
  GridBagConstraints panelGBC;
  Panel panel;
  Checkbox autoSelect, fullColour;
  CheckboxGroup encodingGroup;
  Checkbox zrle, hextile, raw;
  Checkbox viewOnly, acceptClipboard, sendClipboard;
  Checkbox shared, useLocalCursor;
  Button okButton, cancelButton;
}
