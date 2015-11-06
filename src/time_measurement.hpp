#ifndef TIME_MEASUREMENT_HPP
#define TIME_MEASUREMENT_HPP

#include <fast-lib/serialization/serializable.hpp>
#include <boost/timer/timer.hpp>

#include <string>
#include <unordered_map>

// TODO: Add timer guard.
class Time_measurement :
	public fast::Serializable
{
public:
	explicit Time_measurement(bool enable_time_measurement = false);
	~Time_measurement();

	void tick(const std::string &timer_name);
	void tock(const std::string &timer_name);

	bool empty() const;

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;
private:
	bool enabled;
	std::unordered_map<std::string, boost::timer::cpu_timer> timers;
};
YAML_CONVERT_IMPL(Time_measurement)

#endif
