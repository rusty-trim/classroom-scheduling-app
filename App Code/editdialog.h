#ifndef EDITDIALOG_H
#define EDITDIALOG_H

#include <QDialog>
#include <QDate>
#include <QTime>

namespace Ui { class EditDialog; }

class EditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditDialog(QWidget *parent = nullptr);
    ~EditDialog();

    void setReservationData(const QString &name, const QString &room, const QDate &date, const QTime &startTime, const QTime &endTime, const QString &frequency, const QDate &endDate, bool isOverwriteable, int byWeekday);

    QString getUpdatedName() const;
    QString getUpdatedRoom() const;
    QDate getUpdatedDate() const;
    QTime getUpdatedStartTime() const;
    QTime getUpdatedEndTime() const;
    QString getUpdatedFrequency() const;
    QDate getUpdatedEndDate() const;
    bool getUpdatedOverwriteable() const;
    int getUpdatedByWeekday() const;

private:
    Ui::EditDialog *ui;
};

#endif // EDITDIALOG_H
