#include <QSqlDatabase>
#include <QSqlQuery>
#include "database.h"
#include "qsqlerror.h"

bool Database::Room::create()
{
    QSqlQuery query;

    return query.exec("CREATE TABLE IF NOT EXISTS rooms ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT UNIQUE)");
}

bool Database::Room::insert(const QString &name, QString &error)
{
    QSqlQuery query;
    query.prepare("INSERT INTO rooms (name) VALUES (:name)");
    query.bindValue(":name", name);

    if (!query.exec()) {
        error = query.lastError().text();
        return false;
    }

    return true;
}

QStringList Database::Room::retrieve()
{
    QStringList rooms;
    QSqlQuery query("SELECT name FROM rooms");

    while (query.next()) {
        rooms.append(query.value(0).toString());
    }

    return rooms;
}

void Database::Room::update()
{

}

bool Database::Room::remove(const QString &name)
{
    QSqlQuery query;
    query.prepare("DELETE FROM rooms WHERE name = :name");
    query.bindValue(":name", name);

    return query.exec();
}