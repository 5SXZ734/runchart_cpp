#include "metrics.h"

#include <sstream>

std::string Metrics::toPrometheusText() const {
    std::ostringstream out;
    out << "runchart_snapshot_requests_total " << snapshot_requests.load() << "\n";
    out << "runchart_send_measurements_requests_total " << send_measurements_requests.load() << "\n";
    out << "runchart_monitor_requests_total " << monitor_requests.load() << "\n";
    out << "runchart_send_and_check_requests_total " << send_and_check_requests.load() << "\n";
    out << "runchart_health_requests_total " << health_requests.load() << "\n";
    return out.str();
}
