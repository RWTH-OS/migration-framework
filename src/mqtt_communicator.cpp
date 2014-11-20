#include "mqtt_communicator.hpp"

#include "logging.hpp"

#include <stdexcept>

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
	mosqpp::lib_init();
	connect_async(host.c_str(), port, keepalive);
	loop_start();
}

MQTT_communicator::~MQTT_communicator()
{
	disconnect();
	loop_stop();
	mosqpp::lib_cleanup();
}

void MQTT_communicator::on_connect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Connection established to " + host + ":" + std::to_string(port)).c_str());
	} else {
		throw std::runtime_error("Error on connect: Code " + std::to_string(rc));
	}
}

void MQTT_communicator::on_disconnect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Disconnected from  " + host + ":" + std::to_string(port)).c_str());
	} else {
		LOG_PRINT(LOG_NOTICE, ("Unexpected disconnect: Code " + std::to_string(rc)).c_str());
	}
}

void MQTT_communicator::on_publish(int mid)
{
	LOG_PRINT(LOG_NOTICE, ("Message " + std::to_string(mid) + " published.").c_str());
}

void MQTT_communicator::send_message(const std::string &message)
{
	int ret = publish(nullptr, topic.c_str(), message.size(), message.c_str(), 2, false);
	if (ret != MOSQ_ERR_SUCCESS)
		throw std::runtime_error("Error sending message: Code " + std::to_string(ret));
}
