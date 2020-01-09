#include <stdio.h>
#include <iostream>
#include <pv/packet.h>
#include <pv/driver.h>

#include <pv/ethernet.h>
#include <pv/ipv4.h>
#include <pv/icmp.h>

namespace pv {

#define MAX_PACKETLETS	8

uint32_t packetlet_count;
class Packetlet* packetlets[MAX_PACKETLETS];

static void received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	Packet* packet = new Packet(queueId, payload, start, end, size);

	for(uint32_t i = 0; i < packetlet_count; i++) {
		try {
			if(packetlets[i] != nullptr && packetlets[i]->received(packet))
				return;
		} catch(const std::exception& e) {
			fprintf(stderr, "Exception occurred: Packetlet[%u] - %s\n", i, e.what());
			printf("Exception occurred: Packetlet[%u] - %s\n", i, e.what());
		} catch(...) {
			fprintf(stderr, "Unexpected exception occurred: Packetlet[%u]\n", i);
			printf("Unexpected exception occurred: Packetlet[%u]\n", i);
		}
	}

	delete packet;
}

static struct pv_Callback* callback;

static struct pv_Driver driver = {
	.received = pv::received
};

extern "C" {

class MyPacketlet: public Packetlet {
public:
	MyPacketlet();
	bool received(Packet* packet);
};

MyPacketlet::MyPacketlet() : Packetlet() {
}

bool MyPacketlet::received(Packet* packet) {
	try {
		printf("received in packetlet\n");
		std::cout << packet << std::endl;

		Ethernet* eth = new Ethernet(packet);
		std::cout << "type: " << eth->getType() << std::endl;
		std::cout << eth << std::endl;

		IPv4* ipv4 = new IPv4(packet, eth->getPayloadOffset());
		std::cout << ipv4 << std::endl;

		ICMP* icmp = new ICMP(packet, ipv4->getBodyOffset());
		std::cout << icmp << std::endl;

		delete icmp;
		delete ipv4;
		delete eth;
	} catch(const std::exception& e) {
		fprintf(stderr, "Exception occurred in class: Packetlet[%u] - %s\n", id, e.what());
	}

	return false;
}

struct pv_Driver* pv_init(struct pv_Callback* cb) {
	callback = cb;

	new MyPacketlet();

	return &driver;
}

} // extern "C"

int32_t pv_driver_add_packetlet(void* packetlet) {
	if(packetlet_count < MAX_PACKETLETS) {
		packetlets[packetlet_count++] = (Packetlet*)packetlet;
		return packetlet_count - 1;
	}

	return -1;
}

bool pv_driver_remove_packetlet(void* packetlet) {
	for(uint32_t i = 0; i < packetlet_count; i++) {
		if(packetlets[i] != nullptr) {
			if((void*)packetlets[i] == packetlet) {
				packetlets[i] = nullptr;
				return true;
			}
		}
	}

	return false;
}

uint8_t* pv_driver_alloc(uint32_t size) {
	if(callback == nullptr) {
		fprintf(stderr, "Packetvisor driver is not inited yet\n");
		return nullptr;
	}

	return callback->alloc(size);
}

void pv_driver_free(uint8_t* payload) {
	if(callback == nullptr) {
		fprintf(stderr, "Packetvisor driver is not inited yet\n");
		return;
	}

	return callback->free(payload);
}

bool pv_driver_send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	if(callback == nullptr) {
		fprintf(stderr, "Packetvisor driver is not inited yet\n");
		return false;
	}

	return callback->send(queueId, payload, start, end, size);
}

}; // namespace pv