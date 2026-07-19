#pragma once

#include "Common.h"
#include "Protocol.h"
#include <queue>

namespace EspNowRcLink {

class Transmitter {
public:
  enum State {
    DISCOVERING,
    TRANSMITTING,
  };

  Transmitter();
  int begin(bool enSoftAp = false);
  int update();
  void setChannel(size_t c, unsigned int value);
  void commit();
  int getSensor(size_t id) const;

private:
  void _handleDiscovery();
  void _handleTransmit();
  void _handleReceived();

  bool _allowed(const uint8_t *mac) const;
  static void _handleRx(const uint8_t *mac, const uint8_t *buf, size_t count, void *arg);
  static const uint8_t BCAST_PEER[WIFIESPNOW_ALEN];

  MessageRc _channels;
  MessageFc _sensors;
  uint8_t _peer[WIFIESPNOW_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t _channel;
  uint32_t _next_discovery = 0;
  State _state = DISCOVERING;
  std::queue<Message> _queue;
  bool _ready = false;
};

}
