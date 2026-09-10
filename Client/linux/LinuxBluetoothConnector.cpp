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
  //onclose event
  if (isConnected())
  {
    disconnect();
  }
}

int LinuxBluetoothConnector::recv(char *buf, size_t length)
{
  ssize_t bytesRead = ::read(this->_socket, buf, length);
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

  size_t written = ::write(this->_socket, buf, length);
  // printf("length: %ld, written: %ld\n", length, written);
  // for (int i = 0; i < written; i++)
  // {
  //   std::cout << std::setfill('0') << std::setw(2) << std::hex << (0xff & (unsigned int)buf[i]) << " ";
  // }
  // std::cout << '\n';
  return written;
}

void LinuxBluetoothConnector::connect(const std::string &addrStr)
{
  struct sockaddr_rc addr = {0};
  int status;
  const char *dest = addrStr.c_str();

  // allocate a socket
  this->_socket = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

  uint32_t linkmode = RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT;
  int sl = sizeof(linkmode);
  setsockopt(this->_socket, SOL_RFCOMM, RFCOMM_LM, &linkmode, sl);

  // Bound recv so capability probing can't block forever (matches the 2.5s macOS timeout).
  struct timeval recvTimeout = {2, 500000};
  setsockopt(this->_socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));

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
  str2ba(dest, &addr.rc_bdaddr);

  // connect to server
  status = ::connect(this->_socket, (struct sockaddr *)&addr, sizeof(addr));

  if (status < 0)
  {
    throw RecoverableException("Error: could not connect to bluetooth socket", true);
  }
  this->_connected = true;
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
    bool connected = dbus_get_property_bool(connection, adapter.c_str(), "Connected");
    if (connected)
    {
      res.insert(res.begin(), {.name = name, .mac = address});
    }
    else
    {
      res.push_back({.name = name, .mac = address});
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
