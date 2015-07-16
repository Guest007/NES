/*
 * sensor-daemon.cpp
 *
 * daemon to read sensor values and send them
 * via protobuf messages
 *
 * Konstantin Koslowski <konstantin.koslowski@mailbox.org>
 */

// #include <stdio.h>
// #include <iostream>
// #include <unistd.h>
// #include <vector>
#include "sensor-daemon.h"

#ifdef HOST_BBB
LSM303 mag("/dev/i2c-1");
#endif

/*
 * handle sensorcommand
 * return sensorstatus
 */
void handle_sensorcommand (messages::sensorcommand *command, messages::sensordata *data) {
  int s;
  // select sensor
  s = command->sensor();
  data->set_sensor(s);

  if (command->type() == messages::sensorcommand::GET) {
    // sending values anyway
  }

  // TODO
  else if (command->type() == messages::sensorcommand::CALIBRATE) {

    // select sensor
    if (s == 1) {
      // read current values
      mag.readMag();
      sensor1.x_offset = mag.m[0];
      sensor1.y_offset = mag.m[1];
      sensor1.z_offset = mag.m[2];
    }
    else if (s == 2) {
      printf("TODO\n");
      sensor2.x = 0;
      sensor2.y = 0;
      sensor2.z = 0;
    }
  }

  // create response
  if (s == 1) {
    data->set_x(sensor1.x - sensor1.x_offset);
    data->set_y(sensor1.y - sensor1.y_offset);
    data->set_z(sensor1.z - sensor1.z_offset);
  }
  else if (s == 2) {
    data->set_x(sensor2.x - sensor2.x_offset);
    data->set_y(sensor2.y - sensor2.y_offset);
    data->set_z(sensor2.z - sensor2.z_offset);
  }

}

void socket_read_sensorcommand (int sockfd) {
  int new_sockfd;
  // fds to monitor
  fd_set read_fds,write_fds;
  // timeout {[sec], [usec]}
  struct timeval waitd = {0, 100};
  struct timeval tv_now, tv_last;
  // sensor update interval [usec]
  int t_timeout = 100000;
  long int t_diff;
  int sel;
  int max_fd;
  int n;
  std::vector<int> connected;
  char buffer[BUFFERSIZE];
  // store the connecting address and size
  struct sockaddr_storage their_addr;
  socklen_t their_addr_size;

  // initialize time
  gettimeofday(&tv_last, NULL);
  while (1) {
    // read sensor data
    gettimeofday(&tv_now, NULL);
    t_diff = (tv_now.tv_usec - tv_last.tv_usec) + (tv_now.tv_sec - tv_last.tv_sec) * 1000000;
    if (t_diff > t_timeout) {
#ifdef HOST_BBB
      mag.readMag();
      sensor1.x = mag.m[0];
      sensor1.y = mag.m[1];
      sensor1.z = mag.m[2];
      // printf("mag.x: %d, mag.y: %d, mag.z: %d\n", mag.m[0], mag.m[1], mag.m[2]);
      // TODO
      sensor2.x++;
      sensor2.y++;
      sensor2.z++;
#else
      sensor1.x++;
      sensor1.y++;
      sensor1.z++;
      sensor2.x++;
      sensor2.y++;
      sensor2.z++;
#endif
      gettimeofday(&tv_last, NULL);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    // add sockfd
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd, &write_fds);
    max_fd = sockfd;

    // add connected clients
    for (std::vector<int>::iterator it = connected.begin(); it != connected.end(); it++) {
      FD_SET(*it, &read_fds);
      max_fd = std::max(max_fd, *it);
    }

    // check for data
    sel = select(max_fd+1, &read_fds, &write_fds, (fd_set*)0, &waitd);

    // continue on error
    if (sel < 0) {
      print_error("select error");
      continue;
    }

    // data received
    if (sel > 0) {
      // client connected
      if(FD_ISSET(sockfd, &read_fds)) {
        // printf("data on sockfd\n");
        their_addr_size = sizeof(their_addr);
        new_sockfd = accept(sockfd, (struct sockaddr*)&their_addr, &their_addr_size);
        if( new_sockfd < 0) {
            print_error("accept error");
        }
        socket_setnonblock(new_sockfd);
        connected.push_back(new_sockfd);
      }
      for (std::vector<int>::iterator it = connected.begin(); it != connected.end(); it++) {
        if (FD_ISSET(*it, &read_fds)) {
          // skip this fd
          if (*it == sockfd) continue;
          bzero(buffer, BUFFERSIZE);
          n = read(*it, buffer, sizeof(buffer));
          // data available
          if (n > 0) {
            // message, response
            messages::sensorcommand *message = new messages::sensorcommand();
            messages::sensordata *response = new messages::sensordata();

            // parse message
            message->ParseFromString(buffer);
            print_sensorcommand(NET_IN, message);
            // generate response
            handle_sensorcommand(message, response);
            print_sensordata(NET_OUT, response);
            socket_write_sensordata(*it, response);
          }
          // client disconnected
          else if (n == 0) {
            // printf("closing socket: %d\n", *it);
            // shutdown, close socket
            shutdown(*it, SHUT_RDWR);
            close(*it);
            // erase from list
            connected.erase(it);
            // iterator invalid, end for-loop
            break;
          }
        }
      } // for
    } // if (sel > 0)
  } // while (1)
}

/*
 * write reply to socket
 */
void socket_write_sensordata (int sockfd, messages::sensordata *data) {
  char buffer[BUFFERSIZE];
  bzero(buffer, BUFFERSIZE);

  // serialize data
  data->SerializeToArray(buffer, data->ByteSize());
  // send data
  write(sockfd, buffer, data->ByteSize());
}

int main(void) {
  int sockfd;
  sensor1 = (sensor) { 0, 0, 0, 0, 0, 0};
  sensor2 = (sensor) { 0, 0, 0, 0, 0, 0};

#ifdef HOST_BBB
  // initialize sensors
  mag.enable();
#endif

  // initialize socket
  sockfd = socket_open(SENSOR_PORT);

  // main loop
  while (1) {
    socket_read_sensorcommand(sockfd);
  }

  return(0);
}
