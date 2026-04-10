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

    connect(ui->startTimeEdit, &QTimeEdit::timeChanged, this, [this](const QTime &t) {
        if (ui->endTimeEdit->time() <= t)
            ui->endTimeEdit->setTime(t.addSecs(1800));
    });

    connect(ui->endTimeEdit, &QTimeEdit::timeChanged, this, [this](const QTime &t) {
        if (ui->startTimeEdit->time() >= t)
            ui->startTimeEdit->setTime(t.addSecs(-1800));
    });

    ui->weekdayGroupBox->setEnabled(false);

    connect(ui->frequencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
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

    connect(ui->startDateEdit, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        if (ui->endDateEdit->date() < d)
            ui->endDateEdit->setDate(d);
    });

    connect(ui->endDateEdit, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        if (ui->startDateEdit->date() > d)
            ui->startDateEdit->setDate(d);
    });
}

EditDialog::~EditDialog()
{
    delete ui;
}

void EditDialog::setAvailableRooms(const QStringList &rooms, const QString &currentRoom)
{
    ui->roomCombo->clear();
    ui->roomCombo->addItems(rooms);
    ui->roomCombo->setCurrentText(currentRoom);
}

void EditDialog::setReservationData(const QString &name, const QString &room, const QDate &date,
                                    const QTime &startTime, const QTime &endTime,
                                    const QString &frequency, const QDate &endDate, int byWeekday)
{
    ui->resNameEdit->setText(name);
    if (ui->roomCombo->findText(room) == -1)
        ui->roomCombo->addItem(room);
    ui->roomCombo->setCurrentText(room);
    ui->startDateEdit->setDate(date);
    ui->startTimeEdit->setTime(startTime);
    ui->endTimeEdit->setTime(endTime);
    ui->frequencyCombo->setCurrentText(frequency);
    ui->endDateEdit->setDate(endDate);

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

QString EditDialog::getUpdatedName() const { return ui->resNameEdit->text(); }
QString EditDialog::getUpdatedRoom() const { return ui->roomCombo->currentText(); }
QDate EditDialog::getUpdatedDate() const { return ui->startDateEdit->date(); }
QTime EditDialog::getUpdatedStartTime() const { return ui->startTimeEdit->time(); }
QTime EditDialog::getUpdatedEndTime() const { return ui->endTimeEdit->time(); }
QString EditDialog::getUpdatedFrequency() const { return ui->frequencyCombo->currentText(); }
QDate EditDialog::getUpdatedEndDate() const { return ui->endDateEdit->date(); }

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
