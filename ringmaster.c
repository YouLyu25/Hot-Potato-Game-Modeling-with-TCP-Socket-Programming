#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "potato.h"

#define NAME_LEN 64

int main (int argc, char** argv) {
  int port_num;
  int num_players;
  int num_hops;
  int len;
  int socket_fd;
  int stat;
  int ack = 0;
  int i = 0;
  char name[NAME_LEN];
  struct hostent* host_info;
  struct hostent* player_info;
  struct sockaddr_in socket_info;
  struct sockaddr_in player_in;
  struct addrinfo host_socket_info;
  struct addrinfo* host_socket_info_list;
  struct potato_t potato;  

  
  srand((unsigned)time(NULL));

  // check input parameters
  if (argc != 4) {
    fprintf(stderr, "wrong input format, correct format is: <port_num> <num_players> <num_hops>\n");
    return EXIT_FAILURE;
  }
  
  // check input format
  for (i = 1; i < 4; ++i) {
    int j = 0;
    while (argv[i][j] != '\0') {
      if (argv[i][j] > '9' || argv[i][j] < '0') {
        fprintf(stderr, "wrong input format, please enter numbers only\n");
        return EXIT_FAILURE;
      }
      ++j;
    }
  }

  port_num = atoi(argv[1]);
  num_players = atoi(argv[2]);
  num_hops = atoi(argv[3]);

  // check port number
  if (port_num < 1024) {
    fprintf(stderr, "please input valid port number, which is greater or equal to 1024\n");
    return EXIT_FAILURE;
  }
  // check number of players
  if (num_players < 2 || num_players > 999) {
    fprintf(stderr, "the player number should be between 2 and 999\n");
    return EXIT_FAILURE;
  }
  // check number of hops
  if (num_hops < 0 || num_hops > 512) {
    fprintf(stderr, "hop number must be between 0 and 512\n");
    return EXIT_FAILURE;
  }
  printf("Potato Ringmaster\n");
  printf("Players = %d\n", num_players);
  printf("Hops = %d\n", num_hops);
  
  
  // get host name and address info
  gethostname(name, sizeof(name));
  host_info = gethostbyname(name);

  if (host_info == NULL) {
    fprintf(stderr, "host %s not found\n", name);
    return EXIT_FAILURE;
  }
  
  
  memset(&host_socket_info, 0, sizeof(host_socket_info));
  host_socket_info.ai_family   = AF_INET;
  host_socket_info.ai_socktype = SOCK_STREAM; // TCP
  host_socket_info.ai_flags    = AI_PASSIVE;
  host_socket_info.ai_protocol = 0;
  
  stat = getaddrinfo(name, argv[1], &host_socket_info, &host_socket_info_list);
  
  // create TCP socket with IPv4
  socket_fd = socket(host_socket_info_list->ai_family,
                     host_socket_info_list->ai_socktype,
                     host_socket_info_list->ai_protocol);
  
  if (socket_fd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }
  
  // set socket options
  int yes = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    perror("setsockopt");
    close(socket_fd);
    return EXIT_FAILURE;
  }
  
  // bind socket_fd to address socket_info
  stat = bind(socket_fd, host_socket_info_list->ai_addr, host_socket_info_list->ai_addrlen);
  if (stat < 0) {
    perror("bind");
    return EXIT_FAILURE;
  }

  // listen to port
  stat = listen(socket_fd, num_players);
  if (stat < 0) {
    perror("listen");
    return EXIT_FAILURE;
  }

  
  // store player's host name
  char player_name[num_players][NAME_LEN];
  // store player's socket file descriptor and port number
  int player[num_players][2];
  

  for (i = 0; i < num_players; ++i) {
    int player_port_num;
    len = sizeof(socket_info);
    
    // accept incoming connection
    int conn_fd = accept(socket_fd, (struct sockaddr*)&player_in, &len);
    if (conn_fd < 0 ) {
      perror("accept");
      return EXIT_FAILURE;
    }
    printf("Player %d is ready to play\n", i);
    
    // receive player's port number
    len = recv(conn_fd, &player_port_num, sizeof(player_port_num), 0);
    // save player's connection fd and port number 
    player[i][0] = conn_fd;
    player[i][1] = player_port_num;

    // send player's id, total number of players and hops to players
    len = send(player[i][0], (char*)&i, sizeof(i), 0);
    len = send(player[i][0], (char*)&num_players, sizeof(num_players), 0);
    len = send(player[i][0], (char*)&num_hops, sizeof(num_hops), 0);
    len = recv(player[i][0], &ack, sizeof(ack), 0);
   
    // get and store player's name info 
    memset(player_name[i], '\0', NAME_LEN);
    player_info = gethostbyaddr((char*)&player_in.sin_addr, sizeof(struct in_addr), AF_INET);
    strcpy(player_name[i], player_info->h_name);
  }

  // send player's next neighbor's port number and host name to the player
  for (i = 0; i < num_players; ++i) {
    // as it forms a ring, for the last player, send the first player's info
    if (i == num_players - 1) {
      len = send(player[i][0], (char*)&(player[0][1]), sizeof(player[0][1]), 0);
      len = recv(player[i][0], &ack, sizeof(ack), 0);
      len = send(player[i][0], player_name[0], NAME_LEN, 0);
    }
    else {
      len = send(player[i][0], (char*)&(player[i+1][1]), sizeof(player[i+1][1]), 0);
      len = recv(player[i][0], &ack, sizeof(ack), 0);
      len = send(player[i][0], player_name[i+1], NAME_LEN, 0);
    }
  }
 
  // send signal to players indicating the beginning of neighboring connections
  int conn_signal = 1;
  for(i = 0; i < num_players; ++i) {
    len = send(player[i][0], (char*)&(conn_signal), sizeof(conn_signal), 0);
    len = recv(player[i][0], &ack, sizeof(ack), 0);
  }
  
  // wait for a second to finish the game
  sleep(1);
  
  // if the hop number set is 0, immediately shut down the game
  if (num_hops == 0) {
    // indicate players to the game is finished
    int finish = 6666;
    for (i = 0; i < num_players; ++i) {
      len = send(player[i][0], (char*)&(finish), sizeof(finish), 0);
    }
    close(socket_fd);
    for (i = 0; i < num_players; ++i) {
      close(player[i][0]);
    }
    return EXIT_FAILURE;
  }

  // randomly choose the first player to handle the potato
  int first_player_id = ((rand()) % num_players);
  
  printf("Ready to start the game, sending potato to player %d\n", first_player_id);
  
  int signal = 1;
  len = send(player[first_player_id][0], (char*)&(signal), sizeof(signal), 0);
  len = recv(player[first_player_id][0], &ack, sizeof(ack), 0);
  
  // initialize potato parameters
  potato.rest_hops = num_hops;
  memset(potato.trace, '\0', sizeof(char)*(4*num_hops));
  strcat(potato.trace, "");
  // send empty trace as player will wait for receiving trace from neighbors
  // do that so that the player's recv will not be blocked
  char send_buff[sizeof(char)*(4*num_hops)+sizeof(int)];
  
  memcpy(send_buff, &potato, sizeof(char)*(4*num_hops)+sizeof(int));
  len = send(player[first_player_id][0], send_buff, sizeof(char)*(4*num_hops)+sizeof(int), 0);
  len = recv(player[first_player_id][0], &ack, sizeof(ack), 0);
  
  // read file descriptor set
  fd_set read_fds;
  FD_ZERO(&read_fds);
  int max_fds = player[0][0];
  
  for (i = 0; i < num_players; ++i) {
    FD_SET(player[i][0], &read_fds);
    if (player[i][0] > max_fds) {
      max_fds = player[i][0];
    }
  }
  
  
  // check if read file descriptor set has changed
  select(max_fds + 1, &read_fds, NULL, NULL, NULL);
  
  int index = 0;
  // iterate and check if each read file descriptor changes
  for (i = 0; i < num_players; ++i) {
    // this fd is set
    if (FD_ISSET(player[i][0], &read_fds)) {
      char recv_buff[sizeof(char)*(4*num_hops)+sizeof(int)];
      len = recv(player[i][0], recv_buff, sizeof(char)*(4*num_hops)+sizeof(int), 0);
      memcpy(&potato, recv_buff, sizeof(char)*(4*num_hops)+sizeof(int));
    }
  }

  printf("Trace of potato:\n%s\n", potato.trace);
  
  int finish = 6666;
  for (i = 0; i < num_players; ++i) {
    len = send(player[i][0], (char*)&(finish), sizeof(finish), 0);
  }

  for(i = 0; i < num_players; ++i) {
    close(player[i][0]);
  }
  close(socket_fd);

  return EXIT_SUCCESS;

}// end main

