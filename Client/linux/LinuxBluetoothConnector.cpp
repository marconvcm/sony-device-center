#include "LinuxBluetoothConnector.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <cerrno>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include "DBusHelper.h"

LinuxBluetoothConnector::LinuxBluetoothConnector()
{
}
LinuxBluetoothConnector::~LinuxBluetoothConnector()
{
  disconnect();
}

int LinuxBluetoothConnector::recv(char *buf, size_t length)
{
  ssize_t bytesRead;
  do {
    bytesRead = ::read(this->_socket, buf, length);
  } while (bytesRead < 0 && errno == EINTR);
  if (bytesRead < 0)
  {
    // A recv timeout (SO_RCVTIMEO) is expected while probing optional features: report it as recoverable
    // WITHOUT forcing a disconnect so an unanswered GET just hides that feature instead of dropping the link.
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      throw RecoverableException("recv timed out", false);
    }
    throw RecoverableException(std::string("Couldn't recv: ") + strerror(errno), true);
  }
  return (int)bytesRead;
}

int LinuxBluetoothConnector::send(char *buf, size_t length)
{
  ssize_t written;
  do {
    // A closed control channel must never terminate the app with SIGPIPE.
    written = ::send(this->_socket, buf, length, MSG_NOSIGNAL);
  } while (written < 0 && errno == EINTR);
  if (written < 0) {
    throw RecoverableException(std::string("Couldn't send: ") + strerror(errno), true);
  }
  return static_cast<int>(written);
}

void LinuxBluetoothConnector::connect(const std::string &addrStr)
{
  disconnect();
  struct sockaddr_rc addr{};
  if (str2ba(addrStr.c_str(), &addr.rc_bdaddr) != 0) {
    throw RecoverableException("Invalid Bluetooth address", true);
  }
  int status;

  // allocate a socket
  this->_socket = ::socket(AF_BLUETOOTH, SOCK_STREAM | SOCK_CLOEXEC, BTPROTO_RFCOMM);
  if (this->_socket < 0) {
    throw RecoverableException("Couldn't create Bluetooth socket", true);
  }
  try {

    uint32_t linkmode = RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT;
    int sl = sizeof(linkmode);
    if (setsockopt(this->_socket, SOL_RFCOMM, RFCOMM_LM, &linkmode, sl) < 0) {
      throw RecoverableException("Couldn't secure Bluetooth socket", true);
    }

    // Bound recv so capability probing can't block forever (matches the 2.5s macOS timeout).
    struct timeval recvTimeout = {2, 500000};
    if (setsockopt(this->_socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout)) < 0 ||
        setsockopt(this->_socket, SOL_SOCKET, SO_SNDTIMEO, &recvTimeout, sizeof(recvTimeout)) < 0) {
      throw RecoverableException("Couldn't set Bluetooth I/O deadlines", true);
    }

    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;
    // try the v1 service UUID first, then fall back to the v2 (newer-generation) UUID.
    uint8_t channel = sdp_getServiceChannel(addrStr.c_str(), SERVICE_UUID_IN_BYTES);
    this->_protocolVersion = SonyProtocolVersion::V1;
    if (channel == 0)
    {
      channel = sdp_getServiceChannel(addrStr.c_str(), SERVICE_UUID_V2_IN_BYTES);
      this->_protocolVersion = SonyProtocolVersion::V2;
    }
    if (channel == 0)
    {
      throw RecoverableException("Couldn't find the Sony service record on this device (neither protocol version) - is this a supported headset?", true);
    }
    addr.rc_channel = channel;

    // connect to server
    status = ::connect(this->_socket, (struct sockaddr *)&addr, sizeof(addr));

    if (status < 0)
    {
      throw RecoverableException("Error: could not connect to bluetooth socket", true);
    }
    this->_connected = true;
  } catch (...) {
    disconnect();
    throw;
  }
}

std::vector<BluetoothDevice> LinuxBluetoothConnector::getConnectedDevices()
{

  std::vector<BluetoothDevice> res;

  DBusConnection *connection = dbus_open_system_bus();
  if (NULL == connection)
  {
    return res;
  }

  std::vector<std::string> adapter_paths = dbus_list_adapters(connection);
  for (auto &adapter : adapter_paths)
  {
    std::string name = dbus_get_property(connection, adapter.c_str(), "Name");
    std::string address = dbus_get_property(connection, adapter.c_str(), "Address");
    bool paired = dbus_get_property_bool(connection, adapter.c_str(), "Paired");
    if (!paired) continue;
    bool connected = dbus_get_property_bool(connection, adapter.c_str(), "Connected");
    if (connected)
    {
      res.insert(res.begin(), {.name = name, .mac = address, .paired = paired, .connected = connected});
    }
    else
    {
      res.push_back({.name = name, .mac = address, .paired = paired, .connected = connected});
    }
  }

  return res;
}

void LinuxBluetoothConnector::disconnect() noexcept
{
  // close connection
  if (this->_socket != -1)
  {
    ::close(this->_socket);
    this->_socket = -1;
  }
  this->_connected = false;
}

bool LinuxBluetoothConnector::isConnected() noexcept
{
  return this->_connected;
}

SonyProtocolVersion LinuxBluetoothConnector::getProtocolVersion() noexcept
{
  return this->_protocolVersion;
}
