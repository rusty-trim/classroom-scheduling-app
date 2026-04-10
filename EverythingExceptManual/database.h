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

    static bool create();
    static std::optional<Reservation> insert(const Reservation &r, QString &error);
    static QList<Database::Reservation> retrieve();
    static bool update(const Reservation &r, QString &error);
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

    static bool create();
    static bool insert(int reservationId,
                       const QString& roomId,
                       const QString& startDateStr,
                       const QString& endDateStr,
                       const QString& startTime,
                       const QString& endTime,
                       int frequency,    // Add this parameter
                       int byWeekday,         // bitmask: Mon=1<<0, Tue=1<<1, ..., Sun=1<<6
                       QString &error);
    static QList<Database::Slot> retrieve();
    static void update();
    static bool remove();
    static bool updateTime(int reservationId,
                           const QDate &date,
                           const QString &newRoom,
                           const QTime &newStart,
                           const QTime &newEnd,
                           QString &error);
    static bool removeOne(int reservationId, const QDate &date);

};
}

#endif // DATABASE_H
