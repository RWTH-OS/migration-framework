#include "mqtt_communicator.hpp"

#include "logging.hpp"

#include <stdexcept>
#include <cstdlib>

MQTT_communicator::MQTT_communicator(const std::string &id, 
				     const std::string &topic,
				     const std::string &host, 
				     int port,
				     int keepalive) :
	mosqpp::mosquittopp(id.c_str()),
	id(id), 
	topic(topic),
	host(host), 
	port(port),
	keepalive(keepalive)
{
	LOG_PRINT(LOG_DEBUG, "Initializing MQTT_communicator...");
	mosqpp::lib_init();
	loop_start();
	connect_async(host.c_str(), port, keepalive);
	/// \todo Move subscribe to a dedicated public method.
	subscribe(nullptr, topic.c_str(), 2);
	LOG_PRINT(LOG_DEBUG, "MQTT_communicator initialized.");
}

MQTT_communicator::~MQTT_communicator()
{
	LOG_PRINT(LOG_DEBUG, "Closing MQTT_communicator...");
	disconnect();
	loop_stop();
	mosqpp::lib_cleanup();
	LOG_PRINT(LOG_DEBUG, "MQTT_communicator closed.");
}

void MQTT_communicator::on_connect(int rc)
{
	if (rc == 0) {
		LOG_STREAM(LOG_DEBUG, "Connection established to " << host << ":" << port);
	} else {
		LOG_STREAM(LOG_ERR, "Error on connect: Code " << rc);
	}
}

void MQTT_communicator::on_disconnect(int rc)
{
	if (rc == 0) {
		LOG_STREAM(LOG_DEBUG, "Disconnected from  " << host << ":" << port);
	} else {
		LOG_STREAM(LOG_ERR, "Unexpected disconnect: Code " << rc);
	}
}

void MQTT_communicator::on_publish(int mid)
{
	LOG_STREAM(LOG_DEBUG, "Message " << mid << " published.");
}

void MQTT_communicator::on_message(const mosquitto_message *msg)
{
	LOG_PRINT(LOG_DEBUG, "on_message executed.");
	mosquitto_message* buf = static_cast<mosquitto_message*>(malloc(sizeof(mosquitto_message)));
	if (!buf) 
		LOG_PRINT(LOG_ERR, "malloc failed allocating mosquitto_message.");
	std::lock_guard<std::mutex> lock(msg_queue_mutex);
	messages.push(buf);
	mosquitto_message_copy(messages.back(), msg);
	if (messages.size() == 1)
		msg_queue_empty_cv.notify_one();
	LOG_PRINT(LOG_DEBUG, "Message added to queue.");
}

void MQTT_communicator::send_message(const std::string &message)
{
	int ret = publish(nullptr, topic.c_str(), message.size(), message.c_str(), 2, false);
	if (ret != MOSQ_ERR_SUCCESS)
		throw std::runtime_error("Error sending message: Code " + std::to_string(ret));
}

std::string MQTT_communicator::get_message()
{
	LOG_PRINT(LOG_DEBUG, "Wait for message.");
	std::unique_lock<std::mutex> lock(msg_queue_mutex);
	while (messages.empty())
		msg_queue_empty_cv.wait(lock);
	mosquitto_message *msg = messages.front();
	messages.pop();
	std::string buf(static_cast<char*>(msg->payload), msg->payloadlen);
	mosquitto_message_free(&msg);
	LOG_PRINT(LOG_DEBUG, "Message received.");
	return buf;
}
