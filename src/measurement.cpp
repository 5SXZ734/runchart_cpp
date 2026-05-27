#include "measurement.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <ctime>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

// Helper function to parse ISO 8601 timestamp string: "2024-04-08T12:00:00.000Z"
static std::chrono::system_clock::time_point parseIsoTimestamp(const std::string& iso) {
    // Extract just the datetime part (first 19 characters): "2024-04-08T12:00:00"
    if (iso.length() < 19) {
        throw std::runtime_error("Invalid timestamp format (too short): " + iso);
    }

    std::tm tm{};
    std::istringstream ss(iso.substr(0, 19));
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        throw std::runtime_error("Invalid timestamp format: " + iso);
    }

    // Convert to time_t using GMT/UTC
    std::time_t time = timegm(&tm);
    if (time == -1) {
        throw std::runtime_error("Invalid timestamp value: " + iso);
    }

    return std::chrono::system_clock::from_time_t(time);
}

Measurement::Measurement(const runchart::DataPoint& point) {
    timestamp = std::chrono::system_clock::time_point{std::chrono::seconds(point.timestamp().seconds())};
    partNumber = point.part_number();
    nominal = point.spec_nominal();
    tolerance = point.spec_tolerance();
    measurement = point.measurement();
}

Measurement::Measurement(
    const std::string& _timestampIso,
    const std::string& _partNumber,
    double _nominal,
    double _tolerance,
    double _measurement
)
    : timestamp(parseIsoTimestamp(_timestampIso)),
      partNumber(_partNumber),
      nominal(_nominal),
      tolerance(_tolerance),
      measurement(_measurement)
{
}

runchart::DataPoint Measurement::toDataPoint() const {
    runchart::DataPoint point;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
    point.mutable_timestamp()->set_seconds(seconds);
    point.set_part_number(partNumber);
    point.set_spec_nominal(nominal);
    point.set_spec_tolerance(tolerance);
    point.set_measurement(measurement);
    return point;
}

std::string Measurement::toString() const {
    std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << "Timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC") << '\n'
        << "Part number: " << partNumber << '\n'
        << "Spec: " << nominal << " +/- " << tolerance << '\n'
        << "Measurement: " << measurement << '\n';
    return out.str();
}

Measurement Measurement::defaultMeasurement() {
    Measurement m;
    m.timestamp = std::chrono::system_clock::now();
    m.partNumber = "111-22-3344";
    m.nominal = 10.0;
    m.tolerance = 1.0;
    m.measurement = 10.0;
    return m;
}

std::chrono::system_clock::time_point Measurement::parseIsoUtc(const std::string& value) {
    return parseIsoTimestamp(value);
}
