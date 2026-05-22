#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include "runchart.pb.h"

class Measurement {
public:
    std::chrono::system_clock::time_point timestamp{};
    std::string partNumber;
    double nominal = 0.0;
    double tolerance = 0.0;
    double measurement = 0.0;

	Measurement() = default;
	explicit Measurement(const runchart::DataPoint& point);
	Measurement(
		const std::string& timestampIso,
		const std::string& partNumber,
		double nominal,
		double tolerance,
		double measurement
	);

    runchart::DataPoint toDataPoint() const;
    std::string toString() const;

    static Measurement defaultMeasurement();
    static std::chrono::system_clock::time_point parseIsoUtc(const std::string& value);
};
