#include "database.h"
#include "qsqlerror.h"
#include "qsqlquery.h"
#include <QDebug>

bool Database::Slot::create()
{
    QSqlQuery query;
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS slots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "reservation_id INTEGER, "
        "room TEXT, "
        "date TEXT, "
        "start_time TEXT, "
        "end_time TEXT, "
        "FOREIGN KEY (reservation_id) REFERENCES reservations(id) ON DELETE CASCADE"
        ")");

    if (!ok) {
        qDebug() << "Table creation failed:" << query.lastError().text();
        return false;
    }

    ok = query.exec(
        "CREATE INDEX IF NOT EXISTS idx_slots_room_date_time "
        "ON slots(room, date, start_time, end_time)"
    );
    if (!ok)
        qDebug() << "Index creation failed:" << query.lastError().text();

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
    QString &error
    ) {

    QDate startDate = QDate::fromString(startDateStr, "yyyy-MM-dd");
    QDate endDate   = QDate::fromString(endDateStr,   "yyyy-MM-dd");
    QTime tStart    = QTime::fromString(startTime,    "HH:mm");
    QTime tEnd      = QTime::fromString(endTime,      "HH:mm");

    if (!startDate.isValid() || !endDate.isValid() || !tStart.isValid() || !tEnd.isValid()) {
        error = "Invalid date or time values. Please check your inputs.";
        return false;
    }
    if (tEnd <= tStart) {
        error = "End time must be after start time.";
        return false;
    }

    for (QDate current = startDate; current <= endDate; current = current.addDays(1)) {

        bool shouldInsert = false;
        if (frequency == 0)      shouldInsert = (current == startDate);
        else if (frequency == 1) shouldInsert = true;
        else if (frequency == 2) shouldInsert = (byWeekday & (1 << (current.dayOfWeek() - 1))) != 0;
        else if (frequency == 3) shouldInsert = (current.day() == startDate.day());
        else if (frequency == 4) shouldInsert = (current.month() == startDate.month() &&
                                                  current.day()   == startDate.day());
        if (!shouldInsert) continue;

        QString currentDate = current.toString("yyyy-MM-dd");

        {
            QSqlQuery conflictQuery;
            conflictQuery.prepare(
                "SELECT reservations.name, slots.room, slots.date, slots.start_time, slots.end_time "
                "FROM slots "
                "JOIN reservations ON reservations.id = slots.reservation_id "
                "WHERE slots.room = :room AND slots.date = :date "
                "AND slots.start_time < :end_time AND slots.end_time > :start_time "
                "LIMIT 1"
            );
            conflictQuery.bindValue(":room", roomId);
            conflictQuery.bindValue(":date", currentDate);
            conflictQuery.bindValue(":end_time", endTime);
            conflictQuery.bindValue(":start_time", startTime);

            if (!conflictQuery.exec()) {
                error = "Database error during conflict check: " + conflictQuery.lastError().text();
                return false;
            }
            if (conflictQuery.next()) {
                error = QString("This reservation conflicts with:\n\n"
                                "Name: %1\n"
                                "Room: %2\n"
                                "Date: %3\n"
                                "Time: %4 - %5")
                            .arg(conflictQuery.value(0).toString(),
                                 conflictQuery.value(1).toString(),
                                 QDate::fromString(conflictQuery.value(2).toString(), "yyyy-MM-dd").toString("MM/dd/yyyy"),
                                 QTime::fromString(conflictQuery.value(3).toString(), "HH:mm").toString("hh:mm AP"),
                                 QTime::fromString(conflictQuery.value(4).toString(), "HH:mm").toString("hh:mm AP"));
                return false;
            }
        }

        {
            QSqlQuery insertQuery;
            insertQuery.prepare(
                "INSERT INTO slots (reservation_id, room, date, start_time, end_time) "
                "VALUES (?, ?, ?, ?, ?)"
            );
            insertQuery.addBindValue(reservationId);
            insertQuery.addBindValue(roomId);
            insertQuery.addBindValue(currentDate);
            insertQuery.addBindValue(startTime);
            insertQuery.addBindValue(endTime);

            if (!insertQuery.exec()) {
                error = "Database error while saving slot: " + insertQuery.lastError().text();
                return false;
            }
        }
    }

    return true;
}

QList<Database::Slot> Database::Slot::retrieve()
{
    QList<Database::Slot> list;
    QSqlQuery query("SELECT id, reservation_id, room, date, start_time, end_time FROM slots");

    while (query.next()) {
        Slot s;
        s.id            = query.value(0).toInt();
        s.reservationId = query.value(1).toInt();
        s.roomId        = query.value(2).toString();
        s.date          = QDate::fromString(query.value(3).toString(), "yyyy-MM-dd");
        s.startTime     = QTime::fromString(query.value(4).toString(), "HH:mm");
        s.endTime       = QTime::fromString(query.value(5).toString(), "HH:mm");
        list.append(s);
    }

    return list;
}

bool Database::Slot::updateTime(int reservationId,
                                const QDate &date,
                                const QString &newRoom,
                                const QTime &newStart,
                                const QTime &newEnd,
                                QString &error)
{
    if (!newStart.isValid() || !newEnd.isValid() || newEnd <= newStart) {
        error = "End time must be after start time.";
        return false;
    }

    if (newRoom.isEmpty()) {
        error = "Please choose a room.";
        return false;
    }

    {
        QSqlQuery conflictQuery;
        conflictQuery.prepare(
            "SELECT reservations.name, slots.room, slots.date, slots.start_time, slots.end_time "
            "FROM slots "
            "JOIN reservations ON reservations.id = slots.reservation_id "
            "WHERE slots.room = :room AND slots.date = :date "
            "AND slots.start_time < :end_time AND slots.end_time > :start_time "
            "AND slots.reservation_id != :reservation_id "
            "LIMIT 1"
        );
        conflictQuery.bindValue(":room", newRoom);
        conflictQuery.bindValue(":date", date.toString("yyyy-MM-dd"));
        conflictQuery.bindValue(":end_time", newEnd.toString("HH:mm"));
        conflictQuery.bindValue(":start_time", newStart.toString("HH:mm"));
        conflictQuery.bindValue(":reservation_id", reservationId);

        if (!conflictQuery.exec()) {
            error = "Database error during conflict check: " + conflictQuery.lastError().text();
            return false;
        }
        if (conflictQuery.next()) {
            error = QString("This change conflicts with:\n\n"
                            "Name: %1\n"
                            "Room: %2\n"
                            "Date: %3\n"
                            "Time: %4 - %5")
                        .arg(conflictQuery.value(0).toString(),
                             conflictQuery.value(1).toString(),
                             QDate::fromString(conflictQuery.value(2).toString(), "yyyy-MM-dd").toString("MM/dd/yyyy"),
                             QTime::fromString(conflictQuery.value(3).toString(), "HH:mm").toString("hh:mm AP"),
                             QTime::fromString(conflictQuery.value(4).toString(), "HH:mm").toString("hh:mm AP"));
            return false;
        }
    }

    {
        QSqlQuery query;
        query.prepare("UPDATE slots SET room = ?, start_time = ?, end_time = ? WHERE reservation_id = ? AND date = ?");
        query.addBindValue(newRoom);
        query.addBindValue(newStart.toString("HH:mm"));
        query.addBindValue(newEnd.toString("HH:mm"));
        query.addBindValue(reservationId);
        query.addBindValue(date.toString("yyyy-MM-dd"));

        if (!query.exec()) {
            error = "Failed to update the reservation time: " + query.lastError().text();
            return false;
        }
        return query.numRowsAffected() > 0;
    }
}

bool Database::Slot::removeOne(int reservationId, const QDate &date)
{
    QSqlQuery query;
    query.prepare("DELETE FROM slots WHERE reservation_id = ? AND date = ?");
    query.addBindValue(reservationId);
    query.addBindValue(date.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        qDebug() << "Slot::removeOne failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}
