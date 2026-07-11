#include "traveler_renderer.h"
#include <QTextDocument>
#include <QPrinter>
#include <QDir>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace tuxrepair {

static std::string generateMockBarcodeSVG(int invoice_id) {
    std::string code = std::to_string(invoice_id);
    std::string svg = "<svg width=\"150\" height=\"50\" viewBox=\"0 0 150 50\">";
    
    int x = 10;
    // Start guard
    svg += "<rect x=\"" + std::to_string(x) + "\" y=\"5\" width=\"2\" height=\"30\" fill=\"black\"/>"; x += 3;
    svg += "<rect x=\"" + std::to_string(x) + "\" y=\"5\" width=\"1\" height=\"30\" fill=\"black\"/>"; x += 3;
    
    // Simple encoding logic
    for (char c : code) {
        int val = c - '0';
        for (int i = 0; i < 3; ++i) {
            int w1 = ((val + i) % 2 == 0) ? 3 : 1;
            svg += "<rect x=\"" + std::to_string(x) + "\" y=\"5\" width=\"" + std::to_string(w1) + "\" height=\"30\" fill=\"black\"/>";
            x += w1 + 2;
        }
    }
    
    // End guard
    svg += "<rect x=\"" + std::to_string(x) + "\" y=\"5\" width=\"1\" height=\"30\" fill=\"black\"/>"; x += 3;
    svg += "<rect x=\"" + std::to_string(x) + "\" y=\"5\" width=\"2\" height=\"30\" fill=\"black\"/>"; x += 3;
    
    // Label
    svg += "<text x=\"40\" y=\"45\" font-family=\"monospace\" font-size=\"10\" fill=\"black\">*TR-" + code + "*</text>";
    svg += "</svg>";
    return svg;
}

std::string TravelerRenderer::generateTravelerHTML(const Invoice& invoice, const std::string& template_html, double tax_rate, double supplies_percent, int64_t supplies_cap_cents) {
    std::string html = template_html;
    if (html.empty()) {
        html = "<html><body><h1>Tech Traveler #{{TICKET_ID}}</h1>{{TICKET_ITEMS_TABLE}}</body></html>";
    }

    double parts_tot = 0;
    double labor_tot = 0;
    for (const auto& item : invoice.items) {
        std::string spec_lower = item.specification;
        spec_lower.erase(0, spec_lower.find_first_not_of(" \t\r\n"));
        spec_lower.erase(spec_lower.find_last_not_of(" \t\r\n") + 1);
        std::transform(spec_lower.begin(), spec_lower.end(), spec_lower.begin(), ::tolower);
        if (spec_lower == "part") {
            parts_tot += item.quantity * (item.unit_price / 100.0);
        } else {
            labor_tot += item.quantity * (item.unit_price / 100.0);
        }
    }
    double supplies = 0.0;
    if (labor_tot > 0 && !invoice.supplies_removed) {
        supplies = labor_tot * supplies_percent;
        double cap = supplies_cap_cents / 100.0;
        if (supplies > cap) supplies = cap;
    }
    double tax = parts_tot * tax_rate;
    double grand = parts_tot + labor_tot + supplies + tax;

    auto fmtDouble = [](double val) {
        std::stringstream ss;
        ss << "$" << std::fixed << std::setprecision(2) << val;
        return ss.str();
    };

    // Render items table
    std::stringstream table_html;
    table_html << "<table style=\"width:100%; border-collapse:collapse; margin-top:10px;\">"
               << "  <thead>"
               << "    <tr style=\"background-color:#f0f0f0; border-bottom:2px solid #ddd;\">"
               << "      <th style=\"padding:6px; text-align:left; border:1px solid #ddd;\">SKU/Code</th>"
               << "      <th style=\"padding:6px; text-align:left; border:1px solid #ddd;\">Description</th>"
               << "      <th style=\"padding:6px; text-align:center; border:1px solid #ddd;\">Qty</th>"
               << "      <th style=\"padding:6px; text-align:right; border:1px solid #ddd;\">Price</th>"
               << "      <th style=\"padding:6px; text-align:right; border:1px solid #ddd;\">Total</th>"
               << "    </tr>"
               << "  </thead>"
               << "  <tbody>";
    for (const auto& item : invoice.items) {
        double unit = item.unit_price / 100.0;
        double tot = (item.quantity * item.unit_price) / 100.0;
        table_html << "    <tr>"
                   << "      <td style=\"padding:6px; border:1px solid #ddd;\">" << item.part_number << "</td>"
                   << "      <td style=\"padding:6px; border:1px solid #ddd;\">" << item.description << "</td>"
                   << "      <td style=\"padding:6px; text-align:center; border:1px solid #ddd;\">" << item.quantity << "</td>"
                   << "      <td style=\"padding:6px; text-align:right; border:1px solid #ddd;\">" << fmtDouble(unit) << "</td>"
                   << "      <td style=\"padding:6px; text-align:right; border:1px solid #ddd;\">" << fmtDouble(tot) << "</td>"
                   << "    </tr>";
    }
    if (invoice.items.empty()) {
        table_html << "    <tr><td colspan=\"5\" style=\"padding:12px; text-align:center; color:#888;\">No work order line items.</td></tr>";
    }
    table_html << "  </tbody></table>";

    // Placeholders replacements
    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };

    replaceAll(html, "{{SHOP_NAME}}", "TuxRepair Garage");
    replaceAll(html, "{{TICKET_ID}}", std::to_string(invoice.id));
    replaceAll(html, "{{CUSTOMER_NAME}}", invoice.customer.first_name + " " + invoice.customer.last_name);
    replaceAll(html, "{{CUSTOMER_PHONE}}", invoice.customer.phone_number);
    replaceAll(html, "{{CUSTOMER_ADDRESS}}", invoice.customer.address + ", " + invoice.customer.city);
    replaceAll(html, "{{VEHICLE_PLATE}}", invoice.vehicle.license_plate);
    replaceAll(html, "{{VEHICLE_YEAR_MODEL}}", std::to_string(invoice.vehicle.year) + " " + invoice.vehicle.model);
    replaceAll(html, "{{VEHICLE_ENGINE}}", invoice.vehicle.engine_specs);
    replaceAll(html, "{{VEHICLE_VIN}}", invoice.vehicle.vin.empty() ? "N/A" : invoice.vehicle.vin);
    replaceAll(html, "{{TICKET_TYPE}}", invoice.ticket_type);
    replaceAll(html, "{{DATE_CREATED}}", invoice.date_created);
    replaceAll(html, "{{MILEAGE_IN}}", std::to_string(invoice.mileage_in));
    replaceAll(html, "{{MILEAGE_OUT}}", std::to_string(invoice.mileage_out));
    replaceAll(html, "{{BILLED_BY}}", invoice.writer);
    
    replaceAll(html, "{{TICKET_SUBTOTAL}}", fmtDouble(parts_tot + labor_tot));
    replaceAll(html, "{{TICKET_SUPPLIES}}", fmtDouble(supplies));
    replaceAll(html, "{{TICKET_TAX}}", fmtDouble(tax));
    replaceAll(html, "{{TICKET_GRAND_TOTAL}}", fmtDouble(grand));
    replaceAll(html, "{{TICKET_ITEMS_TABLE}}", table_html.str());
    replaceAll(html, "{{BARCODE_SVG}}", generateMockBarcodeSVG(invoice.id));

    return html;
}

bool TravelerRenderer::printToPDF(const Invoice& invoice, const std::string& template_html, const std::string& output_pdf_path, double tax_rate, double supplies_percent, int64_t supplies_cap_cents) {
    QTextDocument doc;
    doc.setHtml(QString::fromStdString(generateTravelerHTML(invoice, template_html, tax_rate, supplies_percent, supplies_cap_cents)));
    
    QPrinter printer;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(QString::fromStdString(output_pdf_path));
    
    if (template_html.find("width: 280px") != std::string::npos || template_html.find("width:280px") != std::string::npos) {
        printer.setPageSize(QPageSize(QSizeF(80, 200), QPageSize::Millimeter));
        printer.setPageMargins(QMarginsF(4, 4, 4, 4), QPageLayout::Millimeter);
    } else {
        printer.setPageSize(QPageSize(QPageSize::Letter));
        printer.setPageMargins(QMarginsF(36, 36, 36, 36), QPageLayout::Point);
    }
    
    doc.print(&printer);
    return true;
}

} // namespace tuxrepair
