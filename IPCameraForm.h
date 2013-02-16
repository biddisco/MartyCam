#ifndef IPCAMERAFORM_H
#define IPCAMERAFORM_H

#include "ui_IPCameraForm.h"
//
#include <vector>
#include <string>
//
typedef std::pair< std::string, std::string> stringpair;
typedef std::vector< stringpair > stringpairlist;

class IPCameraForm : public QDialog {
Q_OBJECT;
public:
  IPCameraForm(QWidget* parent);
  //
  void loadSettings();
  void saveSettings();

signals:

protected:
  Ui::IPCameraForm    ui;
  QString             settingsFileName;
  stringpairlist      CameraList;
};

#endif
