#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include "potato.h"

#define NAME_LEN 64


int main (int argc, char** argv) {
  int ringmaster_port_num;
  int player_port_num;
  int neighbor_port_num;
  int num_hops;
  int num_players;
  int ringmaster_sfd;
  int player_sfd;
  int right_neighbor_sfd;
  int left_neighbor_sfd;
  int player_id;
  int stat;
  int len;
  int ack = 1;
  int i = 0;
  struct hostent* ringmaster_info;
  struct hostent* player_info;
  struct hostent* neighbor_info;
  struct sockaddr_in neighbor_in;
  socklen_t addr_len = sizeof(neighbor_in);
  char player_name[NAME_LEN];
  struct sockaddr_in player_socket_info;
  struct sockaddr_in neighbor_socket_info;
  struct addrinfo host_info;
  struct addrinfo* host_info_list;
  struct potato_t potato;
  
  
  
  // check input parameters
  if (argc != 3) {
    perror("wrong input format, correct format is: <machine_name> <port_num>\n");
    exit(1);
  }
  
  // check input format
  int j = 0;
  while (argv[2][j] != '\0') {
    if (argv[2][j] > '9' || argv[2][j] < '0') {
      fprintf(stderr, "wrong <port_num> format, please enter numbers only\n");
      return EXIT_FAILURE;
    }
    ++j;
  }
  
// CONNECT TO THE RINGMASTER ======================================================================
  ringmaster_info = gethostbyname(argv[1]);
  if (ringmaster_info == NULL) {
    fprintf(stderr, "%s: host not found (%s)\n", argv[0], argv[1]);
    exit(1);
  }

  ringmaster_port_num = atoi(argv[2]);

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  stat = getaddrinfo(argv[1], argv[2], &host_info, &host_info_list);
  
  // create TCP socket used to connect to the ringmaster
  ringmaster_sfd = socket(host_info_list->ai_family,
                          host_info_list->ai_socktype,
                          host_info_list->ai_protocol);
  if (ringmaster_sfd < 0) {
    perror("socket");
    exit(ringmaster_sfd);
  }
  
  stat = connect(ringmaster_sfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (stat < 0) {
    perror("ringmaster connect");
    exit(stat);
  }
  
  
// CREATE PLAYER'S SOCKET AND WAIT FOR CONNECTION =================================================
  gethostname(player_name, sizeof(player_name));
  player_info = gethostbyname(player_name);
  
  // create TCP socket for player itself, which will wait for incoming neighboring connection
  player_sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (player_sfd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  player_socket_info.sin_family = AF_INET;
  for (i = 51015; i <= 51097; ++i) {
    if (i == ringmaster_port_num || i == neighbor_port_num) {
      continue;
    }
    player_socket_info.sin_port = htons(i);
    memcpy(&player_socket_info.sin_addr, player_info->h_addr_list[0], player_info->h_length);
    
    stat = bind(player_sfd, (struct sockaddr *)&player_socket_info, sizeof(player_socket_info));
    if (stat == 0) {
      break;
    }
    else if (i == 51097 && stat < 0) {
      fprintf(stderr, "no usable port\n");
      return EXIT_FAILURE;
    }
  }
  
  struct sockaddr_in bind_addr_buff;
  // get socket info according to player's socket fd
  if (getsockname(player_sfd, (struct sockaddr*)&bind_addr_buff, &addr_len) == 0) {
    // get player's port number according to the socket bond
    player_port_num = ntohs(bind_addr_buff.sin_port);
  }
  else {
    perror("getsockname");
    return EXIT_FAILURE;
  }

#if 1
  // send player's port number, receive player's id, total number of players and hops
  len = send(ringmaster_sfd , (char*)&player_port_num, sizeof(player_port_num), 0);
  len = recv(ringmaster_sfd, &player_id, sizeof(player_id), 0);
  // set random seed
  srand((unsigned int)time(NULL) + player_id);
  len = recv(ringmaster_sfd, &num_players, sizeof(num_players), 0);
  printf("Connected as player %d out of %d total players\n", player_id, num_players);
  len = recv(ringmaster_sfd, &num_hops, sizeof(num_hops), 0);
  len = send(ringmaster_sfd, (char*)&ack, sizeof(ack), 0);
  
  // receive neighbor's port number and send ringmaster acknowledgement
  len = recv(ringmaster_sfd, &neighbor_port_num, sizeof(neighbor_port_num), 0);
  len = send(ringmaster_sfd, (char*)&ack, sizeof(ack), 0);
  char neighbor_name[NAME_LEN];
  memset(neighbor_name, '\0', NAME_LEN);
  len = recv(ringmaster_sfd, neighbor_name, NAME_LEN, 0);
  
  // if using the same machine
  if (strcmp(neighbor_name, "localhost") == 0) {
    neighbor_info = gethostbyname(player_name);
  }
  else {
    neighbor_info = gethostbyname(neighbor_name);
  }
  
  if (neighbor_info == NULL) {
    fprintf(stderr, "host %s not found\n", player_name);
    exit(1);
  }
#endif

  stat = listen(player_sfd, 2);
  if (stat < 0) {
    perror("listen");
    exit(stat);
  }

// CONNECT TO THE NEIGHBOR NODES ==================================================================
  right_neighbor_sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (right_neighbor_sfd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }
  
  neighbor_socket_info.sin_family = AF_INET;
  neighbor_socket_info.sin_port = htons(neighbor_port_num);
  memcpy(&neighbor_socket_info.sin_addr, neighbor_info->h_addr_list[0], neighbor_info->h_length);
  
  int conn_signal;
  len = recv(ringmaster_sfd, &conn_signal, sizeof(conn_signal), 0);
  
  stat = connect(right_neighbor_sfd, (struct sockaddr*)&neighbor_socket_info, sizeof(neighbor_socket_info));
  if (stat < 0) {
    perror("right neighbor connect");
    return EXIT_FAILURE;
  }
  
  len = send(ringmaster_sfd, (char*)&ack, sizeof(ack), 0);
  left_neighbor_sfd = accept(player_sfd, (struct sockaddr*)&neighbor_in, &addr_len);
  
  if (left_neighbor_sfd < 0) {
    perror("accept");
    return EXIT_FAILURE;
  }
  
  fd_set read_fds;
  FD_ZERO(&read_fds);
  int max_sfd = ringmaster_sfd;
  FD_SET(ringmaster_sfd, &read_fds);
  FD_SET(right_neighbor_sfd, &read_fds);

  if (right_neighbor_sfd > max_sfd) {
    max_sfd = right_neighbor_sfd;
  }
  
  FD_SET(left_neighbor_sfd, &read_fds);
  if (left_neighbor_sfd > max_sfd) {
    max_sfd = left_neighbor_sfd;
  }
  
  // add player id to previous potato trace
  char prev_trace[4] = "";
  sprintf(prev_trace, "%d", player_id);
  prev_trace[3] = '\0';
  memset(potato.trace, '\0', sizeof(char)*(4*num_hops));


  // iterate and check if any fd has changed
  while (1) {
    fd_set temp_fds = read_fds;
    int num_fd = select(max_sfd + 1, &temp_fds, NULL, NULL, NULL);
    
    int curr_fd;
    if (FD_ISSET(ringmaster_sfd, &temp_fds)) {
      curr_fd = ringmaster_sfd;
    }
    else if (FD_ISSET(left_neighbor_sfd , &temp_fds)) {
      curr_fd = left_neighbor_sfd;
    }
    else if(FD_ISSET(right_neighbor_sfd, &temp_fds)) {
      curr_fd = right_neighbor_sfd;
    }
        
    // receive signal from ringmaster or neighbors
    int signal = 0;
    len = recv(curr_fd, &signal, sizeof(signal), 0);
    
    // if received signal is 6666, the game has finished
    if (signal == 6666) {
      break;
    }
    else {
      len = send(curr_fd, (char*)&ack, sizeof(ack), 0);
//*************************************************************************************************
      // receive potato trace from neighbor (or from ringmaster for uniformity)
      char recv_buff[sizeof(char)*(4*num_hops)+sizeof(int)];
      len = recv(curr_fd, recv_buff, sizeof(char)*(4*num_hops)+sizeof(int), 0);
      memcpy(&potato, recv_buff, sizeof(char)*(4*num_hops)+sizeof(int));
      len = send(curr_fd, (char*)&ack, sizeof(ack), 0);
//*************************************************************************************************
      --potato.rest_hops;
      
      // if run out of hops
      if (potato.rest_hops == 0) {
        strcat(potato.trace, prev_trace);
        // send complete potato trace back to the ringmaster
        len = send(ringmaster_sfd, (char*)&potato, sizeof(potato), 0);
        printf("I'm it\n");
        continue;
      }
      // send potato to the next player
      else {
        int neighbor_id;
        int dest; // potato's next destination
        
        // add current info to the potato trace
        strcat(potato.trace, prev_trace);
        strcat(potato.trace, ",");
        
        // randomly choose neighbor to send potato
        int rand_neighbor = (rand()) % 2;
        
        // send potato to left neighbor
        if (rand_neighbor == 0) {
          dest = left_neighbor_sfd;
        }
        // send potato to right neighbor
        else {
          dest = right_neighbor_sfd;
        }
        
        
        if (player_id == 0) {
          if (rand_neighbor == 1) { // right neighbor
            neighbor_id = player_id + 1;
          }
          else if (rand_neighbor == 0) { // left neighbor
            neighbor_id = num_players - 1;
          }
        }
        else if(player_id == num_players - 1) {
          if (rand_neighbor == 0) { // left neighbor
            neighbor_id = player_id - 1;
          }
          else if (rand_neighbor == 1) { // right neighbor
            neighbor_id = 0;
          }
        }
        else {
          if (rand_neighbor == 1) { // right neighbor
            neighbor_id = player_id + 1;
          }
          else if(rand_neighbor == 0) { // left neighbor
            neighbor_id = player_id - 1;
          }
        }
        
        printf("Sending potato to %d \n", neighbor_id);
        // signal used to determine whether the game has finished or not
        // if it is 6666, then game has finished, else the game goes on
        signal = 66;
        len = send(dest, (char*)&signal, sizeof(signal), 0);
        len = recv(dest, &ack, sizeof(ack), 0);
//*************************************************************************************************
        len = send(dest, (char*)&potato, sizeof(char)*(4*num_hops)+sizeof(int), 0);
        len = recv(dest, &ack, sizeof(ack), 0);
//*************************************************************************************************
        continue;
      }
    }
  }
  
  close(right_neighbor_sfd);
  close(player_sfd);
  close(ringmaster_sfd);
  
  return EXIT_SUCCESS;
}
