#ifndef HOMEWINDOW_H
#define HOMEWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "database.h"
#include <QGraphicsScene>

QT_BEGIN_NAMESPACE
namespace Ui { class HomeWindow; }
QT_END_NAMESPACE

class HomeWindow : public QMainWindow
{
    Q_OBJECT

public:
    HomeWindow(QWidget *parent = nullptr);
    ~HomeWindow();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Room Buttons
    void on_btnCreateRoom_clicked();
    void on_btnDeleteRoom_clicked();

    // Reservation Buttons
    void on_btnCreateReservation_clicked();
    void on_btnEditReservation_clicked();
    void on_btnDeleteReservation_clicked();

    // Filter Slots
    void on_filterRoomComboBox_currentTextChanged(const QString &arg1);
    void on_filterDateCheckBox_toggled(bool checked);
    void on_filterDateEdit_dateChanged(const QDate &date);

    void on_btnPrintSchedule_clicked();

    void on_btnImportDatabase_clicked();

    void on_btnExportDatabase_clicked();

  private:
    Ui::HomeWindow *ui;

    // Database Functions
    QSqlDatabase db;
    bool setupDatabase();
    void loadRoomsFromDatabase();
    void extracted(QList<Database::Reservation> &reservations);
    void loadReservationsFromDatabase();

    // Custom Graphics Schedule (Visual Only)
    QGraphicsScene *scheduleScene;
    void drawSchedule();
    bool isReservationOnDate(const Database::Reservation &r, const QDate &targetDate);

    // Helper Functions
    void applyFilters();
    void handleScheduleClick(const Database::Reservation &r);

};

#endif // HOMEWINDOW_H