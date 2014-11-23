#include "mqtt_communicator.hpp"

#include "logging.hpp"

#include <stdexcept>
#include <cstdlib>

MQTT_communicator::MQTT_communicator(const std::string &id, 
				     const std::string &topic,
				     const std::string &host, 
				     int port) :
	mosqpp::mosquittopp(id.c_str()),
	id(id), 
	topic(topic),
	host(host), 
	port(port),
	keepalive(60)
{
	LOG_PRINT(LOG_NOTICE, "Initializing MQTT_communicator...");
	empty_mutex.lock();
	mosqpp::lib_init();
	connect_async(host.c_str(), port, keepalive);
	loop_start();
	/// TODO: Move subscribe to a dedicated public method.
	subscribe(nullptr, "topic1", 2);
	LOG_PRINT(LOG_NOTICE, "MQTT_communicator initialized.");
}

MQTT_communicator::~MQTT_communicator()
{
	LOG_PRINT(LOG_NOTICE, "Closing MQTT_communicator...");
	disconnect();
	loop_stop();
	mosqpp::lib_cleanup();
	LOG_PRINT(LOG_NOTICE, "MQTT_communicator closed.");
}

void MQTT_communicator::on_connect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Connection established to " + host + ":" + std::to_string(port)).c_str());
	} else {
		LOG_PRINT(LOG_ERR, ("Error on connect: Code " + std::to_string(rc)).c_str());
	}
}

void MQTT_communicator::on_disconnect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Disconnected from  " + host + ":" + std::to_string(port)).c_str());
	} else {
		LOG_PRINT(LOG_ERR, ("Unexpected disconnect: Code " + std::to_string(rc)).c_str());
	}
}

void MQTT_communicator::on_publish(int mid)
{
	LOG_PRINT(LOG_NOTICE, ("Message " + std::to_string(mid) + " published.").c_str());
}

void MQTT_communicator::on_message(const mosquitto_message *msg)
{
	LOG_PRINT(LOG_NOTICE, "on_message executed.");
	std::lock_guard<std::mutex> lock(msg_queue_mutex);
	if (messages.empty())
		empty_mutex.unlock();
	mosquitto_message* buf = static_cast<mosquitto_message*>(malloc(sizeof(mosquitto_message)));
	if (!buf) 
		LOG_PRINT(LOG_ERR, "malloc failed allocating mosquitto_message.");
	messages.push(buf);
	mosquitto_message_copy(messages.back(), msg);
	LOG_PRINT(LOG_NOTICE, "Message added to queue.");
}

void MQTT_communicator::send_message(const std::string &message)
{
	int ret = publish(nullptr, topic.c_str(), message.size(), message.c_str(), 2, false);
	if (ret != MOSQ_ERR_SUCCESS)
		throw std::runtime_error("Error sending message: Code " + std::to_string(ret));
}

std::string MQTT_communicator::get_message()
{
	LOG_PRINT(LOG_NOTICE, "Wait for message.");
	mosquitto_message *msg;
	empty_mutex.lock();
	{
		std::lock_guard<std::mutex> lock(msg_queue_mutex);
		msg = messages.front();
		messages.pop();
		if(!messages.empty())
			empty_mutex.unlock();
	}
	std::string buf(static_cast<char*>(msg->payload), msg->payloadlen);
	mosquitto_message_free(&msg);
	LOG_PRINT(LOG_NOTICE, "Message received.");
	return buf;
}
