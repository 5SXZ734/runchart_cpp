#include "measurement.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <ctime>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

static std::chrono::system_clock::time_point parseIsoTimestamp(const std::string& iso) {
    std::tm tm{};
    std::istringstream ss(iso.substr(0, 19));
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        throw std::runtime_error("Invalid timestamp: " + iso);
    }

    std::time_t time = _mkgmtime(&tm);
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
        << "Spec: " << nominal << "±" << tolerance << '\n'
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
    std::tm tm{};
    std::istringstream in(value.substr(0, 19));
    in >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (in.fail()) {
        throw std::runtime_error("Invalid timestamp: " + value);
    }
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}
