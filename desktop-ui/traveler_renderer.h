#ifndef TUXREPAIR_TRAVELER_RENDERER_H
#define TUXREPAIR_TRAVELER_RENDERER_H

#include "models.h"
#include <string>
#include <memory>
#include "db_manager.h"

namespace tuxrepair {

class TravelerRenderer {
public:
    static std::string generateTravelerHTML(const Invoice& invoice, const std::string& template_html, double tax_rate = 0.0825, double supplies_percent = 0.10, int64_t supplies_cap_cents = 3500);
    static bool printToPDF(const Invoice& invoice, const std::string& template_html, const std::string& output_pdf_path, double tax_rate = 0.0825, double supplies_percent = 0.10, int64_t supplies_cap_cents = 3500);
};

} // namespace tuxrepair

#endif // TUXREPAIR_TRAVELER_RENDERER_H
