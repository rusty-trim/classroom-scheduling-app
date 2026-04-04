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
                        "by_weekday INTEGER, "
                        "overwriteable INTEGER" // SQLite uses 1 for True, 0 for False
                      ")");
}

std::optional<Database::Reservation> Database::Reservation::insert(const Reservation &r)
{
    QSqlDatabase db = QSqlDatabase::database();

    if(!db.transaction())
    {   qDebug() << db.lastError().text();
        return std::nullopt;
    }

    QSqlQuery query;
    query.prepare(
        "INSERT INTO reservations "
        "(name, room, start_date, start_time, end_time, end_date, frequency, by_weekday, overwriteable) "
        "VALUES (:name, :room, :start_date, :start_time, :end_time, :end_date, :frequency, :by_weekday, :overwriteable)"
        );

    query.bindValue(":name", r.name);
    query.bindValue(":room", r.room);
    query.bindValue(":start_date", r.startDate.toString("yyyy-MM-dd"));
    query.bindValue(":start_time", r.startTime.toString("HH:mm"));
    query.bindValue(":end_time", r.endTime.toString("HH:mm"));
    query.bindValue(":end_date", r.endDate.toString("yyyy-MM-dd"));
    query.bindValue(":frequency", r.frequency);
    query.bindValue(":by_weekday", r.byWeekday);
    query.bindValue(":overwriteable", r.overwriteable ? 1 : 0);

    if (!query.exec()) {
        qDebug() << "Reservation insert failed:" << query.lastError().text();
        db.rollback();
        return std::nullopt;
    }

    Reservation created = r;
    created.id = query.lastInsertId().toInt();

           // No open transaction above this point, so Slot::insert() has a clean connection
    if (!Database::Slot::insert(
            created.id,
            r.room,
            r.startDate.toString("yyyy-MM-dd"),
            r.endDate.toString("yyyy-MM-dd"),
            r.startTime.toString("HH:mm"),
            r.endTime.toString("HH:mm"),
            r.frequency,
            r.byWeekday,
            r.overwriteable ? 1 : 0
            ))
    {
        db.rollback();
        return std::nullopt;
    }

    if(!db.commit())
    {
        qDebug() << db.lastError().text();
        db.rollback();
        return std::nullopt;
    }

    return created;
}

QList<Database::Reservation> Database::Reservation::retrieve()
{
    QList<Database::Reservation> list;

    QSqlQuery query("SELECT id, name, room, start_date, start_time, end_time, end_date, frequency, by_weekday, overwriteable FROM reservations");

    while (query.next()) {
        Reservation r;
        r.id = query.value(0).toInt();
        r.name = query.value(1).toString();
        r.room = query.value(2).toString();

        r.startDate = QDate::fromString(query.value(3).toString(), "yyyy-MM-dd");
        r.startTime = QTime::fromString(query.value(4).toString(), "HH:mm");
        r.endTime = QTime::fromString(query.value(5).toString(), "HH:mm");
        r.endDate = QDate::fromString(query.value(6).toString(), "yyyy-MM-dd");

        r.frequency = query.value(7).toInt();
        r.byWeekday = query.value(8).toInt();

        r.overwriteable = query.value(9).toInt() == 1;

        list.append(r);
    }

    return list;
}

bool Database::Reservation::update(const Reservation &r)
{
    QSqlQuery query;
    query.prepare("UPDATE reservations SET "
                  "name = :name, room = :room, start_date = :start_date, "
                  "start_time = :start_time, end_time = :end_time, "
                  "end_date = :end_date, frequency = :frequency, "
                  "by_weekday = :by_weekday, "
                  "overwriteable = :overwriteable "
                  "WHERE id = :id");

    query.bindValue(":name", r.name);
    query.bindValue(":room", r.room);

    // FIX: Convert QDate/QTime to formatted strings so SQLite accepts them
    query.bindValue(":start_date", r.startDate.toString("yyyy-MM-dd"));
    query.bindValue(":start_time", r.startTime.toString("HH:mm"));
    query.bindValue(":end_time", r.endTime.toString("HH:mm"));
    query.bindValue(":end_date", r.endDate.toString("yyyy-MM-dd"));

    query.bindValue(":frequency", r.frequency);
    query.bindValue(":by_weekday", r.byWeekday);
    query.bindValue(":overwriteable", r.overwriteable ? 1 : 0);
    query.bindValue(":id", r.id);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

void Database::Reservation::remove()
{

}