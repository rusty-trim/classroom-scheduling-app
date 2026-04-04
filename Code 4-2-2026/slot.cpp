#include "database.h"
#include "qsqlerror.h"
#include "qsqlquery.h"
#include <QDebug>

using namespace std::chrono;

bool Database::Slot::create()
{
    QSqlQuery query;

           // Create table
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS slots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "reservation_id INTEGER, "
        "room TEXT, "
        "date TEXT, "
        "start_time TEXT, "
        "end_time TEXT, "
        "overwriteable INTEGER)");

    if (!ok) {
        qDebug() << "Table creation failed:" << query.lastError().text();
        return false;
    }

           // Optional: create index
    ok = query.exec(
        "CREATE INDEX IF NOT EXISTS idx_slots_room_date_time_overwriteable "
        "ON slots(room, date, start_time, end_time, overwriteable)"
        );

    if (!ok) {
        qDebug() << "Index creation failed:" << query.lastError().text();
    }

    return ok;
}


bool Database::Slot::insert(
    int reservationId,
    const QString& roomId,
    const QString& startDateStr,
    const QString& endDateStr,
    const QString& startTime,
    const QString& endTime,
    int frequency,
    int byWeekday,
    int overwriteable
    ) {

    QDate startDate = QDate::fromString(startDateStr, "yyyy-MM-dd");
    QDate endDate   = QDate::fromString(endDateStr, "yyyy-MM-dd");
    QTime tStart = QTime::fromString(startTime, "HH:mm");
    QTime tEnd   = QTime::fromString(endTime,   "HH:mm");

    if (!startDate.isValid() || !endDate.isValid() || !tStart.isValid() || !tEnd.isValid() || tEnd <= tStart) {
        qDebug() << "Invalid date/time parameters";
        return false;
    }

    for (QDate current = startDate; current <= endDate; current = current.addDays(1)) {

        // Determine if this date should get a slot based on frequency
        bool shouldInsert = false;

        if (frequency == 0) {
            // Does not repeat — only the start date
            shouldInsert = (current == startDate);

        } else if (frequency == 1) {
            // Every day — always insert
            shouldInsert = true;

        } else if (frequency == 2) {
            // Every week — only on selected weekdays
            int weekday = current.dayOfWeek(); // 1=Mon ... 7=Sun
            int weekdayBit = 1 << (weekday - 1);
            shouldInsert = (byWeekday & weekdayBit) != 0;

        } else if (frequency == 3) {
            // Every month — same day of the month as start date
            shouldInsert = (current.day() == startDate.day());

        } else if (frequency == 4) {
            // Every year — same month and day as start date
            shouldInsert = (current.month() == startDate.month() &&
                            current.day() == startDate.day());
        }

        if (!shouldInsert) continue;

        QString currentDate = current.toString("yyyy-MM-dd");

               // Conflict check
        QSqlQuery conflictQuery;
        conflictQuery.prepare(
            "SELECT 1 FROM slots "
            "WHERE room = ? "
            "AND date = ? "
            "AND start_time < ? "
            "AND end_time > ? "
            "AND overwriteable = 0 "
            "LIMIT 1"
            );
        conflictQuery.addBindValue(roomId);
        conflictQuery.addBindValue(currentDate);
        conflictQuery.addBindValue(endTime);    // existing start_time < new end
        conflictQuery.addBindValue(startTime);  // existing end_time > new start

        if (!conflictQuery.exec()) {
            qDebug() << "Conflict check failed:" << conflictQuery.lastError().text();
            return false;
        }

        if (conflictQuery.next()) {
            qDebug() << "Conflict detected on" << currentDate;
            return false;
        }

               // Insert slot
        QSqlQuery insertQuery;
        insertQuery.prepare(
            "INSERT INTO slots "
            "(reservation_id, room, date, start_time, end_time, overwriteable) "
            "VALUES (?, ?, ?, ?, ?, ?)"
            );
        insertQuery.addBindValue(reservationId);
        insertQuery.addBindValue(roomId);
        insertQuery.addBindValue(currentDate);
        insertQuery.addBindValue(startTime);
        insertQuery.addBindValue(endTime);
        insertQuery.addBindValue(overwriteable ? 1 : 0);

        if (!insertQuery.exec()) {
            qDebug() << "Slot insert failed:" << insertQuery.lastError().text();
            return false;
        }
    }

    return true;
}

QList<Database::Slot> Database::Slot::retrieve()
{
    QList<Database::Slot> list;

    QSqlQuery query("SELECT id, reservation_id, room, date, start_time, end_time, overwriteable FROM slots");

    while (query.next()) {
        Slot s;
        s.id = query.value(0).toInt();
        s.reservationId = query.value(1).toInt();
        s.roomId = query.value(2).toString();
        s.date = QDate::fromString(query.value(3).toString(), "yyyy-MM-dd");
        s.startTime = QTime::fromString(query.value(4).toString(), "HH:mm");
        s.endTime = QTime::fromString(query.value(5).toString(), "HH:mm");
        s.overwriteable = query.value(6).toInt() == 1;
        list.append(s);
    }

    return list;
}