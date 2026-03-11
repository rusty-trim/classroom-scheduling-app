#include "editdialog.h"
#include "ui_editdialog.h"
#include <QTime>

EditDialog::EditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditDialog)
{
    ui->setupUi(this);
    populateTimeComboBoxes();
}

EditDialog::~EditDialog()
{
    delete ui;
}

void EditDialog::populateTimeComboBoxes()
{
    QTime time(0, 0);
    for (int i = 0; i < 48; ++i) {
        QString timeString = time.toString("hh:mm AP");
        ui->startTimeCombo->addItem(timeString);
        ui->endTimeCombo->addItem(timeString);
        time = time.addSecs(30 * 60);
    }
}

// Set the dialog's visual fields with the data passed from HomeWindow
void EditDialog::setReservationData(const QString &name, const QString &room, const QDate &date, const QString &startTime, const QString &endTime, const QString &recurrence, const QDate &endDate, bool isOverwriteable)
{
    ui->resNameEdit->setText(name);
    ui->roomCombo->addItem(room);
    ui->roomCombo->setCurrentText(room);
    ui->startDateEdit->setDate(date);
    ui->startTimeCombo->setCurrentText(startTime);
    ui->endTimeCombo->setCurrentText(endTime);
    ui->recurrenceCombo->setCurrentText(recurrence);
    ui->endDateEdit->setDate(endDate);
    ui->overwriteableCheckBox->setChecked(isOverwriteable);
}

// Data Getters
QString EditDialog::getUpdatedName() const { return ui->resNameEdit->text(); }
QString EditDialog::getUpdatedRoom() const { return ui->roomCombo->currentText(); }
QDate EditDialog::getUpdatedDate() const { return ui->startDateEdit->date(); }
QString EditDialog::getUpdatedStartTime() const { return ui->startTimeCombo->currentText(); }
QString EditDialog::getUpdatedEndTime() const { return ui->endTimeCombo->currentText(); }
QString EditDialog::getUpdatedRecurrence() const { return ui->recurrenceCombo->currentText(); }
QDate EditDialog::getUpdatedEndDate() const { return ui->endDateEdit->date(); }
bool EditDialog::getUpdatedOverwriteable() const { return ui->overwriteableCheckBox->isChecked(); }
