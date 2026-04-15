#include <QSqlDatabase>
#include <QSqlQuery>
#include "database.h"
#include "qsqlerror.h"

bool Database::Reservation::create()
{
    QSqlQuery query;
    return query.exec("CREATE TABLE IF NOT EXISTS reservations ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT, "
                      "room TEXT, "
                      "start_date TEXT, "
                      "start_time TEXT, "
                      "end_time TEXT, "
                      "end_date TEXT, "
                      "frequency INTEGER, "
                      "by_weekday INTEGER"
                      ")");
}

std::optional<Database::Reservation> Database::Reservation::insert(const Reservation &r, QString &error)
{
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.transaction()) {
        error = "Could not start database transaction: " + db.lastError().text();
        return std::nullopt;
    }

    int newId = -1;
    {
        QSqlQuery query;
        query.prepare(
            "INSERT INTO reservations "
            "(name, room, start_date, start_time, end_time, end_date, frequency, by_weekday) "
            "VALUES (:name, :room, :start_date, :start_time, :end_time, :end_date, :frequency, :by_weekday)"
        );
        query.bindValue(":name",       r.name);
        query.bindValue(":room",       r.room);
        query.bindValue(":start_date", r.startDate.toString("yyyy-MM-dd"));
        query.bindValue(":start_time", r.startTime.toString("HH:mm"));
        query.bindValue(":end_time",   r.endTime.toString("HH:mm"));
        query.bindValue(":end_date",   r.endDate.toString("yyyy-MM-dd"));
        query.bindValue(":frequency",  r.frequency);
        query.bindValue(":by_weekday", r.byWeekday);

        if (!query.exec()) {
            error = "Failed to save reservation: " + query.lastError().text();
            db.rollback();
            return std::nullopt;
        }
        newId = query.lastInsertId().toInt();
    }

    Reservation created = r;
    created.id = newId;

    if (!Database::Slot::insert(
            created.id,
            r.room,
            r.startDate.toString("yyyy-MM-dd"),
            r.endDate.toString("yyyy-MM-dd"),
            r.startTime.toString("HH:mm"),
            r.endTime.toString("HH:mm"),
            r.frequency,
            r.byWeekday,
            error
        ))
    {
        db.rollback();
        return std::nullopt;
    }

    if (!db.commit()) {
        error = "Failed to commit transaction: " + db.lastError().text();
        db.rollback();
        return std::nullopt;
    }

    return created;
}

QList<Database::Reservation> Database::Reservation::retrieve()
{
    QList<Database::Reservation> list;
    QSqlQuery query("SELECT id, name, room, start_date, start_time, end_time, end_date, frequency, by_weekday FROM reservations");

    while (query.next()) {
        Reservation r;
        r.id        = query.value(0).toInt();
        r.name      = query.value(1).toString();
        r.room      = query.value(2).toString();
        r.startDate = QDate::fromString(query.value(3).toString(), "yyyy-MM-dd");
        r.startTime = QTime::fromString(query.value(4).toString(), "HH:mm");
        r.endTime   = QTime::fromString(query.value(5).toString(), "HH:mm");
        r.endDate   = QDate::fromString(query.value(6).toString(), "yyyy-MM-dd");
        r.frequency = query.value(7).toInt();
        r.byWeekday = query.value(8).toInt();
        list.append(r);
    }

    return list;
}

bool Database::Reservation::update(const Reservation &r, QString &error)
{
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.transaction()) {
        error = "Could not start database transaction: " + db.lastError().text();
        return false;
    }

    // 1. Fetch old reservation data before updating
    QDate oldStartDate, oldEndDate;
    int oldFrequency = 0;
    int oldByWeekday = 0;
    {
        QSqlQuery oldRes;
        oldRes.prepare("SELECT start_date, end_date, frequency, by_weekday FROM reservations WHERE id = ?");
        oldRes.addBindValue(r.id);
        if (!oldRes.exec() || !oldRes.next()) {
            error = "Could not load the existing reservation: " + oldRes.lastError().text();
            db.rollback();
            return false;
        }
        oldStartDate = QDate::fromString(oldRes.value(0).toString(), "yyyy-MM-dd");
        oldEndDate   = QDate::fromString(oldRes.value(1).toString(), "yyyy-MM-dd");
        oldFrequency = oldRes.value(2).toInt();
        oldByWeekday = oldRes.value(3).toInt();
    }

    // 2. Collect existing slot dates
    QSet<QString> existingSlotDates;
    {
        QSqlQuery existing;
        existing.prepare("SELECT date FROM slots WHERE reservation_id = ?");
        existing.addBindValue(r.id);
        if (!existing.exec()) {
            error = "Could not load existing reservation slots: " + existing.lastError().text();
            db.rollback();
            return false;
        }
        while (existing.next())
            existingSlotDates.insert(existing.value(0).toString());
    }

    // 3. Compute old expected dates and find individually-deleted ones
    QSet<QString> oldExpectedDates;
    for (QDate current = oldStartDate; current <= oldEndDate; current = current.addDays(1)) {
        bool shouldExist = false;
        if (oldFrequency == 0)      shouldExist = (current == oldStartDate);
        else if (oldFrequency == 1) shouldExist = true;
        else if (oldFrequency == 2) shouldExist = (oldByWeekday & (1 << (current.dayOfWeek() - 1))) != 0;
        else if (oldFrequency == 3) shouldExist = (current.day() == oldStartDate.day());
        else if (oldFrequency == 4) shouldExist = (current.month() == oldStartDate.month() &&
                                                   current.day()   == oldStartDate.day());
        if (shouldExist)
            oldExpectedDates.insert(current.toString("yyyy-MM-dd"));
    }
    QSet<QString> deletedDates = oldExpectedDates - existingSlotDates;

    // 4. Update the reservation row
    {
        QSqlQuery query;
        query.prepare("UPDATE reservations SET "
                      "name = :name, room = :room, start_date = :start_date, "
                      "start_time = :start_time, end_time = :end_time, "
                      "end_date = :end_date, frequency = :frequency, "
                      "by_weekday = :by_weekday "
                      "WHERE id = :id");
        query.bindValue(":name",       r.name);
        query.bindValue(":room",       r.room);
        query.bindValue(":start_date", r.startDate.toString("yyyy-MM-dd"));
        query.bindValue(":start_time", r.startTime.toString("HH:mm"));
        query.bindValue(":end_time",   r.endTime.toString("HH:mm"));
        query.bindValue(":end_date",   r.endDate.toString("yyyy-MM-dd"));
        query.bindValue(":frequency",  r.frequency);
        query.bindValue(":by_weekday", r.byWeekday);
        query.bindValue(":id",         r.id);
        if (!query.exec() || query.numRowsAffected() == 0) {
            error = "Failed to save reservation changes: " + query.lastError().text();
            db.rollback();
            return false;
        }
    }

    // 5. Delete all existing slots
    {
        QSqlQuery deleteSlots;
        deleteSlots.prepare("DELETE FROM slots WHERE reservation_id = ?");
        deleteSlots.addBindValue(r.id);
        if (!deleteSlots.exec()) {
            error = "Failed to refresh reservation slots: " + deleteSlots.lastError().text();
            db.rollback();
            return false;
        }
    }

    // 6. Reinsert slots skipping individually-deleted dates
    for (QDate current = r.startDate; current <= r.endDate; current = current.addDays(1)) {
        bool shouldExist = false;
        if (r.frequency == 0)      shouldExist = (current == r.startDate);
        else if (r.frequency == 1) shouldExist = true;
        else if (r.frequency == 2) shouldExist = (r.byWeekday & (1 << (current.dayOfWeek() - 1))) != 0;
        else if (r.frequency == 3) shouldExist = (current.day() == r.startDate.day());
        else if (r.frequency == 4) shouldExist = (current.month() == r.startDate.month() &&
                                                   current.day()   == r.startDate.day());
        if (!shouldExist) continue;

        QString dateStr = current.toString("yyyy-MM-dd");
        bool wasInOldRange = (current >= oldStartDate && current <= oldEndDate);
        if (wasInOldRange && deletedDates.contains(dateStr)) continue;

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
            conflictQuery.bindValue(":room", r.room);
            conflictQuery.bindValue(":date", dateStr);
            conflictQuery.bindValue(":end_time", r.endTime.toString("HH:mm"));
            conflictQuery.bindValue(":start_time", r.startTime.toString("HH:mm"));
            conflictQuery.bindValue(":reservation_id", r.id);
            if (!conflictQuery.exec()) {
                error = "Database error during conflict check: " + conflictQuery.lastError().text();
                db.rollback();
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
                db.rollback();
                return false;
            }
        }

        {
            QSqlQuery insertQuery;
            insertQuery.prepare(
                "INSERT INTO slots (reservation_id, room, date, start_time, end_time) "
                "VALUES (?, ?, ?, ?, ?)"
            );
            insertQuery.addBindValue(r.id);
            insertQuery.addBindValue(r.room);
            insertQuery.addBindValue(dateStr);
            insertQuery.addBindValue(r.startTime.toString("HH:mm"));
            insertQuery.addBindValue(r.endTime.toString("HH:mm"));
            if (!insertQuery.exec()) {
                error = "Failed to save one of the reservation occurrences: " + insertQuery.lastError().text();
                db.rollback();
                return false;
            }
        }
    }

    if (!db.commit()) {
        error = "Failed to commit reservation update: " + db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

void Database::Reservation::remove() {}
