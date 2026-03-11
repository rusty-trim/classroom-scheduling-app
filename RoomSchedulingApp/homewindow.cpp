#include "homewindow.h"
#include "ui_homewindow.h"
#include "editdialog.h"
#include <QTime>
#include <QDate>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QVariant>
#include <QDebug>

// --- SETUP & CONSTRUCTOR ---

HomeWindow::HomeWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::HomeWindow)
{
    ui->setupUi(this);

    // 1. Setup Database First
    if (!setupDatabase()) {
        QMessageBox::critical(this, "Database Error", "Failed to connect to the database. Your data will not be saved.");
    }

    populateTimeComboBoxes();

    ui->startDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDate(QDate::currentDate());
    ui->scheduleDateEdit->setDate(QDate::currentDate());
    ui->filterDateEdit->setDate(QDate::currentDate());

    ui->filterRoomComboBox->addItem("All Rooms");
    ui->filterDateEdit->setEnabled(false);

    ui->reservationTable->setColumnCount(9);
    ui->reservationTable->setHorizontalHeaderLabels({"ID", "Name", "Room", "Start Date", "Start Time", "End Time", "End Date", "Recurrence", "Overwriteable"});

    ui->reservationTable->hideColumn(0); // Hide Database ID
    ui->reservationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->reservationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->reservationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->reservationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    ui->reservationTable->setColumnWidth(1, 200);
    ui->reservationTable->setColumnWidth(2, 120);
    ui->reservationTable->setColumnWidth(3, 110);
    ui->reservationTable->setColumnWidth(4, 100);
    ui->reservationTable->setColumnWidth(5, 100);
    ui->reservationTable->setColumnWidth(6, 110);
    ui->reservationTable->setColumnWidth(7, 130);
    ui->reservationTable->setColumnWidth(8, 110);

    // 2. Load Existing Data from Database into the UI
    loadRoomsFromDatabase();
    loadReservationsFromDatabase();
}

HomeWindow::~HomeWindow()
{
    db.close();
    delete ui;
}

// --- DATABASE INITIALIZATION & LOADING ---

bool HomeWindow::setupDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("reservations.db"); // Creates a local file named reservations.db

    if (!db.open()) {
        return false;
    }

    QSqlQuery query;
    // Create Rooms Table
    query.exec("CREATE TABLE IF NOT EXISTS rooms ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT UNIQUE)");

    // Create Reservations Table
    query.exec("CREATE TABLE IF NOT EXISTS reservations ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT, "
               "room TEXT, "
               "start_date TEXT, "
               "start_time TEXT, "
               "end_time TEXT, "
               "end_date TEXT, "
               "recurrence TEXT, "
               "overwriteable INTEGER)"); // SQLite uses 1 for True, 0 for False

    return true;
}

void HomeWindow::loadRoomsFromDatabase()
{
    QSqlQuery query("SELECT name FROM rooms");
    while (query.next()) {
        QString roomName = query.value(0).toString();
        ui->roomComboBox->addItem(roomName);
        ui->filterRoomComboBox->addItem(roomName);
    }
}

void HomeWindow::loadReservationsFromDatabase()
{
    QSqlQuery query("SELECT id, name, room, start_date, start_time, end_time, end_date, recurrence, overwriteable FROM reservations");
    while (query.next()) {
        int newRow = ui->reservationTable->rowCount();
        ui->reservationTable->insertRow(newRow);

        ui->reservationTable->setItem(newRow, 0, new QTableWidgetItem(query.value(0).toString())); // ID
        ui->reservationTable->setItem(newRow, 1, new QTableWidgetItem(query.value(1).toString())); // Name
        ui->reservationTable->setItem(newRow, 2, new QTableWidgetItem(query.value(2).toString())); // Room
        ui->reservationTable->setItem(newRow, 3, new QTableWidgetItem(query.value(3).toString())); // Start Date
        ui->reservationTable->setItem(newRow, 4, new QTableWidgetItem(query.value(4).toString())); // Start Time
        ui->reservationTable->setItem(newRow, 5, new QTableWidgetItem(query.value(5).toString())); // End Time
        ui->reservationTable->setItem(newRow, 6, new QTableWidgetItem(query.value(6).toString())); // End Date
        ui->reservationTable->setItem(newRow, 7, new QTableWidgetItem(query.value(7).toString())); // Recurrence

        bool isOverwriteable = query.value(8).toInt() == 1;
        ui->reservationTable->setItem(newRow, 8, new QTableWidgetItem(isOverwriteable ? "Yes" : "No")); // Overwriteable
    }
}

void HomeWindow::populateTimeComboBoxes()
{
    ui->startTimeComboBox->clear();
    ui->endTimeComboBox->clear();

    QTime time(0, 0);
    for (int i = 0; i < 48; ++i) {
        QString timeString = time.toString("hh:mm AP");
        ui->startTimeComboBox->addItem(timeString);
        ui->endTimeComboBox->addItem(timeString);
        time = time.addSecs(30 * 60);
    }
}

// --- ROOM MANAGEMENT ---

void HomeWindow::on_btnCreateRoom_clicked()
{
    QString roomName = ui->roomInput->text();
    if (roomName.isEmpty()) return;

    QString fullRoomName = "CAS - " + roomName;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Creation",
                                                              "Are you sure you want to create Room " + fullRoomName + "?",
                                                              QMessageBox::Yes | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        // SAVE TO DATABASE
        QSqlQuery query;
        query.prepare("INSERT INTO rooms (name) VALUES (:name)");
        query.bindValue(":name", fullRoomName);

        if (query.exec()) {
            QMessageBox::information(this, "Success", "Room created successfully.");
            ui->roomComboBox->addItem(fullRoomName);
            ui->filterRoomComboBox->addItem(fullRoomName);
            ui->roomInput->clear();
        } else {
            QMessageBox::warning(this, "Error", "Room already exists or database error.");
        }
    }
}

void HomeWindow::on_btnDeleteRoom_clicked()
{
    QString roomName = ui->roomInput->text();
    if (roomName.isEmpty()) return;

    QString fullRoomName = "CAS - " + roomName;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Deletion",
                                                              "Are you sure you want to delete Room " + fullRoomName + "?\nThis cannot be undone.",
                                                              QMessageBox::Yes | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        // DELETE FROM DATABASE
        QSqlQuery query;
        query.prepare("DELETE FROM rooms WHERE name = :name");
        query.bindValue(":name", fullRoomName);

        if (query.exec()) {
            QMessageBox::information(this, "Success", "Room deleted successfully.");

            int createIdx = ui->roomComboBox->findText(fullRoomName);
            if (createIdx != -1) ui->roomComboBox->removeItem(createIdx);

            int filterIdx = ui->filterRoomComboBox->findText(fullRoomName);
            if (filterIdx != -1) ui->filterRoomComboBox->removeItem(filterIdx);

            ui->roomInput->clear();
        }
    }
}

// --- RESERVATION MANAGEMENT ---

void HomeWindow::on_btnCreateReservation_clicked()
{
    QString resName = ui->resNameInput->text();
    QString room = ui->roomComboBox->currentText();
    QString startDate = ui->startDateEdit->date().toString("MM/dd/yyyy");
    QString startTime = ui->startTimeComboBox->currentText();
    QString endTime = ui->endTimeComboBox->currentText();
    QString endDate = ui->endDateEdit->date().toString("MM/dd/yyyy");
    QString recurrence = ui->recurrenceComboBox->currentText();
    bool isOverwriteable = ui->overwriteCheckBox->isChecked();
    int dbOverwriteable = isOverwriteable ? 1 : 0; // Convert to int for DB

    if (resName.isEmpty() || room.isEmpty()) {
        QMessageBox::warning(this, "Missing Info", "Please provide a room and reservation name.");
        return;
    }

    // 1. SAVE TO DATABASE
    QSqlQuery query;
    query.prepare("INSERT INTO reservations (name, room, start_date, start_time, end_time, end_date, recurrence, overwriteable) "
                  "VALUES (:name, :room, :start_date, :start_time, :end_time, :end_date, :recurrence, :overwriteable)");
    query.bindValue(":name", resName);
    query.bindValue(":room", room);
    query.bindValue(":start_date", startDate);
    query.bindValue(":start_time", startTime);
    query.bindValue(":end_time", endTime);
    query.bindValue(":end_date", endDate);
    query.bindValue(":recurrence", recurrence);
    query.bindValue(":overwriteable", dbOverwriteable);

    if (query.exec()) {
        // Grab the actual ID the database assigned it
        QString newId = query.lastInsertId().toString();

        // 2. ADD TO VISUAL TABLE
        int newRow = ui->reservationTable->rowCount();
        ui->reservationTable->insertRow(newRow);

        ui->reservationTable->setItem(newRow, 0, new QTableWidgetItem(newId)); // Hidden ID
        ui->reservationTable->setItem(newRow, 1, new QTableWidgetItem(resName));
        ui->reservationTable->setItem(newRow, 2, new QTableWidgetItem(room));
        ui->reservationTable->setItem(newRow, 3, new QTableWidgetItem(startDate));
        ui->reservationTable->setItem(newRow, 4, new QTableWidgetItem(startTime));
        ui->reservationTable->setItem(newRow, 5, new QTableWidgetItem(endTime));
        ui->reservationTable->setItem(newRow, 6, new QTableWidgetItem(endDate));
        ui->reservationTable->setItem(newRow, 7, new QTableWidgetItem(recurrence));
        ui->reservationTable->setItem(newRow, 8, new QTableWidgetItem(isOverwriteable ? "Yes" : "No"));

        QMessageBox::information(this, "Success", "Reservation created!");
        ui->resNameInput->clear();
        applyFilters();
    } else {
        QMessageBox::critical(this, "Error", "Failed to save reservation to database.");
    }
}

void HomeWindow::on_btnEditReservation_clicked()
{
    int currentRow = ui->reservationTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Select Reservation", "Please click on a reservation in the table to edit it.");
        return;
    }

    QString resId = ui->reservationTable->item(currentRow, 0)->text();
    QString currentName = ui->reservationTable->item(currentRow, 1)->text();
    QString currentRoom = ui->reservationTable->item(currentRow, 2)->text();
    QDate currentDate = QDate::fromString(ui->reservationTable->item(currentRow, 3)->text(), "MM/dd/yyyy");
    QString currentStart = ui->reservationTable->item(currentRow, 4)->text();
    QString currentEnd = ui->reservationTable->item(currentRow, 5)->text();
    QDate currentEndDate = QDate::fromString(ui->reservationTable->item(currentRow, 6)->text(), "MM/dd/yyyy");
    QString currentRecurrence = ui->reservationTable->item(currentRow, 7)->text();
    bool currentOverwriteable = (ui->reservationTable->item(currentRow, 8)->text() == "Yes");

    EditDialog editPopup(this);
    editPopup.setReservationData(currentName, currentRoom, currentDate, currentStart, currentEnd, currentRecurrence, currentEndDate, currentOverwriteable);

    if (editPopup.exec() == QDialog::Accepted) {

        // Grab the new data from the popup
        QString newName = editPopup.getUpdatedName();
        QString newRoom = editPopup.getUpdatedRoom();
        QString newDate = editPopup.getUpdatedDate().toString("MM/dd/yyyy");
        QString newStart = editPopup.getUpdatedStartTime();
        QString newEnd = editPopup.getUpdatedEndTime();
        QString newEndDate = editPopup.getUpdatedEndDate().toString("MM/dd/yyyy");
        QString newRecurrence = editPopup.getUpdatedRecurrence();
        int newOverwriteDb = editPopup.getUpdatedOverwriteable() ? 1 : 0;
        QString newOverwriteUI = editPopup.getUpdatedOverwriteable() ? "Yes" : "No";

        // 1. UPDATE THE DATABASE
        QSqlQuery query;
        query.prepare("UPDATE reservations SET name = :name, room = :room, start_date = :start_date, "
                      "start_time = :start_time, end_time = :end_time, end_date = :end_date, "
                      "recurrence = :recurrence, overwriteable = :overwriteable WHERE id = :id");
        query.bindValue(":name", newName);
        query.bindValue(":room", newRoom);
        query.bindValue(":start_date", newDate);
        query.bindValue(":start_time", newStart);
        query.bindValue(":end_time", newEnd);
        query.bindValue(":end_date", newEndDate);
        query.bindValue(":recurrence", newRecurrence);
        query.bindValue(":overwriteable", newOverwriteDb);
        query.bindValue(":id", resId);

        if (query.exec()) {
            // 2. UPDATE THE VISUAL TABLE
            ui->reservationTable->item(currentRow, 1)->setText(newName);
            ui->reservationTable->item(currentRow, 2)->setText(newRoom);
            ui->reservationTable->item(currentRow, 3)->setText(newDate);
            ui->reservationTable->item(currentRow, 4)->setText(newStart);
            ui->reservationTable->item(currentRow, 5)->setText(newEnd);
            ui->reservationTable->item(currentRow, 6)->setText(newEndDate);
            ui->reservationTable->item(currentRow, 7)->setText(newRecurrence);
            ui->reservationTable->item(currentRow, 8)->setText(newOverwriteUI);

            QMessageBox::information(this, "Success", "Reservation updated successfully!");
            applyFilters();
        } else {
            QMessageBox::critical(this, "Error", "Failed to update reservation in database.");
        }
    }
}

void HomeWindow::on_btnDeleteReservation_clicked()
{
    int currentRow = ui->reservationTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Select Reservation", "Please click on a reservation in the table to delete it.");
        return;
    }

    QString resId = ui->reservationTable->item(currentRow, 0)->text();
    QString resName = ui->reservationTable->item(currentRow, 1)->text();
    QString room = ui->reservationTable->item(currentRow, 2)->text();
    QString startDate = ui->reservationTable->item(currentRow, 3)->text();
    QString startTime = ui->reservationTable->item(currentRow, 4)->text();
    QString endTime = ui->reservationTable->item(currentRow, 5)->text();
    QString endDate = ui->reservationTable->item(currentRow, 6)->text();
    QString recurrence = ui->reservationTable->item(currentRow, 7)->text();
    QString overwriteText = ui->reservationTable->item(currentRow, 8)->text();

    QString summary = QString("Name: %1\nRoom: %2\nStart Date: %3\nTime: %4 to %5\nEnd Date: %6\nRecurrence: %7\nOverwriteable: %8")
                          .arg(resName, room, startDate, startTime, endTime, endDate, recurrence, overwriteText);

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Deletion",
                                                              "Are you sure you want to delete the following reservation? This cannot be undone.\n\n" + summary,
                                                              QMessageBox::Yes | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        // DELETE FROM DATABASE
        QSqlQuery query;
        query.prepare("DELETE FROM reservations WHERE id = :id");
        query.bindValue(":id", resId);

        if (query.exec()) {
            ui->reservationTable->removeRow(currentRow);
            QMessageBox::information(this, "Deleted", "Reservation deleted!");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete from database.");
        }
    }
}

// --- FILTERING LOGIC ---

void HomeWindow::applyFilters()
{
    QString targetRoom = ui->filterRoomComboBox->currentText();
    bool checkDate = ui->filterDateCheckBox->isChecked();
    QString targetDate = ui->filterDateEdit->date().toString("MM/dd/yyyy");

    for (int i = 0; i < ui->reservationTable->rowCount(); ++i) {
        bool showRow = true;

        QString rowRoom = ui->reservationTable->item(i, 2)->text();
        if (targetRoom != "All Rooms" && targetRoom != "" && rowRoom != targetRoom) {
            showRow = false;
        }

        if (checkDate && showRow) {
            QString rowDate = ui->reservationTable->item(i, 3)->text();
            if (rowDate != targetDate) {
                showRow = false;
            }
        }

        ui->reservationTable->setRowHidden(i, !showRow);
    }
}

void HomeWindow::on_filterRoomComboBox_currentTextChanged(const QString &arg1)
{
    applyFilters();
}

void HomeWindow::on_filterDateCheckBox_toggled(bool checked)
{
    ui->filterDateEdit->setEnabled(checked);
    applyFilters();
}

void HomeWindow::on_filterDateEdit_dateChanged(const QDate &date)
{
    applyFilters();
}
