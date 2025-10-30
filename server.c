/* 
 * Program:	Basic HTTP Web Server
 * Author: 	D Pascoe
 * Date: 	30th Oct 2025
 * License: 	MIT
*/

#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define BACKLOG 26
#define RESPONSE_BUF 1024
#define REQUEST_BUF 1024
#define MAX_EVENTS 10

int setup_server_socket(char *PORT);
int setup_epoll(int server_socket);
int setnonblocking(int client_socket);
int do_use_fd(int client_socket, int epollfd);
struct addrinfo *setup_addrinfo(char *PORT);

int main(int argc, char *argv[])
{
	char *PORT;

	if (argc != 2) {
		printf("Usage: %s [PORT NUMBER]\n", argv[0]);
		exit(1);
	}
	else {
		PORT = argv[1];
	}

	int server_socket = setup_server_socket(PORT);
	setup_epoll(server_socket); /* runs forever */
	
	return 0;
}

int setup_server_socket(char *PORT)
{
	int server_socket;
	int socket_opt = 1;

	char dst_buf[INET_ADDRSTRLEN];
	
	const char *sockname;
	
	struct addrinfo *rp, *result; 
	struct sockaddr_storage server_address;
	
	socklen_t server_address_len = sizeof(server_address);

	result = setup_addrinfo(PORT);	
	if (result == NULL) {
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {

		server_socket = socket(rp->ai_family, rp->ai_socktype, 
				rp->ai_protocol);
		if (server_socket == -1) {
			perror("socket");
			continue;
		}

		if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
					&socket_opt, sizeof(socket_opt)) == -1) {
			perror("setsockopt");
		}

		if (bind(server_socket, rp->ai_addr, rp->ai_addrlen) == 0) {
			break; 
		}
			perror("bind");

		close(server_socket);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind to an address\n");
		exit(EXIT_FAILURE);
	}

	if (listen(server_socket, BACKLOG) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	if (getsockname(server_socket, (struct sockaddr *)&server_address, &server_address_len) == -1) {
		perror("getsockname");
	}
	
	sockname = inet_ntop(AF_INET, &((struct sockaddr_in *)&server_address)->sin_addr, dst_buf, sizeof(dst_buf));
	if (sockname == NULL) {
		perror("inet_ntop");
	}

	printf("Server starting...\n");
	printf("Listening on address: %s, PORT: %s\n\n", sockname, PORT);

	return server_socket;
}

int setup_epoll(int server_socket)
{
	int n, nfds, epollfd, client_socket;
	struct epoll_event ev, events[MAX_EVENTS];
	
	ev.events = EPOLLIN;
	ev.data.fd = server_socket;
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
	   perror("epoll_ctl: server_socket");
	   exit(EXIT_FAILURE);
	}
	
	for (;;) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		for (n = 0; n < nfds; ++n) {
			if (events[n].data.fd == server_socket) {
				client_socket = accept(server_socket, NULL, NULL);
				if (client_socket == -1) {
					perror("accept");
					exit(EXIT_FAILURE);
				}

				setnonblocking(client_socket);
				
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = client_socket;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, 
					client_socket, &ev) == -1) {
					perror("epoll_ctl: client_socket");
					exit(EXIT_FAILURE);
				}
			} else {
				do_use_fd(events[n].data.fd, epollfd);
			}
		}
	}
}

struct addrinfo *setup_addrinfo(char *PORT)
{
	int s;
	
	struct addrinfo hints, *result;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, PORT, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return NULL;
	}

	return result;
}

int setnonblocking(int client_socket)
{
	int fd_flag = fcntl(client_socket, F_GETFL);
	if (fd_flag == -1) {
		perror("fcntl F_GETFL");
	}
	return fcntl(client_socket, F_SETFL, fd_flag | O_NONBLOCK);
}

int do_use_fd(int client_socket, int epollfd)
{
	int fd;

	char dst_buf[INET_ADDRSTRLEN];
	char html_file_buffer[1024];
	char client_request[REQUEST_BUF];
	char *uri_find, *uri_result, *uri_start = "/";
	char filepath[1024];
	char pub_buf[] = "public_html";
	char header_200[RESPONSE_BUF] = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"\r\n";
	char header_404[RESPONSE_BUF] = 
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/html\r\n"
		"\r\n";
	
	const char *sockname;
	
	ssize_t bytes_read;
	ssize_t bytes_received;
	
	struct sockaddr_storage client_address;
	
	socklen_t client_address_len = sizeof(client_address);
	
	bytes_received = recv(client_socket, client_request, 
			sizeof(client_request), 0);
	if (bytes_received == 0) {
		epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
		close(client_socket);
		return 0;
	}

	if (bytes_received == -1 && errno != EAGAIN) {
		perror("recv");
		epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
		close(client_socket);
		return -1;
	}

	if (bytes_received == -1 && errno == EAGAIN) {
		return 0;
	}

	client_request[bytes_received] = '\0';
	
	uri_find = strstr(client_request, uri_start);
	if (uri_find == NULL) {
		perror("strstr, NULL");
		epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
		close(client_socket);
		return -1;
	}

	uri_result = strtok(uri_find, " ");
	snprintf(filepath, sizeof(filepath), "%s%s", pub_buf, uri_result);

	if (strstr(filepath, "..") != NULL) {
		printf("Malicious path requested\n");
	}

	fd = open(filepath, O_RDONLY, 0);
	if (fd == -1) {
		perror("open");
	}
	
	if (getpeername(client_socket, (struct sockaddr *)&client_address, &client_address_len) == -1) {
		perror("getsockname");
	}
	
	sockname = inet_ntop(AF_INET, &((struct sockaddr_in *)&client_address)->sin_addr, dst_buf, sizeof(dst_buf));
	if (sockname == NULL) {
		perror("inet_ntop");
	}

	printf("Connection from: %s, uri: %s\n\n", sockname, filepath);

	if (fd != -1) {
		if (send(client_socket, header_200, strlen(header_200), 0) == -1) {
			perror("send header");
			close(fd);
			close(client_socket);
		}
		while ((bytes_read = read(
			fd, html_file_buffer, sizeof(html_file_buffer))) > 0) {
			if (send(client_socket, 
				html_file_buffer, bytes_read, 0) == -1) {
				perror("send body");
				break;
			}
		}
		if (bytes_read == -1) {
			perror("read");
			epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
			close(client_socket);
			return -1;
		}

		close(fd);
	}

	if (fd == -1) {
		if (send(client_socket, header_404, strlen(header_404), 0) == -1) {
			perror("send");
			epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
			close(client_socket);
			return -1;
		}
	}

	epoll_ctl(epollfd, EPOLL_CTL_DEL, client_socket, NULL);
	close(client_socket);
	return 0;
}
