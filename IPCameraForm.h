#ifndef IPCAMERAFORM_H
#define IPCAMERAFORM_H

#include "ui_IPCameraForm.h"
//
#include <map>
#include <string>
//
typedef std::pair< std::string, std::string > stringpair;
typedef std::map< std::string, std::string > stringpairlist;

class IPCameraForm : public QDialog {
Q_OBJECT;
public:
  IPCameraForm(QWidget* parent);
  //
  void seupModelView();
  void loadSettings();
  void saveSettings();

  stringpairlist& getList() { return this->CameraList; }

signals:

protected:
  Ui::IPCameraForm    ui;
  QString             settingsFileName;
  stringpairlist      CameraList;
};

#endif
