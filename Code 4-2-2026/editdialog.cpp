#include "editdialog.h"
#include "ui_editdialog.h"
#include <QTime>

EditDialog::EditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditDialog)
{
    ui->setupUi(this);

    ui->startTimeEdit->setTimeRange(QTime(8, 0), QTime(20, 0));
    ui->endTimeEdit->setTimeRange(QTime(8, 0), QTime(20, 0));

    // 1. Disable the days group box by default
    ui->weekdayGroupBox->setEnabled(false);

    // 2. Listen for changes in the Edit popup's Frequency Dropdown
    connect(ui->frequencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index){
        // Index 0: Does not repeat
        // Index 1: Everyday
        // Index 2: Every Week
        // Index 3: Every Month
        // Index 4: Every Year
        
        bool isWeekly = (index == 2); 
        
        ui->weekdayGroupBox->setEnabled(isWeekly);
        
        if (!isWeekly) {
            ui->chkMon->setChecked(false);
            ui->chkTue->setChecked(false);
            ui->chkWed->setChecked(false);
            ui->chkThu->setChecked(false);
            ui->chkFri->setChecked(false);
            ui->chkSat->setChecked(false);
            ui->chkSun->setChecked(false);
        }
    });
}

EditDialog::~EditDialog()
{
    delete ui;
}

// Set the dialog's visual fields with the data passed from HomeWindow
void EditDialog::setReservationData(const QString &name, const QString &room, const QDate &date, const QTime &startTime, const QTime &endTime, const QString &frequency, const QDate &endDate, bool isOverwriteable, int byWeekday)
{
    ui->resNameEdit->setText(name);
    ui->roomCombo->addItem(room);
    ui->roomCombo->setCurrentText(room);
    ui->startDateEdit->setDate(date);
    
    // Use setTime() for the new QTimeEdit widgets
    ui->startTimeEdit->setTime(startTime);
    ui->endTimeEdit->setTime(endTime);
    
    // This triggers the connect function above, which enables/disables the box
    ui->frequencyCombo->setCurrentText(frequency);
    
    ui->endDateEdit->setDate(endDate);
    ui->overwriteableCheckBox->setChecked(isOverwriteable);

    // ONLY check the boxes if the event is actually a Weekly repeating event!
    if (frequency == "Every Week") {
        ui->chkMon->setChecked(byWeekday & 1);
        ui->chkTue->setChecked(byWeekday & 2);
        ui->chkWed->setChecked(byWeekday & 4);
        ui->chkThu->setChecked(byWeekday & 8);
        ui->chkFri->setChecked(byWeekday & 16);
        ui->chkSat->setChecked(byWeekday & 32);
        ui->chkSun->setChecked(byWeekday & 64);
    }
}

// Data Getters
QString EditDialog::getUpdatedName() const { return ui->resNameEdit->text(); }
QString EditDialog::getUpdatedRoom() const { return ui->roomCombo->currentText(); }
QDate EditDialog::getUpdatedDate() const { return ui->startDateEdit->date(); }
QTime EditDialog::getUpdatedStartTime() const { return ui->startTimeEdit->time(); } // Grabs QTime directly
QTime EditDialog::getUpdatedEndTime() const { return ui->endTimeEdit->time(); }     // Grabs QTime directly
QString EditDialog::getUpdatedFrequency() const { return ui->frequencyCombo->currentText(); }
QDate EditDialog::getUpdatedEndDate() const { return ui->endDateEdit->date(); }
bool EditDialog::getUpdatedOverwriteable() const { return ui->overwriteableCheckBox->isChecked(); }

int EditDialog::getUpdatedByWeekday() const {
    int mask = 0;
    if (ui->chkMon->isChecked()) mask |= 1;
    if (ui->chkTue->isChecked()) mask |= 2;
    if (ui->chkWed->isChecked()) mask |= 4;
    if (ui->chkThu->isChecked()) mask |= 8;
    if (ui->chkFri->isChecked()) mask |= 16;
    if (ui->chkSat->isChecked()) mask |= 32;
    if (ui->chkSun->isChecked()) mask |= 64;
    return mask;
}