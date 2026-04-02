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
#include <QShowEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVariantAnimation>
#include <QTimer>
#include <QLabel>
#include <QColor>
#include <QScreen>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QFontMetricsF>
#include <QPainter>
#include <QtMath>
#include <QPointer>
#include <QEvent>
#include <functional>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QPrinter>
#include <QPrintDialog>

#include "database.h"

// custom clickable item helpter class
class ReservationItem : public QGraphicsRectItem {
  public:
    Database::Reservation data;
    std::function<void(const Database::Reservation&)> onClicked;

    ReservationItem(const Database::Reservation& r) : data(r) {
        setAcceptHoverEvents(true);
        setCursor(Qt::PointingHandCursor);
    }

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        if (onClicked) onClicked(data);
        QGraphicsRectItem::mousePressEvent(event);
    }
};

// Helper function to convert bitmask to readable days
QString getDaysString(int byWeekday) {
    if (byWeekday == 0) return "Does not repeat";
    if (byWeekday == 127) return "Everyday";

    QStringList days;
    if (byWeekday & 1) days << "Mon";
    if (byWeekday & 2) days << "Tue";
    if (byWeekday & 4) days << "Wed";
    if (byWeekday & 8) days << "Thu";
    if (byWeekday & 16) days << "Fri";
    if (byWeekday & 32) days << "Sat";
    if (byWeekday & 64) days << "Sun";

    return days.join(", ");
}

//this function catches the clicks or taps from the user to skip animation
namespace {
class AnimSkipFilter : public QObject {
  public:
    std::function<void()> onSkip;
    AnimSkipFilter(QObject* parent = nullptr) : QObject(parent) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress) {
            if (onSkip) {
                onSkip();
                onSkip = nullptr; // Prevent double triggers
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};
}

// constructor

HomeWindow::HomeWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::HomeWindow)
{
    ui->setupUi(this);

    ui->startTimeEdit->setTimeRange(QTime(8, 0), QTime(20, 0));
    ui->endTimeEdit->setTimeRange(QTime(8, 0), QTime(20, 0));

           // new day checkbox logic start
           // 1. Disable the days group box by default when the app opens
    ui->weekdayGroupBox->setEnabled(false);

           // 2. Listen for changes in the Frequency Dropdown
    connect(ui->frequencyComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index){
        // Index 0: Does not repeat
        // Index 1: Everyday
        // Index 2: Every Week
        // Index 3: Every Month
        // Index 4: Every Year

        bool isWeekly = (index == 2); // True ONLY if it's "Every Week"

        // Enable or disable the whole box
        ui->weekdayGroupBox->setEnabled(isWeekly);

        // If disabled, uncheck all the boxes to keep the database data clean
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
    // --- NEW DAY CHECKBOX LOGIC END ---


           // Force the app to always open on the first tab (Welcome / Rooms)
    ui->tabWidget->setCurrentIndex(0);

           // Setup Database First
    if (!setupDatabase()) {
        QMessageBox::critical(this, "Database Error", "Failed to connect to the database. Your data will not be saved.");
    }

           // Set Default Dates
    ui->startDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDate(QDate::currentDate());
    ui->scheduleDateEdit->setDate(QDate::currentDate());
    ui->filterDateEdit->setDate(QDate::currentDate());

    ui->filterRoomComboBox->addItem("All Rooms");
    ui->filterDateEdit->setEnabled(false);

           // Setup Table
    ui->reservationTable->setColumnCount(10);
    ui->reservationTable->setHorizontalHeaderLabels({"ID", "Name", "Room", "Start Date", "Start Time", "End Time", "End Date", "Frequency", "Days", "Overwriteable"});

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

           // Load Existing Data from Database into the UI
    loadRoomsFromDatabase();
    loadReservationsFromDatabase();

           // --- SETUP GRAPHICS VIEW CANVAS ---
    scheduleScene = new QGraphicsScene(this);
    ui->scheduleView->setScene(scheduleScene);
    ui->scheduleView->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // 1. Redraw when the user picks a new date
    connect(ui->scheduleDateEdit, &QDateEdit::dateChanged, this, &HomeWindow::drawSchedule);

    // 2. Redraw when the user picks a different room from the dropdown
    connect(ui->scheduleRoomComboBox, &QComboBox::currentTextChanged, this, &HomeWindow::drawSchedule);

           // 3. Setup the Next/Prev Week buttons!
    connect(ui->btnPrevWeek, &QPushButton::clicked, this, [=]() {
        ui->scheduleDateEdit->setDate(ui->scheduleDateEdit->date().addDays(-7));
    });
    connect(ui->btnNextWeek, &QPushButton::clicked, this, [=]() {
        ui->scheduleDateEdit->setDate(ui->scheduleDateEdit->date().addDays(7));
    });

           // Draw it once right at startup
    drawSchedule();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [=](int index){
        if (index == 2) {
            drawSchedule();
        }
    });
    // --- END GRAPHICS SETUP ---


           // pre launch overlay
    QWidget *startupOverlay = new QWidget(this);
    startupOverlay->setObjectName("startupOverlay");
    startupOverlay->setGeometry(-5000, -5000, 10000, 10000);
    startupOverlay->setAttribute(Qt::WA_StyledBackground, true);
    QColor bgColor = this->palette().color(QPalette::Window);
    startupOverlay->setStyleSheet(QString("background-color: rgba(%1, %2, %3, 255);")
                                      .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()));
    startupOverlay->raise();
}

HomeWindow::~HomeWindow()
{
    db.close();
    delete ui;
}

//database set up

bool HomeWindow::setupDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("reservations.db"); // Creates a local file named reservations.db

    if (!db.open()) {
        return false;
    }

           // Creates tables
    if(!Database::Room::create())
        return false;

    if(!Database::Reservation::create())
        return false;

    return true;
}

void HomeWindow::loadRoomsFromDatabase()
{
    QStringList rooms = Database::Room::retrieve();
    ui->roomComboBox->addItems(rooms);
    ui->filterRoomComboBox->addItems(rooms);
    ui->scheduleRoomComboBox->addItems(rooms);
}

void HomeWindow::loadReservationsFromDatabase()
{
    ui->reservationTable->setColumnCount(10);
    ui->reservationTable->setHorizontalHeaderLabels({"ID", "Name", "Room", "Start Date", "Start Time", "End Time", "End Date", "Frequency", "Days", "Overwriteable"});

           // Fetch all reservations from the database
    QList<Database::Reservation> reservations = Database::Reservation::retrieve();

           // CRITICAL FIX: Force the visual table to perfectly match the database size.
           // This entirely prevents old rows from stacking up on the screen!
    ui->reservationTable->setRowCount(reservations.size());

    for (int row = 0; row < reservations.size(); ++row) {
        const Database::Reservation &r = reservations[row];

        ui->reservationTable->setItem(row, 0, new QTableWidgetItem(QString::number(r.id)));
        ui->reservationTable->setItem(row, 1, new QTableWidgetItem(r.name));
        ui->reservationTable->setItem(row, 2, new QTableWidgetItem(r.room));
        ui->reservationTable->setItem(row, 3, new QTableWidgetItem(r.startDate.toString("MM/dd/yyyy")));
        ui->reservationTable->setItem(row, 4, new QTableWidgetItem(r.startTime.toString("hh:mm AP")));
        ui->reservationTable->setItem(row, 5, new QTableWidgetItem(r.endTime.toString("hh:mm AP")));
        ui->reservationTable->setItem(row, 6, new QTableWidgetItem(r.endDate.toString("MM/dd/yyyy")));

        QString freqText = "Does not repeat";
        if (r.frequency == 1) freqText = "Everyday";
        else if (r.frequency == 2) freqText = "Every Week";
        else if (r.frequency == 3) freqText = "Every Month";
        else if (r.frequency == 4) freqText = "Every Year";
        ui->reservationTable->setItem(row, 7, new QTableWidgetItem(freqText));

        QString daysStr = getDaysString(r.byWeekday);
        ui->reservationTable->setItem(row, 8, new QTableWidgetItem(daysStr));
        ui->reservationTable->setItem(row, 9, new QTableWidgetItem(r.overwriteable ? "Yes" : "No"));
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
        QString error;

        if (Database::Room::insert(fullRoomName, error)) {
            QMessageBox::information(this, "Success", "Room created successfully.");
            ui->roomComboBox->addItem(fullRoomName);
            ui->filterRoomComboBox->addItem(fullRoomName);
            ui->scheduleRoomComboBox->addItem(fullRoomName);
            ui->roomInput->clear();
        } else {
            QMessageBox::warning(this, "Error", error);
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
        // QSqlQuery query;
        // query.prepare("DELETE FROM rooms WHERE name = :name");
        // query.bindValue(":name", fullRoomName);

        if(Database::Room::remove(fullRoomName))
        {
            QMessageBox::information(this, "Success", "Room deleted successfully.");

            int createIdx = ui->roomComboBox->findText(fullRoomName);
            if (createIdx != -1) ui->roomComboBox->removeItem(createIdx);

            int filterIdx = ui->filterRoomComboBox->findText(fullRoomName);
            if (filterIdx != -1) ui->filterRoomComboBox->removeItem(filterIdx);

            int schedIdx = ui->scheduleRoomComboBox->findText(fullRoomName);
            if (schedIdx != -1) ui->scheduleRoomComboBox->removeItem(schedIdx);

            ui->roomInput->clear();
        }
        else
        {
            QMessageBox::warning(this, "Error", "Failed to delete room.");
        }
    }
}

// --- RESERVATION MANAGEMENT ---

void HomeWindow::on_btnCreateReservation_clicked()
{
    // 1. Gather basic info directly from the widgets
    QString resName = ui->resNameInput->text();
    QString room = ui->roomComboBox->currentText();

    // Grab the dates and times directly as objects!
    QDate startDate = ui->startDateEdit->date();
    QDate endDate = ui->endDateEdit->date();

    // --> THESE ARE THE CRITICAL FIXES <--
    QTime startTime = ui->startTimeEdit->time();
    QTime endTime = ui->endTimeEdit->time();

    int frequencyIndex = ui->frequencyComboBox->currentIndex();
    QString frequencyStr = ui->frequencyComboBox->currentText();
    bool isOverwriteable = ui->overwriteCheckBox->isChecked();

    if (resName.isEmpty() || room.isEmpty()) {
        QMessageBox::warning(this, "Missing Info", "Please provide a room and reservation name.");
        return;
    }

           // 2. Build the weekday bitmask
    int weekdayMask = 0;
    if (ui->chkMon->isChecked()) weekdayMask |= 1;
    if (ui->chkTue->isChecked()) weekdayMask |= 2;
    if (ui->chkWed->isChecked()) weekdayMask |= 4;
    if (ui->chkThu->isChecked()) weekdayMask |= 8;
    if (ui->chkFri->isChecked()) weekdayMask |= 16;
    if (ui->chkSat->isChecked()) weekdayMask |= 32;
    if (ui->chkSun->isChecked()) weekdayMask |= 64;

           // 3. Save to Reservation Object
    Database::Reservation r;
    r.name = resName;
    r.room = room;
    r.startDate = startDate;
    r.startTime = startTime;
    r.endTime = endTime;
    r.endDate = endDate;
    r.frequency = frequencyIndex;
    r.byWeekday = weekdayMask;
    r.overwriteable = isOverwriteable;

           // 4. Show the Confirmation Pop-up
    QString daysStr = getDaysString(r.byWeekday);
    QString confirmMsg = QString("Are you sure you want to create the following reservation?\n\n"
                                 "Name: %1\n"
                                 "Room: %2\n"
                                 "Start: %3 at %4\n"
                                 "End: %5 at %6\n"
                                 "Frequency: %7\n"
                                 "Days: %8\n"
                                 "Overwriteable: %9")
                             .arg(r.name)
                             .arg(r.room)
                             .arg(r.startDate.toString("MM/dd/yyyy"))
                             .arg(r.startTime.toString("hh:mm AP"))
                             .arg(r.endDate.toString("MM/dd/yyyy"))
                             .arg(r.endTime.toString("hh:mm AP"))
                             .arg(frequencyStr)
                             .arg(daysStr)
                             .arg(r.overwriteable ? "Yes" : "No");

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Reservation", confirmMsg, QMessageBox::Yes | QMessageBox::No);

           // 5. Insert into Database if user clicks "Yes"
    if (reply == QMessageBox::Yes) {
        auto result = Database::Reservation::insert(r);

        if (result.has_value()) {
            QMessageBox::information(this, "Success", "Reservation successfully created!");

            // Clear the form fields back to defaults
            ui->resNameInput->clear();
            ui->startDateEdit->setDate(QDate::currentDate());
            ui->endDateEdit->setDate(QDate::currentDate());

            // Reset times back to a default (e.g., midnight)
            ui->startTimeEdit->setTime(QTime(0, 0));
            ui->endTimeEdit->setTime(QTime(0, 0));

            ui->frequencyComboBox->setCurrentIndex(0); // This will automatically uncheck the days!
            ui->overwriteCheckBox->setChecked(false);

                   // Refresh the table perfectly by reloading the database
            loadReservationsFromDatabase();

        } else {
            QMessageBox::critical(this, "Error", "Failed to save reservation to database.");
        }
    }
}

void HomeWindow::on_btnEditReservation_clicked()
{
    int selectedRow = ui->reservationTable->currentRow();
    if (selectedRow < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a reservation to edit.");
        return;
    }

           // 1. Grab the critical ID from the hidden column 0
    int resId = ui->reservationTable->item(selectedRow, 0)->text().toInt();

           // 2. Grab the rest of the data to pre-fill the Edit Popup
    QString name = ui->reservationTable->item(selectedRow, 1)->text();
    QString room = ui->reservationTable->item(selectedRow, 2)->text();
    QDate startDate = QDate::fromString(ui->reservationTable->item(selectedRow, 3)->text(), "MM/dd/yyyy");
    QTime startTime = QTime::fromString(ui->reservationTable->item(selectedRow, 4)->text(), "hh:mm AP");
    QTime endTime = QTime::fromString(ui->reservationTable->item(selectedRow, 5)->text(), "hh:mm AP");
    QDate endDate = QDate::fromString(ui->reservationTable->item(selectedRow, 6)->text(), "MM/dd/yyyy");
    QString freqText = ui->reservationTable->item(selectedRow, 7)->text();
    QString daysStr = ui->reservationTable->item(selectedRow, 8)->text();
    bool isOverwriteable = (ui->reservationTable->item(selectedRow, 9)->text() == "Yes");

           // Reverse-engineer the days string back into the integer bitmask for the checkboxes
    int byWeekday = 0;
    if (daysStr == "Everyday") byWeekday = 127;
    else {
        if (daysStr.contains("Mon")) byWeekday |= 1;
        if (daysStr.contains("Tue")) byWeekday |= 2;
        if (daysStr.contains("Wed")) byWeekday |= 4;
        if (daysStr.contains("Thu")) byWeekday |= 8;
        if (daysStr.contains("Fri")) byWeekday |= 16;
        if (daysStr.contains("Sat")) byWeekday |= 32;
        if (daysStr.contains("Sun")) byWeekday |= 64;
    }

           // 3. Open the Dialog
    EditDialog editPopup(this);
    editPopup.setReservationData(name, room, startDate, startTime, endTime, freqText, endDate, isOverwriteable, byWeekday);

           // 4. If the user clicks "OK" on the Edit window
    if (editPopup.exec() == QDialog::Accepted) {

        Database::Reservation r;
        r.id = resId; // <--- This is what connects the update to the correct database row!
        r.name = editPopup.getUpdatedName();
        r.room = editPopup.getUpdatedRoom();
        r.startDate = editPopup.getUpdatedDate();
        r.startTime = editPopup.getUpdatedStartTime();
        r.endTime = editPopup.getUpdatedEndTime();
        r.endDate = editPopup.getUpdatedEndDate();
        r.byWeekday = editPopup.getUpdatedByWeekday();
        r.overwriteable = editPopup.getUpdatedOverwriteable();

               // Convert the string frequency from the dropdown back to an integer for SQLite
        QString updatedFreqText = editPopup.getUpdatedFrequency();
        if (updatedFreqText == "Does not repeat") r.frequency = 0;
        else if (updatedFreqText == "Everyday") r.frequency = 1;
        else if (updatedFreqText == "Every Week") r.frequency = 2;
        else if (updatedFreqText == "Every Month") r.frequency = 3;
        else if (updatedFreqText == "Every Year") r.frequency = 4;
        else r.frequency = 0;

               // 5. Show Final Confirmation Pop-up
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Edit",
                                                                  "Are you sure you want to save these changes?",
                                                                  QMessageBox::Yes | QMessageBox::No);

               // 6. Save to Database
        if (reply == QMessageBox::Yes) {
            if (Database::Reservation::update(r)) {
                QMessageBox::information(this, "Success", "Reservation successfully updated.");
                loadReservationsFromDatabase(); // Refresh the table to show the changes
            } else {
                QMessageBox::critical(this, "Error", "Failed to update reservation in the database.");
            }
        }
    }
}

void HomeWindow::on_btnDeleteReservation_clicked()
{
    // 1. Check if the user actually clicked on a row
    int selectedRow = ui->reservationTable->currentRow();
    if (selectedRow < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a reservation to delete.");
        return;
    }

           // 2. Safely grab ALL the data from the correct columns for the selected row
    QString resId = ui->reservationTable->item(selectedRow, 0)->text();
    QString resName = ui->reservationTable->item(selectedRow, 1)->text();
    QString room = ui->reservationTable->item(selectedRow, 2)->text();
    QString startDate = ui->reservationTable->item(selectedRow, 3)->text();
    QString startTime = ui->reservationTable->item(selectedRow, 4)->text();
    QString endTime = ui->reservationTable->item(selectedRow, 5)->text();
    QString endDate = ui->reservationTable->item(selectedRow, 6)->text();
    QString frequency = ui->reservationTable->item(selectedRow, 7)->text();
    QString daysStr = ui->reservationTable->item(selectedRow, 8)->text();
    QString overwriteable = ui->reservationTable->item(selectedRow, 9)->text();

           // 3. Build the detailed confirmation message (matches the Create pop-up)
    QString confirmMsg = QString("Are you sure you want to permanently delete this reservation?\n\n"
                                 "Name: %1\n"
                                 "Room: %2\n"
                                 "Start: %3 at %4\n"
                                 "End: %5 at %6\n"
                                 "Frequency: %7\n"
                                 "Days: %8\n"
                                 "Overwriteable: %9")
                             .arg(resName)
                             .arg(room)
                             .arg(startDate)
                             .arg(startTime)
                             .arg(endDate)
                             .arg(endTime)
                             .arg(frequency)
                             .arg(daysStr)
                             .arg(overwriteable);

           // Show the pop-up
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Reservation", confirmMsg,
                                                              QMessageBox::Yes | QMessageBox::No);

           // 4. Delete from the database if confirmed
    if (reply == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM reservations WHERE id = :id");
        query.bindValue(":id", resId);

        if (query.exec()) {
            loadReservationsFromDatabase(); // Refresh the table so the deleted row disappears
            QMessageBox::information(this, "Deleted", "Reservation successfully deleted.");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete reservation from the database.");
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

void HomeWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    static bool firstLaunch = true;
    if (!firstLaunch) return;
    firstLaunch = false;

           // --- ANIMATION STATE TRACKER ---
           // This parent object holds all active animations. If we delete it, everything stops instantly.
    QObject* animState = new QObject(this);
    QPointer<QObject> pAnimState(animState);

    QColor bgColor = this->palette().color(QPalette::Window);

    QWidget *overlay = this->findChild<QWidget*>("startupOverlay");
    if (!overlay) {
        overlay = new QWidget(this);
        overlay->setGeometry(-5000, -5000, 10000, 10000);
        overlay->setAttribute(Qt::WA_StyledBackground, true);
        overlay->setStyleSheet(QString("background-color: rgba(%1, %2, %3, 255);")
                                   .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()));
    }
    overlay->raise();
    overlay->show();
    QPointer<QWidget> pOverlay(overlay);

    QLabel *targetLabel = ui->welcomeLabel;

    QFont appFont = targetLabel->font();
    appFont.setStyleStrategy(QFont::PreferAntialias);
    appFont.setHintingPreference(QFont::PreferNoHinting);

    QFont compFont = appFont;
    compFont.setPointSizeF(20.0);

    QFont bigFont = appFont;
    bigFont.setPointSizeF(36.0);

    QLabel *animLabel = new QLabel(this);
    animLabel->raise();
    animLabel->show();
    QPointer<QLabel> pAnimLabel(animLabel);

           // --- SKIP LOGIC HANDLER ---
    AnimSkipFilter* skipFilter = new AnimSkipFilter(this);
    QPointer<AnimSkipFilter> pSkipFilter(skipFilter);

    skipFilter->onSkip = [=]() {
        // INSTANT KILL: Destroying animState violently stops all timers and running animations perfectly safely
        if (pAnimState) pAnimState->deleteLater();
        if (pOverlay) pOverlay->deleteLater();
        if (pAnimLabel) pAnimLabel->deleteLater();
        if (pSkipFilter) pSkipFilter->deleteLater(); // Remove the keyboard listener
        targetLabel->show();
    };
    qApp->installEventFilter(skipFilter); // Start listening globally

    QString targetStr = "Welcome to the CAS Room Manager";
    QFontMetricsF fmComp(compFont);
    qreal textW = fmComp.horizontalAdvance(targetStr);

           // --- PHASE 1: START IMMEDIATELY ---
    QString gibberish = "ABCDEFGHJKLNOPQRSTUVXYZ023456789#$";
    QTimer *decodeTimer = new QTimer(animState); // Attached to animState!
    decodeTimer->setProperty("revealIdx", 0);
    decodeTimer->setProperty("tick", 0);

    connect(decodeTimer, &QTimer::timeout, animState, [=]() {
        int rIdx = decodeTimer->property("revealIdx").toInt();
        int tick = decodeTimer->property("tick").toInt();

        qreal dpr = this->devicePixelRatioF();

        int w = this->width();
        int h = this->height();
        if (pAnimLabel) pAnimLabel->setGeometry(0, 0, w, h);

        QPixmap frame(qCeil(w * dpr), qCeil(h * dpr));
        frame.setDevicePixelRatio(dpr);
        frame.fill(Qt::transparent);

        QPainter p(&frame);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

        auto drawComputer = [textW](QPainter &painter) {
            qreal monitorW = textW + 160;
            qreal monitorH = monitorW * (9.0 / 16.0);
            qreal bzl = 14;

            qreal neckW = monitorW * 0.08;
            qreal neckH = monitorH * 0.15;
            qreal baseW = monitorW * 0.35;
            qreal baseH = monitorH * 0.04;
            qreal baseBottomY = monitorH/2 + neckH + baseH;

            qreal kbW = monitorW * 0.85;
            qreal kbH = monitorW * 0.14;
            qreal kbX = -(kbW/2) - (monitorW * 0.08);
            qreal kbY = baseBottomY + (monitorH * 0.10);

            qreal mouseW = monitorW * 0.07;
            qreal mouseH = monitorW * 0.11;
            qreal mouseX = kbX + kbW + (monitorW * 0.04);
            qreal mouseY = kbY + (kbH - mouseH)/2;

            qreal cpuW = monitorW * 0.32;
            qreal cpuH = monitorH * 1.3;
            qreal cpuX = monitorW/2 + (monitorW * 0.08);
            qreal cpuY = kbY + kbH - cpuH;

            painter.setBrush(QColor("#2C2C2E"));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(cpuX, cpuY, cpuW, cpuH, 12, 12);
            painter.setBrush(QColor("#34C759"));
            painter.drawEllipse(cpuX + cpuW/2 - 6, cpuY + (cpuH * 0.1), 12, 12);
            painter.setBrush(QColor("#1C1C1E"));
            painter.drawRect(cpuX + cpuW/2 - (cpuW*0.3), cpuY + (cpuH * 0.2), cpuW*0.6, 6);
            painter.drawRect(cpuX + cpuW/2 - (cpuW*0.3), cpuY + (cpuH * 0.25), cpuW*0.6, 6);

            painter.setBrush(QColor("#2C2C2E"));
            painter.drawRoundedRect(-monitorW/2, -monitorH/2, monitorW, monitorH, 12, 12);
            painter.setBrush(QColor("#000000"));
            painter.drawRect(-(monitorW/2) + bzl, -(monitorH/2) + bzl, monitorW - (bzl*2), monitorH - (bzl*2.5));
            painter.setBrush(QColor("#8E8E93"));
            painter.drawRect(-neckW/2, monitorH/2, neckW, neckH);
            painter.setBrush(QColor("#AEAEB2"));
            painter.drawRoundedRect(-baseW/2, monitorH/2 + neckH, baseW, baseH, 5, 5);

            painter.setBrush(QColor("#AEAEB2"));
            painter.drawRoundedRect(kbX, kbY, kbW, kbH, 8, 8);
            painter.setBrush(QColor("#8E8E93"));
            qreal pad = kbH * 0.12;
            painter.drawRoundedRect(kbX + pad, kbY + pad, kbW - (pad*2), kbH - (pad*2), 4, 4);

            painter.setBrush(QColor("#AEAEB2"));
            painter.drawRoundedRect(mouseX, mouseY, mouseW, mouseH, mouseW/2, mouseW/2);
            painter.setBrush(QColor("#2C2C2E"));
            painter.drawRoundedRect(mouseX + mouseW/2 - 2, mouseY + (mouseH*0.15), 4, mouseH*0.25, 2, 2);
        };

        p.save();
        p.translate(w/2.0, h/2.0);
        drawComputer(p);
        p.restore();

        p.setFont(compFont);
        qreal startX = (w - textW) / 2.0;

        for (int i = 0; i < targetStr.length(); ++i) {
            if (targetStr[i] == ' ') continue;

            qreal charX = startX + fmComp.horizontalAdvance(targetStr.left(i));
            qreal charW = fmComp.horizontalAdvance(targetStr.mid(i, 1));
            QRectF charRect(charX, 0, charW, h);

            if (i < rIdx) {
                p.setPen(QColor("#34C759"));
                p.drawText(charRect, Qt::AlignVCenter | Qt::AlignLeft, QString(targetStr[i]));
            } else {
                p.setPen(QColor("#C21807"));
                QString gib = QString(gibberish[QRandomGenerator::global()->bounded(gibberish.length())]);
                p.drawText(charRect, Qt::AlignVCenter | Qt::AlignHCenter, gib);
            }
        }
        if (pAnimLabel) pAnimLabel->setPixmap(frame);

        tick++; if (tick % 3 == 0) rIdx++;
        decodeTimer->setProperty("revealIdx", rIdx); decodeTimer->setProperty("tick", tick);

               // --- TRANSITION TO PHASE 2 & 3 ---
        if (rIdx > targetStr.length()) {
            decodeTimer->stop(); decodeTimer->deleteLater();

            int finalW = this->width();
            int finalH = this->height();

            QPixmap finalAppPix(qCeil(targetLabel->width() * dpr), qCeil(targetLabel->height() * dpr));
            finalAppPix.setDevicePixelRatio(dpr);
            finalAppPix.fill(Qt::transparent);
            targetLabel->render(&finalAppPix, QPoint(), QRegion(), QWidget::DrawChildren);

            QPoint targetPos = targetLabel->mapTo(this, QPoint(0,0));
            QRectF targetBox(targetPos.x(), targetPos.y(), targetLabel->width(), targetLabel->height());

            Qt::Alignment align = targetLabel->alignment();
            if (align == 0) align = Qt::AlignLeft | Qt::AlignVCenter;

            targetLabel->hide();

            QFontMetricsF fmBig(bigFont);
            qreal textBigW = fmBig.horizontalAdvance(targetStr);
            qreal textBigH = fmBig.height();
            QPointF centerScreen(finalW / 2.0, finalH / 2.0);

                   // --- PHASE 2: CAMERA ZOOM ANIMATION ---
            QVariantAnimation *zoomAnim = new QVariantAnimation(animState); // Attached!
            zoomAnim->setDuration(900);
            zoomAnim->setEasingCurve(QEasingCurve::InExpo);
            zoomAnim->setStartValue(0.0);
            zoomAnim->setEndValue(1.0);

            connect(zoomAnim, &QVariantAnimation::valueChanged, animState, [=](const QVariant &value){
                double z = value.toDouble();

                QPixmap zFrame(qCeil(finalW * dpr), qCeil(finalH * dpr));
                zFrame.setDevicePixelRatio(dpr);
                zFrame.fill(Qt::transparent);
                QPainter pZ(&zFrame);
                pZ.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

                double compScale = 1.0 + (z * 10.0);
                double compOpacity = 1.0 - std::pow(z, 2.0);

                if (compOpacity > 0.01) {
                    pZ.save();
                    pZ.translate(centerScreen.x(), centerScreen.y());
                    pZ.scale(compScale, compScale);
                    pZ.setOpacity(compOpacity);
                    drawComputer(pZ);
                    pZ.restore();
                }

                qreal textScale = (20.0 / 36.0) + ((1.0 - (20.0 / 36.0)) * z);

                pZ.save();
                pZ.translate(centerScreen.x(), centerScreen.y());
                pZ.scale(textScale, textScale);
                pZ.setFont(bigFont);
                pZ.setPen(QColor("#34C759"));

                QRectF drawRect(-textBigW / 2.0, -textBigH / 2.0, textBigW, textBigH);
                pZ.drawText(drawRect, Qt::AlignCenter, targetStr);
                pZ.restore();

                if (pAnimLabel) pAnimLabel->setPixmap(zFrame);
            });

                   // --- PHASE 3: MORPH & FLY ANIMATION ---
            QParallelAnimationGroup *morphGroup = new QParallelAnimationGroup(animState); // Attached!
            QVariantAnimation *fly = new QVariantAnimation(morphGroup);
            fly->setDuration(1200);
            fly->setEasingCurve(QEasingCurve::InOutQuart);
            fly->setStartValue(0.0); fly->setEndValue(1.0);

            QColor startColor("#34C759");
            QColor targetColor = targetLabel->palette().color(QPalette::WindowText);

            QFontMetricsF fmTarget(appFont);
            qreal textTargetW = fmTarget.horizontalAdvance(targetStr);
            qreal targetScale = textTargetW / textBigW;

            QPointF targetCenter;
            if (align & Qt::AlignHCenter) targetCenter = QPointF(targetBox.x() + targetBox.width() / 2.0, targetBox.y() + targetBox.height() / 2.0);
            else if (align & Qt::AlignRight) targetCenter = QPointF(targetBox.x() + targetBox.width() - (textTargetW / 2.0), targetBox.y() + targetBox.height() / 2.0);
            else targetCenter = QPointF(targetBox.x() + (textTargetW / 2.0), targetBox.y() + targetBox.height() / 2.0);

            connect(fly, &QVariantAnimation::valueChanged, animState, [=](const QVariant &value){
                double prog = value.toDouble();

                QPixmap frameB(qCeil(finalW * dpr), qCeil(finalH * dpr));
                frameB.setDevicePixelRatio(dpr);
                frameB.fill(Qt::transparent);

                QPainter pB(&frameB);
                pB.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

                pB.setOpacity(prog);
                pB.drawPixmap(targetBox.x(), targetBox.y(), finalAppPix);

                int r = startColor.red() + (targetColor.red() - startColor.red()) * prog;
                int g = startColor.green() + (targetColor.green() - startColor.green()) * prog;
                int b = startColor.blue() + (targetColor.blue() - startColor.blue()) * prog;
                QColor curColor(r, g, b);

                QPointF curCenter(
                    centerScreen.x() + (targetCenter.x() - centerScreen.x()) * prog,
                    centerScreen.y() + (targetCenter.y() - centerScreen.y()) * prog
                    );

                qreal curScale = 1.0 + (targetScale - 1.0) * prog;

                pB.save();
                pB.translate(curCenter.x(), curCenter.y());
                pB.scale(curScale, curScale);

                pB.setOpacity(1.0 - prog);
                pB.setFont(bigFont);
                pB.setPen(curColor);

                QRectF drawRect(-textBigW / 2.0, -textBigH / 2.0, textBigW, textBigH);
                pB.drawText(drawRect, Qt::AlignCenter, targetStr);
                pB.restore();

                if (pAnimLabel) pAnimLabel->setPixmap(frameB);
            });

            QVariantAnimation *fade = new QVariantAnimation(morphGroup);
            fade->setDuration(1000); fade->setStartValue(255); fade->setEndValue(0);
            connect(fade, &QVariantAnimation::valueChanged, animState, [=](const QVariant &v){
                if (pOverlay) pOverlay->setStyleSheet(QString("background-color: rgba(%1, %2, %3, %4);")
                                                .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()).arg(v.toInt()));
            });

            morphGroup->addAnimation(fly); morphGroup->addAnimation(fade);

                   // --- CHAIN THE ANIMATIONS ---
            connect(zoomAnim, &QVariantAnimation::finished, animState, [=]() {
                morphGroup->start(QAbstractAnimation::DeleteWhenStopped);
            });

                   // Natural Finish (If the user DOES NOT skip)
            connect(morphGroup, &QParallelAnimationGroup::finished, animState, [=](){
                if (pOverlay) pOverlay->deleteLater();
                if (pAnimLabel) pAnimLabel->deleteLater();
                targetLabel->show();
                if (pSkipFilter) {
                    pSkipFilter->onSkip = nullptr;
                    pSkipFilter->deleteLater();
                }
                if (pAnimState) pAnimState->deleteLater(); // Clean up tracker
            });

                   // Start the Zoom sequence! (Safely tied to animState so it won't fire if skipped early)
            QTimer::singleShot(150, animState, [=]() {
                zoomAnim->start(QAbstractAnimation::DeleteWhenStopped);
            });
        }
    });

    decodeTimer->start(30);
}


// helper function to Check if a reservation happens on a specific date
bool HomeWindow::isReservationOnDate(const Database::Reservation &r, const QDate &targetDate) {
    // 1. Is it completely outside the overall start/end date range?
    if (targetDate < r.startDate || targetDate > r.endDate) {
        return false;
    }

           // 2. Determine if it lands on this specific target date based on the Frequency
    if (r.frequency == 0) {

        return targetDate == r.startDate;

    } else if (r.frequency == 1) {
        return true;

    } else if (r.frequency == 2) {
        int dayOfWeek = targetDate.dayOfWeek(); // Qt returns 1=Mon, 2=Tue... 7=Sun
        int maskMap[] = {0, 1, 2, 4, 8, 16, 32, 64};

        return (r.byWeekday & maskMap[dayOfWeek]) != 0;

    } else if (r.frequency == 3) {
        // Must land on the exact same day of the month
        return targetDate.day() == r.startDate.day();

    } else if (r.frequency == 4) {
        //Must land on the exact same month and day
        return targetDate.month() == r.startDate.month() &&
               targetDate.day() == r.startDate.day();
    }

    return false;
}


// --- THE MAIN GRAPHICS ENGINE ---
void HomeWindow::drawSchedule() {
    scheduleScene->clear();

    QDate selectedDate = ui->scheduleDateEdit->date();
    QString currentRoom = ui->scheduleRoomComboBox->currentText();
    QList<Database::Reservation> allRes = Database::Reservation::retrieve();

    int daysToSubtract = selectedDate.dayOfWeek() - 1;
    QDate startOfWeek = selectedDate.addDays(-daysToSubtract);

           // -- GRID SETTINGS --
    int startHour = 8;
    int endHour = 20;
    int pixelsPerMinute = 2;
    int hourHeight = 60 * pixelsPerMinute;
    int timeColWidth = 80;

    int availableWidth = ui->scheduleView->viewport()->width() - 5;
    int colWidth = (availableWidth > timeColWidth) ? ((availableWidth - timeColWidth) / 7) : 140;

    int yOffset = 50;

    QStringList daysOfWeek = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

           // 1. Draw Headers
    for (int i = 0; i < 7; ++i) {
        QDate dayDate = startOfWeek.addDays(i);
        QString headerText = QString("%1\n%2").arg(daysOfWeek[i]).arg(dayDate.toString("MM/dd"));

        QGraphicsTextItem *dayLabel = scheduleScene->addText(headerText);
        QFont font = dayLabel->font();
        font.setBold(true);
        font.setPointSize(10);
        dayLabel->setFont(font);

        int xPos = timeColWidth + (i * colWidth) + (colWidth / 2) - (dayLabel->boundingRect().width() / 2);
        dayLabel->setPos(xPos, 0);
    }

           // 2. Draw the Grid Lines and Times
    for (int h = startHour; h <= endHour; ++h) {
        int y = yOffset + ((h - startHour) * hourHeight);
        QString timeText = (h == 24) ? "12:00 AM" : QTime(h, 0).toString("h:mm AP");
        QGraphicsTextItem *timeLabel = scheduleScene->addText(timeText);
        timeLabel->setPos(5, y - 10);

        scheduleScene->addLine(timeColWidth, y, timeColWidth + (7 * colWidth), y, QPen(Qt::lightGray));

        if (h < endHour) {
            scheduleScene->addLine(timeColWidth, y + (hourHeight / 2), timeColWidth + (7 * colWidth), y + (hourHeight / 2), QPen(Qt::DotLine));
        }
    }

           // 3. Draw Vertical Day Divider Lines
    for (int i = 0; i <= 7; ++i) {
        int x = timeColWidth + (i * colWidth);
        scheduleScene->addLine(x, yOffset, x, yOffset + ((endHour - startHour) * hourHeight), QPen(Qt::black));
    }

           // 4. DRAW THE RESERVATIONS
    if (currentRoom.isEmpty()) return;

    for (const auto& r : allRes) {
        if (r.room != currentRoom) continue;

        for (int dayIndex = 0; dayIndex < 7; ++dayIndex) {
            QDate currentDay = startOfWeek.addDays(dayIndex);

            if (isReservationOnDate(r, currentDay)) {
                int resStartH = r.startTime.hour();
                int resStartM = r.startTime.minute();
                int resEndH = r.endTime.hour();
                int resEndM = r.endTime.minute();

                if (resStartH < startHour) resStartH = startHour;
                if (resEndH > endHour) resEndH = endHour;

                int startY = yOffset + ((resStartH - startHour) * hourHeight) + (resStartM * pixelsPerMinute);
                int endY = yOffset + ((resEndH - startHour) * hourHeight) + (resEndM * pixelsPerMinute);
                int blockHeight = endY - startY;
                int blockX = timeColWidth + (dayIndex * colWidth);

                       // --- STEP 2 CODE STARTS HERE ---

                // 1. Create the custom clickable block (REPLACES addRect)
                ReservationItem *rectItem = new ReservationItem(r);
                rectItem->setRect(blockX + 2, startY, colWidth - 4, blockHeight);
                rectItem->setPen(QPen(Qt::black));
                rectItem->setBrush(QBrush(QColor(100, 150, 255, 200)));
                scheduleScene->addItem(rectItem);

                       // 2. Add the text as a child of the rectItem (REPLACES addText)
                QString blockText = QString("%1\n%2 - %3")
                                        .arg(r.name)
                                        .arg(r.startTime.toString("h:mm AP"))
                                        .arg(r.endTime.toString("h:mm AP"));
                QGraphicsTextItem *textItem = new QGraphicsTextItem(blockText, rectItem);
                textItem->setPos(blockX + 4, startY + 2);

                QFont font = textItem->font();
                font.setPointSize(8);
                textItem->setFont(font);

                       // 3. Set the click behavior
                rectItem->onClicked = [this](const Database::Reservation& res) {
                    handleScheduleClick(res);
                };

                // --- STEP 2 CODE ENDS HERE ---
            }
        }
    }
}

void HomeWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Only redraw the schedule if the user is actively looking at it
    if (ui->tabWidget->currentIndex() == 2) {
        drawSchedule();
    }
}

void HomeWindow::handleScheduleClick(const Database::Reservation &r)
{
    // 1. Format strings for the info box
    QString freqText = (r.frequency == 0) ? "Does not repeat" :
                           (r.frequency == 1) ? "Everyday" :
                           (r.frequency == 2) ? "Every Week" :
                           (r.frequency == 3) ? "Every Month" : "Every Year";

    QString info = QString("<b>Name:</b> %1<br>"
                           "<b>Room:</b> %2<br>"
                           "<b>Time:</b> %3 - %4<br>"
                           "<b>Date:</b> %5 to %6<br>"
                           "<b>Repeats:</b> %7<br>"
                           "<b>Days:</b> %8")
                       .arg(r.name, r.room)
                       .arg(r.startTime.toString("h:mm AP"), r.endTime.toString("h:mm AP"))
                       .arg(r.startDate.toString("MM/dd/yyyy"), r.endDate.toString("MM/dd/yyyy"))
                       .arg(freqText, getDaysString(r.byWeekday));

           // 2. Show Info Box with Edit/Delete options
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Reservation Details");
    msgBox.setText(info);
    QPushButton *editBtn = msgBox.addButton("Edit", QMessageBox::ActionRole);
    QPushButton *deleteBtn = msgBox.addButton("Delete", QMessageBox::DestructiveRole);
    msgBox.addButton(QMessageBox::Close);

    msgBox.exec();

    if (msgBox.clickedButton() == editBtn) {
        // reuse your existing EditDialog logic
        EditDialog editPopup(this);
        editPopup.setReservationData(r.name, r.room, r.startDate, r.startTime, r.endTime,
                                     freqText, r.endDate, r.overwriteable, r.byWeekday);

        if (editPopup.exec() == QDialog::Accepted) {
            Database::Reservation updated = r; // Keep the ID!
            updated.name = editPopup.getUpdatedName();
            updated.room = editPopup.getUpdatedRoom();
            updated.startDate = editPopup.getUpdatedDate();
            updated.startTime = editPopup.getUpdatedStartTime();
            updated.endTime = editPopup.getUpdatedEndTime();
            updated.endDate = editPopup.getUpdatedEndDate();
            updated.byWeekday = editPopup.getUpdatedByWeekday();
            updated.overwriteable = editPopup.getUpdatedOverwriteable();

            // Re-map frequency index
            QString fStr = editPopup.getUpdatedFrequency();
            updated.frequency = (fStr == "Does not repeat") ? 0 : (fStr == "Everyday") ? 1 :
                                                                  (fStr == "Every Week") ? 2 : (fStr == "Every Month") ? 3 : 4;

            if (Database::Reservation::update(updated)) {
                loadReservationsFromDatabase();

                // CRITICAL FIX: Delay the redraw so Qt finishes the click event first!
                QTimer::singleShot(0, this, [this]() {
                    drawSchedule();
                });
            }
        }
    }
    else if (msgBox.clickedButton() == deleteBtn) {
        if (QMessageBox::question(this, "Confirm", "Delete this reservation?") == QMessageBox::Yes) {
            QSqlQuery query;
            query.prepare("DELETE FROM reservations WHERE id = :id");
            query.bindValue(":id", r.id);
            if (query.exec()) {
                loadReservationsFromDatabase();

                // CRITICAL FIX: Delay the redraw so Qt finishes the click event first!
                QTimer::singleShot(0, this, [this]() {
                    drawSchedule();
                });
            }
        }
    }
}

void HomeWindow::on_btnPrintSchedule_clicked()
{
    if (!scheduleScene || scheduleScene->items().isEmpty()) {
        QMessageBox::information(this, "Empty Schedule", "There is nothing on the schedule to print.");
        return;
    }


    QPrinter printer(QPrinter::HighResolution);

           // FIX 1: Take FULL control of the page to ignore weird hardware driver offsets
    printer.setFullPage(true);


           // FIX 2: Force Landscape using the strict PageLayout engine
    QPageLayout layout = printer.pageLayout();
    layout.setOrientation(QPageLayout::Landscape);
    printer.setPageLayout(layout);


    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle("Print Schedule");

    if (dialog.exec() == QDialog::Accepted) {

        // FIX 3: Re-enforce Landscape in case the user's system print dialog reset it!
        if (printer.pageLayout().orientation() != QPageLayout::Landscape) {
            layout = printer.pageLayout();
            layout.setOrientation(QPageLayout::Landscape);
            printer.setPageLayout(layout);
        }


        QPainter painter(&printer);

               // FIX 4: Calculate a perfect 0.5-inch margin using the printer's literal DPI
               // Because setFullPage(true) is on, this guarantees a border on all 4 sides!
        int margin = printer.resolution() / 2;
        int padding = margin / 4;

        QRect pageRect = printer.pageRect(QPrinter::DevicePixel).toRect();
        QRect drawRect = pageRect.adjusted(margin, margin, -margin, -margin);


               // --- 1. BUILD THE HEADER ---
        QString roomName = ui->scheduleRoomComboBox->currentText();
        QDate selectedDate = ui->scheduleDateEdit->date();
        QDate startOfWeek = selectedDate.addDays(-(selectedDate.dayOfWeek() - 1));
        QDate endOfWeek = startOfWeek.addDays(6);

        QString weekString = QString("%1 - %2").arg(startOfWeek.toString("MMM d")).arg(endOfWeek.toString("MMM d, yyyy"));
        QString headerText = QString("Weekly Schedule: %1   (%2)").arg(roomName, weekString);


        QFont headerFont = painter.font();
        headerFont.setPixelSize(pageRect.height() * 0.025);
        headerFont.setBold(true);
        painter.setFont(headerFont);


        QFontMetrics fm(headerFont);
        int headerHeight = fm.height() + padding;


        painter.drawText(drawRect.left(), drawRect.top() + fm.ascent(), headerText);
        painter.setPen(QPen(Qt::black, 4));
        painter.drawLine(drawRect.left(), drawRect.top() + headerHeight, drawRect.right(), drawRect.top() + headerHeight);


               // --- 2. FIX COLORS FOR PRINTING ---
        struct ItemState {
            QGraphicsItem* item;
            QBrush brush;
            QPen pen;
            QColor textColor;
        };
        QList<ItemState> originalStates;


        for (QGraphicsItem* item : scheduleScene->items()) {
            ItemState state;
            state.item = item;


            if (ReservationItem* resItem = dynamic_cast<ReservationItem*>(item)) {
                state.brush = resItem->brush();
                state.pen = resItem->pen();
                originalStates.append(state);


                resItem->setBrush(QBrush(Qt::white));
                resItem->setPen(QPen(Qt::black, 2));
            }
            else if (QGraphicsTextItem* textItem = dynamic_cast<QGraphicsTextItem*>(item)) {
                state.textColor = textItem->defaultTextColor();
                originalStates.append(state);
                textItem->setDefaultTextColor(Qt::black);
            }
            else if (QGraphicsLineItem* lineItem = dynamic_cast<QGraphicsLineItem*>(item)) {
                state.pen = lineItem->pen();
                originalStates.append(state);
                QPen blackPen = lineItem->pen();
                blackPen.setColor(Qt::black);
                lineItem->setPen(blackPen);
            }
        }


               // --- 3. HIGH-RES RASTERIZATION ---
        QRectF sourceSceneRect = scheduleScene->itemsBoundingRect();

        sourceSceneRect.adjust(-20, -50, 20, 20);

        double rasterScale = 4.0;
        QSize imageSize(sourceSceneRect.width() * rasterScale, sourceSceneRect.height() * rasterScale);
        QImage highResImage(imageSize, QImage::Format_ARGB32);
        highResImage.fill(Qt::white);


        QPainter imagePainter(&highResImage);
        imagePainter.setRenderHint(QPainter::Antialiasing);
        imagePainter.setRenderHint(QPainter::TextAntialiasing);
        scheduleScene->render(&imagePainter, QRectF(0, 0, imageSize.width(), imageSize.height()), sourceSceneRect);
        imagePainter.end();


               // --- 4. THE SMART PAGINATION ENGINE ---
        double sceneHeaderBottom = 50.0;
        double headerRatio = (sceneHeaderBottom - sourceSceneRect.top()) / sourceSceneRect.height();
        int imgHeaderHeight = highResImage.height() * headerRatio;


        QImage imgHeader = highResImage.copy(0, 0, highResImage.width(), imgHeaderHeight);
        QImage imgBody = highResImage.copy(0, imgHeaderHeight, highResImage.width(), highResImage.height() - imgHeaderHeight);


        double printScale = (double)drawRect.width() / highResImage.width();
        int printHeaderHeight = imgHeaderHeight * printScale;


        int currentImgY = 0;
        int pageNum = 1;


        int scaledHalfHour = 60 * rasterScale;


        while (currentImgY < imgBody.height()) {
            if (pageNum > 1) {
                printer.newPage();
            }


            int availablePaperHeight = (pageNum == 1) ? (drawRect.height() - headerHeight - padding) : drawRect.height();
            int paperTopMargin = (pageNum == 1) ? (drawRect.top() + headerHeight + padding) : drawRect.top();


            QRect targetHeaderRect(drawRect.left(), paperTopMargin, drawRect.width(), printHeaderHeight);
            painter.drawImage(targetHeaderRect, imgHeader);


            int remainingPaperHeight = availablePaperHeight - printHeaderHeight;
            int chunkImgHeight = remainingPaperHeight / printScale;


            if (currentImgY + chunkImgHeight < imgBody.height()) {
                chunkImgHeight = (chunkImgHeight / scaledHalfHour) * scaledHalfHour;
                remainingPaperHeight = chunkImgHeight * printScale;
            } else {
                chunkImgHeight = imgBody.height() - currentImgY;
                remainingPaperHeight = chunkImgHeight * printScale;
            }


            QImage chunk = imgBody.copy(0, currentImgY, imgBody.width(), chunkImgHeight);

                   // Draw perfectly inside our protected drawRect borders!
            QRect targetChunkRect(drawRect.left(), targetHeaderRect.bottom() + 1, drawRect.width(), remainingPaperHeight);
            painter.drawImage(targetChunkRect, chunk);


            currentImgY += chunkImgHeight;
            pageNum++;
        }


               // --- 5. RESTORE UI COLORS ---
        for (const ItemState& state : originalStates) {
            if (ReservationItem* resItem = dynamic_cast<ReservationItem*>(state.item)) {
                resItem->setBrush(state.brush);
                resItem->setPen(state.pen);
            } else if (QGraphicsTextItem* textItem = dynamic_cast<QGraphicsTextItem*>(state.item)) {
                textItem->setDefaultTextColor(state.textColor);
            } else if (QGraphicsLineItem* lineItem = dynamic_cast<QGraphicsLineItem*>(state.item)) {
                lineItem->setPen(state.pen);
            }
        }
    }
}