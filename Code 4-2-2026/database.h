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

class Slot
{
  public:
    int id;
    int reservationId;
    QString roomId;
    QDate date;
    QTime startTime;
    QTime endTime;
    QString notes;
    bool overwriteable;

    static bool create();
    static bool insert(int reservationId,
                       const QString& roomId,
                       const QString& startDateStr,
                       const QString& endDateStr,
                       const QString& startTime,
                       const QString& endTime,
                       int byWeekday,         // bitmask: Mon=1<<0, Tue=1<<1, ..., Sun=1<<6
                       int overwriteable);
    static QList<Database::Slot> retrieve();
    static void update();
    static bool remove();
};
}

#endif // DATABASE_H
