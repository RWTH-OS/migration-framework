#ifndef PSCOM_HANDLER_HPP
#define PSCOM_HANDLER_HPP

#include <fast-lib/message/migfra/time_measurement.hpp>
#include <fast-lib/message/migfra/task.hpp>

#include <fast-lib/mqtt_communicator.hpp>

#include <memory>
#include <string>

/**
 * \brief The Pscom_handler ensures that processes using parastation are suspended during migration.
 *
 * This handler follows the RAII pattern by suspending processes in the constructor and resuming in the destructor.
 * Thus, processes are tried to be resumed even in error cases.
 * The suspend/resume functions use MQTT to send messages to the request topic.
 * Thereafter, Pscom_handler waits on the processes to confirm suspension/resumption on the response topic.
 */
class Pscom_handler
{	
public:
	Pscom_handler(const fast::msg::migfra::Migrate &task,
		      std::shared_ptr<fast::Communicator> comm,
		      fast::msg::migfra::Time_measurement &time_measurement);
	~Pscom_handler() noexcept(false);

	/**
	 * \brief This static function may be used to alter the topic for requests.
	 *
	 * The default request topic is: "fast/pscom/<vm_name>/any_proc/request".
	 */
	static void set_request_topic_template(std::string request);
	/**
	 * \brief This static function may be used to alter the topic for responses.
	 *
	 * The default response topic is: "fast/pscom/<vm_name>/+/response".
	 */
	static void set_response_topic_template(std::string response);
	/**
	 * \brief This static function may be used to alter the QoS.
	 *
	 * The default QoS is 0.
	 */
	static void set_qos(int qos);
private:
	void suspend();
	void resume();

	const std::string vm_name;
	const unsigned int messages_expected;
	std::shared_ptr<fast::MQTT_communicator> comm;
	unsigned int answers;
	fast::msg::migfra::Time_measurement &time_measurement;
	std::string request_topic;
	std::string response_topic;

	static std::string request_topic_template;
	static std::string response_topic_template;
	static int qos;
};

#endif
