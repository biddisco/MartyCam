
/*
 * DatePopup.cpp
 *
 *  Created on: Aug 29, 2009
 *      Author: jordenysp
 */

#include <QtGui>
#include "DatePopup.h"

DatePopup::DatePopup(QWidget *parent) : QDialog(parent, Qt::Popup)
{
    setSizeGripEnabled(false);
    resize(260, 230);
    widget = new QWidget(this);
    widget->setObjectName(QString::fromUtf8("widget"));
    widget->setGeometry(QRect(0, 10, 258, 215));

    verticalLayout = new QVBoxLayout(widget);
    verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    calendarWidget = new QCalendarWidget(widget);
    calendarWidget->setObjectName(QString::fromUtf8("calendarWidget"));

    timeWidget = new QTimeEdit(widget);
    timeWidget->setObjectName(QString::fromUtf8("timeWidget"));
    timeWidget->setTime(QTime::currentTime());

    verticalLayout->addWidget(timeWidget);
    verticalLayout->addWidget(calendarWidget);

    buttonBox = new QDialogButtonBox(widget);
    buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
    buttonBox->setOrientation(Qt::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

    verticalLayout->addWidget(buttonBox);

    QObject::connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QDate DatePopup::selectedDate() const{
    return calendarWidget->selectedDate();
}

QTime DatePopup::selectedTime() const
{
  return timeWidget->time();
}
