#ifndef HOMEWINDOW_H
#define HOMEWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

QT_BEGIN_NAMESPACE
namespace Ui { class HomeWindow; }
QT_END_NAMESPACE

class HomeWindow : public QMainWindow
{
    Q_OBJECT

public:
    HomeWindow(QWidget *parent = nullptr);
    ~HomeWindow();

    // ---> ADD THIS NEW PROTECTED SECTION <---
protected:
    void showEvent(QShowEvent *event) override;

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

private:
    Ui::HomeWindow *ui;

    // Database Functions
    QSqlDatabase db;
    bool setupDatabase();
    void loadRoomsFromDatabase();
    void extracted(QList<Database::Reservation> &reservations);
    void loadReservationsFromDatabase();

    // Helper Functions
    void populateTimeComboBoxes();
    void applyFilters();
};

#endif // HOMEWINDOW_H
