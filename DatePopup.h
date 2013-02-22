 /*
     * DatePopup.h
     *
     *  Created on: Aug 29, 2009
     *      Author: jordenysp
     */

#ifndef DATEPOPUP_H_
#define DATEPOPUP_H_

#include <QDialog>
#include <QDate>
class QCalendarWidget;
class QTimeEdit;
class QDialogButtonBox;
class QVBoxLayout;

class DatePopup : public QDialog{
    Q_OBJECT
public:
    DatePopup(QWidget *parent=0);
    QDate selectedDate() const;
    QTime selectedTime() const;

private:
    QWidget *widget;
    QCalendarWidget *calendarWidget;
    QTimeEdit       *timeWidget;

    QDialogButtonBox* buttonBox;
    QVBoxLayout *verticalLayout;

};

#endif /* DATEPOPUP_H_ */