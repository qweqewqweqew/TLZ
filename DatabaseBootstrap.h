#ifndef DATABASEBOOTSTRAP_H
#define DATABASEBOOTSTRAP_H

#include <QString>

class DatabaseBootstrap
{
public:
    // Discover a local SQL Server instance, create the configured database when
    // needed, and initialize the base schema. Safe to call on every startup.
    static bool ensureDatabase(QString *errorMessage = nullptr);
};

#endif // DATABASEBOOTSTRAP_H
