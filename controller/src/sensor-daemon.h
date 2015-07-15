/*
 * sensor-daemon.h
 *
 * daemon to read sensor values and send them
 * via protobuf messages
 *
 * Konstantin Koslowski <konstantin.koslowski@mailbox.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "messages.pb.h"
#include "nes-socket.h"
#include "nes.h"
#include "sys/time.h"

#ifdef HOST_BBB
#include "LSM303.h"
#include "I2CBus.h"
#endif

typedef struct {
  int x;
  int y;
  int z;
  int x_offset;
  int y_offset;
  int z_offset;
} sensor;
sensor sensor1, sensor2;

void socket_read_sensorcommand (int sockfd);
void socket_write_sensordata (int sockfd, messages::sensordata *data);
