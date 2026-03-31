#ifndef DATABASE_H
#define DATABASE_H

#include "qdatetime.h"
#include "qlist.h"
#include <optional>
namespace Database
{
class Reservation
{
public:
    int id;
    QString name;
    QString room;
    QDate startDate;
    QTime startTime;
    QTime endTime;
    QDate endDate;
    int frequency;
    int byWeekday;
    bool overwriteable;

    static bool create();
    static std::optional<Reservation> insert(const Reservation &r);
    static QList<Database::Reservation> retrieve();
    static bool update(const Reservation &r);
    static void remove();
};

class Room
{
public:
    static bool create();
    static bool insert(const QString &name, QString &error);
    static QStringList retrieve();
    static void update();
    static bool remove(const QString &name);
};
}

#endif // DATABASE_H
