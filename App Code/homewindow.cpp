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
                onSkip = nullptr; // Prevent double-triggers
            }
            return true; // Consume the click/key so it doesn't accidentally press a button underneath
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

    // Force the app to always open on the first tab (Welcome / Rooms)
    ui->tabWidget->setCurrentIndex(0);

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
