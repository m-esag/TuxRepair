#include <QApplication>
#include <QDir>
#include "main_window.h"
#include "db_manager.h"
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Core offline-first: SQLite file in the working directory
    QString dbPath = QDir::current().filePath("tuxrepair.db");
    
    auto db = std::make_shared<tuxrepair::DBManager>();
    if (!db->open(dbPath.toStdString())) {
        std::cerr << "Failed to open SQLite database at: " << dbPath.toStdString() << std::endl;
        return 1;
    }

    // Seed dummy customer/vehicle data if DB is empty for demonstration purposes
    if (db->searchVehiclesByPlate("%").empty()) {
        tuxrepair::Customer c;
        c.first_name = "John";
        c.last_name = "Doe";
        c.middle_name = "A.";
        c.address = "123 Main St";
        c.city = "Tulare, CA";
        c.phone_number = "555-0199";
        int c_id = db->insertCustomer(c);
        if (c_id != -1) {
            tuxrepair::Vehicle v;
            v.customer_id = c_id;
            v.license_plate = "7XYZ89";
            v.vin = "1FA6P8CF0HXXXXXX";
            v.year = 2017;
            v.model = "Ford Mustang GT";
            v.engine_specs = "5.0L V8 naturally aspirated";
            db->insertVehicle(v);
        }
    }

    tuxrepair::MainWindow w(db);
    w.show();

    return app.exec();
}
