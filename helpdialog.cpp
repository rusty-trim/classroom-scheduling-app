#include "helpdialog.h"
#include "ui_helpdialog.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QSizePolicy>
#include <QTextDocument>
#include <QWidget>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HelpDialog)
{
    ui->setupUi(this);

    // Make the help window behave like its own normal window
    setWindowTitle("Help / User Manual");
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    resize(900, 650);
    setMinimumSize(500, 350);

    // Always open on Home tab
    ui->tabWidgetHelp->setCurrentIndex(0);

    // Force all child widgets to allow shrinking
    ui->headerHelpLabel->setMinimumSize(0, 0);
    ui->tabWidgetHelp->setMinimumSize(0, 0);
    ui->textBrowserHome->setMinimumSize(0, 0);
    ui->textBrowserReservations->setMinimumSize(0, 0);
    ui->textBrowserSchedule->setMinimumSize(0, 0);
    ui->textBrowserPopups->setMinimumSize(0, 0);

    // Good size policies
    ui->headerHelpLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->tabWidgetHelp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->textBrowserHome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->textBrowserReservations->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->textBrowserSchedule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->textBrowserPopups->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Make sure each text browser can shrink
    ui->textBrowserHome->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->textBrowserReservations->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->textBrowserSchedule->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->textBrowserPopups->setLineWrapMode(QTextEdit::WidgetWidth);

    // Better document margins
    ui->textBrowserHome->document()->setDocumentMargin(12);
    ui->textBrowserReservations->document()->setDocumentMargin(12);
    ui->textBrowserSchedule->document()->setDocumentMargin(12);
    ui->textBrowserPopups->document()->setDocumentMargin(12);

    // Header text
    ui->headerHelpLabel->setText("Application Help");

    // ---- FORCE MAIN LAYOUT ORDER AND REMOVE SPACERS ----
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        // Remove any old layout items/spacers
        while (mainLayout->count() > 0) {
            QLayoutItem *item = mainLayout->takeAt(0);

            if (item->widget()) {
                item->widget()->hide(); // temporarily detach visually
            }

            delete item;
        }

        mainLayout->addWidget(ui->headerHelpLabel);
        mainLayout->addWidget(ui->tabWidgetHelp);
        // no close button

        // Stretch: tabs take the extra space
        mainLayout->setStretch(0, 0); // header
        mainLayout->setStretch(1, 1); // tabs

        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);
    }

    // ---- FORCE EACH TAB PAGE LAYOUT ----
    auto fixTabPage = [](QWidget *page, QWidget *browser) {
        if (!page || !browser) return;

        // Remove old layout if there is one
        QLayout *oldLayout = page->layout();
        if (oldLayout) {
            while (oldLayout->count() > 0) {
                QLayoutItem *item = oldLayout->takeAt(0);
                delete item;
            }
            delete oldLayout;
        }

        browser->setParent(page);
        browser->setMinimumSize(0, 0);
        browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(browser);
        page->setLayout(layout);
    };

    fixTabPage(ui->tab_1, ui->textBrowserHome);
    fixTabPage(ui->tab_2, ui->textBrowserReservations);
    fixTabPage(ui->tab_3, ui->textBrowserSchedule);
    fixTabPage(ui->tab_4, ui->textBrowserPopups);

    // ---- HELP TEXT ----
    QString homeText;
    homeText += "<h2>Home / Welcome Page</h2>";

    homeText += "<h3>How to create a room</h3>";
    homeText += "<ol>";
    homeText += "<li>Go to the <b>Home</b> page.</li>";
    homeText += "<li>Enter the room number, for example <b>209A</b>.(No parameters on this, you can put <b>Zoom</b> or <b>Lobby</b> as a room name)</li>";
    homeText += "<li>Click <b>Create Room</b>.</li>";
    homeText += "<li>A popup window will appear asking you to confirm the room number.</li>";
    homeText += "<li>Confirm the action to save the room to the database.</li>";
    homeText += "</ol>";

    homeText += "<h3>How to delete a room</h3>";
    homeText += "<ol>";
    homeText += "<li>Enter the full room number you want to remove. If you do not, the room name will not be recognized.</li>";
    homeText += "<li>Click <b>Delete Room</b>.</li>";
    homeText += "<li>Confirm within the popup window that this is the exact room you would like to delete.</li>";
    homeText += "<li>If the room has active reservations, you will have to confirm deletion of room and current or future reservations.</li>";
    homeText += "</ol>";

    homeText += "<h3>Other buttons on this page</h3>";
    homeText += "<ul>";
    homeText += "<li><b>Help / Manual</b> opens this help window for extra assistance.</li>";
    homeText += "<li><b>Import Database</b> lets you load an existing or previous database file.</li>";
    homeText += "<li><b>Export Database</b> lets you save a backup copy of the database, which can be good for saving reservations from a previous semester or switching between computers with new updated data.</li>";
    homeText += "</ul>";
    homeText += "<br><br><br>";

    ui->textBrowserHome->setHtml(homeText);

    QString reservationText;
    reservationText += "<h2>Reservations Page</h2>";

    reservationText += "<h3>How to create a reservation</h3>";
    reservationText += "<ol>";
    reservationText += "<li>Go to the <b>Reservations</b> tab.</li>";
    reservationText += "<li>Select an existing room from the room list.</li>";
    reservationText += "<li>Enter the reservation name.</li>";
    reservationText += "<li>Select the start date and start time.</li>";
    reservationText += "<li>Select the end date and end time.</li>";
    reservationText += "<li>Choose the frequency if the reservation repeats.</li>";
    reservationText += "<li>If this is repeating choose the specific days the reservation repeats.</li>";
    reservationText += "<li>Click <b>Create Reservation</b>.</li>";
    reservationText += "<li>Review the confirmation popup and approve the reservation.</li>";
    reservationText += "</ol>";

    reservationText += "<h3>Rules for entering reservations</h3>";
    reservationText += "<ul>";
    reservationText += "<li>A reservation must be tied to an existing room.</li>";
    reservationText += "<li>End time must be after start time.</li>";
    reservationText += "<li>Required fields cannot be left blank.</li>";
    reservationText += "<li>If there is a conflict, the system will block the reservation from being created.</li>";
    reservationText += "</ul>";

    reservationText += "<h3>How to edit a reservation</h3>";
    reservationText += "<ol>";
    reservationText += "<li>Look at the reservations table on the right side.</li>";
    reservationText += "<li>This can be done by either viewing all reservations (default selection) or <b>filter by date</b>.</li>";
    reservationText += "<li>Select the reservation you want to change.</li>";
    reservationText += "<li>Click <b>Edit Selected</b>.</li>";
    reservationText += "<li>The edit popup window will open with the current reservation information filled in.</li>";
    reservationText += "<li>Make your changes and save them.</li>";
    reservationText += "</ol>";

    reservationText += "<h3>How to delete a reservation</h3>";
    reservationText += "<ol>";
    reservationText += "<li>Select the reservation from the table.</li>";
    reservationText += "<li>Click <b>Delete Selected</b>.</li>";
    reservationText += "<li>Read the confirmation popup carefully.</li>";
    reservationText += "<li>Confirm the action to permanently remove the reservation.</li>";
    reservationText += "</ol>";
    reservationText += "<br><br><br>";

    ui->textBrowserReservations->setHtml(reservationText);

    QString scheduleText;
    scheduleText += "<h2>Schedule Page</h2>";

    scheduleText += "<h3>How to view a room schedule</h3>";
    scheduleText += "<ol>";
    scheduleText += "<li>Go to the <b>Schedule</b> tab.</li>";
    scheduleText += "<li>Select the room number you want to view.</li>";
    scheduleText += "<li>The weekly schedule for that room will be displayed.</li>";
    scheduleText += "</ol>";

    scheduleText += "<h3>How to view a different week</h3>";
    scheduleText += "<ol>";
    scheduleText += "<li>Use the week arrows or the drop down arrow next to the current selected date to open the calendar selection option.</li>";
    scheduleText += "<li>Choose the week / month / date you want to view.</li>";
    scheduleText += "<li>The schedule will update to show reservations for that selected week.</li>";
    scheduleText += "</ol>";

    scheduleText += "<h3>How to edit / delete individual reservations</h3>";
    scheduleText += "<ol>";
    scheduleText += "<li>Select the specific reservation block you would like to change.</li>";
    scheduleText += "<li>In the edit popup (same as other tabs) you can choose if you would like to edit the specific reservation, all occurences, or delete.</li>";
    scheduleText += "<li>If you select delete it will open a popup asking if you would like to <b>Delete all Occurences</b> or <b>Delete this day only</b> .</li>";
    scheduleText += "</ol>";

    scheduleText += "<h3>How to print the schedule</h3>";
    scheduleText += "<ol>";
    scheduleText += "<li>Select the room and week you want to print.</li>";
    scheduleText += "<li>Click <b>Print Schedule</b>.</li>";
    scheduleText += "<li>Choose a printer or choose the option to save as PDF if available.</li>";
    scheduleText += "</ol>";
    scheduleText += "<br><br><br>";

    ui->textBrowserSchedule->setHtml(scheduleText);

    QString popupText;
    popupText += "<h2>Popups / Errors</h2>";

    popupText += "<h3>Create room popup</h3>";
    popupText += "<p>When you create a room, a popup will ask you to confirm the room number before it is saved.</p>";

    popupText += "<h3>Delete room popup</h3>";
    popupText += "<p>When you delete a room, a popup will ask you to confirm deletion and let you know it cannot be undone.</p>";

    popupText += "<h3>Create reservation popup</h3>";
    popupText += "<p>When creating a reservation, the system repeats the information back to you so you can confirm it is correct.</p>";

    popupText += "<h3>Edit popup window</h3>";
    popupText += "<p>The edit popup window opens when a selected reservation is being changed. It fills in the current reservation information so the user can update the fields instead of recreating the reservation from scratch.</p>";

    popupText += "<h3>Schedule edit / delete popup</h3>";
    popupText += "<ul>";
    popupText += "<li>Displays the reservation information.</li>";
    popupText += "<li>If you select <b>edit this day</b>, it allows you to edit the reservation.</li>";
    popupText += "<li>If you select <b>edit all occurrences</b>, it allows you to edit the reservation.</li>";
    popupText += "<li>If you select <b>delete</b>, it gives you the option to delete all occurrences or delete this day only.</li>";
    popupText += "</ul>";

    popupText += "<h3>Warning popups</h3>";
    popupText += "<ul>";
    popupText += "<li><b>Missing fields:</b> required information was not entered.</li>";
    popupText += "<li><b>Invalid times:</b> end time must be after start time.</li>";
    popupText += "<li><b>Conflict warning:</b> another reservation already exists in that time slot. (<b>It will only display the first conflicting reservation.</b>)</li>";
    popupText += "<li><b>Database or file error:</b> there may be a problem reading or saving the database.</li>";
    popupText += "</ul>";
    popupText += "<br><br><br>";

    ui->textBrowserPopups->setHtml(popupText);
}

HelpDialog::~HelpDialog()
{
    delete ui;
}
