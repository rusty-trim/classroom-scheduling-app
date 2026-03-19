#ifndef EDITDIALOG_H
#define EDITDIALOG_H

#include <QDialog>
#include <QDate>

namespace Ui { class EditDialog; }

class EditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditDialog(QWidget *parent = nullptr);
    ~EditDialog();

    // Removed the 'id' parameter from this list
    void setReservationData(const QString &name, const QString &room, const QDate &date, const QString &startTime, const QString &endTime, const QString &recurrence, const QDate &endDate, bool isOverwriteable);

    QString getUpdatedName() const;
    QString getUpdatedRoom() const;
    QDate getUpdatedDate() const;
    QString getUpdatedStartTime() const;
    QString getUpdatedEndTime() const;
    QString getUpdatedRecurrence() const;
    QDate getUpdatedEndDate() const;
    bool getUpdatedOverwriteable() const;

private:
    Ui::EditDialog *ui;
    void populateTimeComboBoxes();
};

#endif // EDITDIALOG_H
