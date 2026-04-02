#include "database.h"
#include "qsqlerror.h"
#include "qsqlquery.h"
#include <chrono>
#include <ctime>

using namespace std::chrono;

bool Database::Slot::create()
{
    QSqlQuery query;

           // Create table
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS slots ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "reservation_id INTEGER, "
        "room_id TEXT, "
        "date TEXT, "
        "start_time TEXT, "
        "end_time TEXT, "
        "notes TEXT, "
        "FOREIGN KEY (reservation_id) REFERENCES reservations(id)"
        ");"
        );
    if (!ok) return false;

           // Create index for fast conflict checking
    ok = query.exec(
        "CREATE INDEX IF NOT EXISTS idx_room_date_time "
        "ON slots(room_id, date, start_time, end_time);"
        );

    return ok;
}

bool Database::Slot::insert(
    int reservationId,
    const QString& roomId,
    const QString& startDateStr,
    const QString& endDateStr,
    const QString& startTime,
    const QString& endTime,
    int byWeekday,
    int overwriteable
    ) {
    QSqlQuery query;
    query.exec("BEGIN TRANSACTION");

           // Parse start and end dates
    std::tm startTm{}, endTm{};
    strptime(startDateStr.toStdString().c_str(), "%Y-%m-%d", &startTm);
    strptime(endDateStr.toStdString().c_str(), "%Y-%m-%d", &endTm);

    sys_days start = sys_days{year{startTm.tm_year + 1900}/month{static_cast<unsigned int>(startTm.tm_mon + 1)}/day{static_cast<unsigned int>(startTm.tm_mday)}};
    sys_days end   = sys_days{year{endTm.tm_year + 1900}/month{static_cast<unsigned int>(endTm.tm_mon + 1)}/day{static_cast<unsigned int>(endTm.tm_mday)}};

    for (sys_days current = start; current <= end; current += days{1}) {
        weekday wd{current};

               // Check weekday bitmask
        int weekdayBit = 1 << wd.c_encoding(); // Monday=0
        if ((byWeekday & weekdayBit) == 0) continue; // skip this day

               // Optional: check for conflicts before inserting
        query.prepare(
            "SELECT 1 FROM slots "
            "WHERE room_id = :room_id AND date = :date "
            "AND (start_time < :end_time AND end_time > :start_time) "
            "AND overwriteable = 0 "
            "LIMIT 1"
            );
        query.bindValue(":room_id", roomId);
        query.bindValue(":date", QString::fromStdString(format("%F", current)));
        query.bindValue(":start_time", startTime);
        query.bindValue(":end_time", endTime);

        if (query.exec() && query.next()) {
            // Conflict exists with a non-overwritable slot
            query.exec("ROLLBACK");
            return false;
        }

               // Insert the slot
        query.prepare(
            "INSERT INTO slots "
            "(reservation_id, room_id, date, start_time, end_time, overwriteable) "
            "VALUES (:reservation_id, :room_id, :date, :start_time, :end_time, :overwriteable)"
            );
        query.bindValue(":reservation_id", reservationId);
        query.bindValue(":room_id", roomId);
        query.bindValue(":date", QString::fromStdString(format("%F", current)));
        query.bindValue(":start_time", startTime);
        query.bindValue(":end_time", endTime);
        query.bindValue(":overwriteable", overwriteable);

        if (!query.exec()) {
            qDebug() << "Slot insert failed:" << query.lastError().text();
            query.exec("ROLLBACK");
            return false;
        }
    }

    query.exec("COMMIT");
    return true;
}

QList<Database::Slot> Database::Slot::retrieve()
{
    QList<Database::Slot> list;

    QSqlQuery query("SELECT id, reservation_id, room_id, date, start_time, end_time, notes, overwriteable FROM slots");

    while (query.next()) {
        Slot s;
        s.id = query.value(0).toInt();
        s.reservationId = query.value(1).toInt();
        s.roomId = query.value(2).toString();

        s.date = QDate::fromString(query.value(3).toString(), "yyyy-MM-dd");
        s.startTime = QTime::fromString(query.value(4).toString(), "HH:mm");
        s.endTime = QTime::fromString(query.value(5).toString(), "HH:mm");

        s.notes = query.value(6).toString();
        s.overwriteable = query.value(7).toInt() == 1;

        list.append(s);
    }

    return list;
}


// bool Database::Slot::insert(
//     int reservationId,
//     int roomId,
//     const QString& startDateStr,
//     const QString& endDateStr,
//     const QString& startTime,
//     const QString& endTime,
//     int byWeekday,         // bitmask: Mon=1<<0, Tue=1<<1, ..., Sun=1<<6
//     int overwriteable
//     ) {
//     QSqlQuery query;

//            // Begin transaction for faster inserts
//     query.exec("BEGIN TRANSACTION");

//     // Parse start and end dates
//     std::tm startTm{}, endTm{};
//     strptime(startDateStr.toStdString().c_str(), "%Y-%m-%d", &startTm);
//     strptime(endDateStr.toStdString().c_str(), "%Y-%m-%d", &endTm);

//     sys_days start = sys_days{year{startTm.tm_year + 1900}/(month{static_cast<unsigned int>(startTm.tm_mon + 1)}/day{static_cast<unsigned int>(startTm.tm_mday)})};
//     sys_days end   = sys_days{year{endTm.tm_year + 1900}/(month{static_cast<unsigned int>(endTm.tm_mon + 1)}/day{static_cast<unsigned int>(endTm.tm_mday)})};

//     for (sys_days current = start; current <= end; current += days{1}) {
//         weekday wd{current};

//                // Check weekday bitmask
//         int weekdayBit = 1 << wd.c_encoding(); // Monday=0
//         if ((byWeekday & weekdayBit) == 0) {
//             continue; // skip this day
//         }

//         query.prepare("INSERT INTO slots "
//                       "() "
//                       "VALUES (:res_id :room, :date, :start_time, :end_time, :overwriteable)");
//         query.bindValue(":res_id", reservationId);
//         query.bindValue(":room", roomId);
//         query.bindValue(":date", QString::fromStdString(format("%F", current)));
//         query.bindValue(":start_time", startTime);
//         query.bindValue(":end_time", endTime);
//         query.bindValue(":overwriteable", overwriteable);

//         if (!query.exec()) {
//             query.exec("ROLLBACK");
//             return false; // stop on first error
//         }
//     }

//     query.exec("COMMIT");
//     return true;
// }