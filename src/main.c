#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

#define LISTEN_BACKLOG 50

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

enum code : char {
	OK = 0x1,
	WRITE = 0x2,
	CLEAR = 0x3,
	ERROR = 0x4,
	PING = 0x5,
};

struct message_header {
	char message_type;
	char reserved[3];
	uint32_t content_length;
};

struct message {
	enum code message_type;
	uint32_t content_length;
	char* body;
};

/* Open a named Unix socket and returns a file descriptor */
int open_socket(char* path){
	int success;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd == -1) { handle_error("socket"); }

	struct sockaddr_un namedsocket;
	memset(&namedsocket, 0, sizeof(namedsocket));
	namedsocket.sun_family = AF_UNIX;
	strncpy(namedsocket.sun_path, path, strlen(path));

	success = bind(fd, (struct sockaddr*) &namedsocket, sizeof(struct sockaddr_un));
	if(success == -1) handle_error("bind");
	
	success = listen(fd, LISTEN_BACKLOG);
	if(success == -1) handle_error("listen");

	return fd;
}

/* Closes and destroy the socket. */
void close_socket(int sockfd, char* path){
	if(close(sockfd) == -1) handle_error("close");
	if(unlink(path) == -1) handle_error("unlink");
}

/* Listens for the next connection on a socket */
int accept_client(int sockfd){
	struct sockaddr_un peer_addr;
	socklen_t peer_addr_len = sizeof peer_addr;
	printf("Waiting for client...\n");
	int clientfd = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);
	if(clientfd == -1) handle_error("accept");

	return clientfd;
}

/* Processes a message. Returns true if an answer should be sent. */
bool answer_message(
	const struct message* request,
	struct message* answer,
	int* filefd, const char* filepath
){
	printf("Received message, code: %d, content length: %d\n", request->message_type, request->content_length);
	answer->content_length = 0;
	answer->body = NULL;
	if(request->content_length != 0){
		if(! (request->message_type == WRITE || request->message_type == ERROR)){
			const char format[] = "Message of type %02x must not have body";
			// by chance, '%02x' has exactly the same size than '0x0.' with which
			// it will be replaced
			answer->message_type = ERROR;
			answer->content_length = sizeof format;
			answer->body = malloc(sizeof format);
			sprintf(answer->body, format, request->message_type);
			return true;
		}
	}

	switch (request->message_type) {
	case OK:
		return false;
	case ERROR:
		printf("Warning: Client sent an error: ");
		if(request->content_length != 0){
			printf("%s", request->body);
		}else{
			printf("(no body)\n");
		}
		return false;
	case WRITE:
		int bytes_written = write(*filefd, request->body, request->content_length);
		if(bytes_written != request->content_length){
			const char format[] = "Could not write";
			answer->message_type = ERROR;
			answer->body = malloc(sizeof format);
			memcpy(answer->body, format, sizeof format);
		} else {
			answer->message_type = OK;
		}
		return true;
	case CLEAR:
		close(*filefd);
		*filefd = open(filepath, O_CREAT|O_TRUNC|O_WRONLY);
		if(*filefd == -1){
			answer->message_type = ERROR;
			const char format[] = "Couldn't clear file";
			answer->body = malloc(sizeof format);
			memcpy(answer->body, format, sizeof format);
		} else {
			answer->message_type = OK;
		}
		return true;
	case PING:
		answer->message_type = OK;
		return true;
	default:
		answer->message_type = ERROR;
		const char format[] = "Unrecognized message type %02x";
		// luckily, "%02x" is exactly as long as the "0x0." by which it will be replaced
		answer->content_length = sizeof format;
		answer->body = malloc(sizeof format);
		sprintf(answer->body, format, request->message_type);
		return true;
	}
}

/* Listens to messages from the client and answers to them.
 * Returns when the client exits. */
void handle_client(int clientfd, int* filefd, const char* filepath){
	struct message_header header_buffer;
	struct message message, answer;
	while(true) {
		int len = recv(clientfd, (char*)&header_buffer, sizeof header_buffer, 0);
		if(len == 0) return;
		if(len < sizeof header_buffer){
			printf("Error: got incomplete message header");
			return;
		}
		message.message_type = header_buffer.message_type;
		message.content_length = ntohl(header_buffer.content_length); 
		message.body = (char*) malloc(message.content_length);

		len = recv(clientfd, message.body, message.content_length, 0);
		if(len < message.content_length){
			printf("Error: got incomplete message body");
			return;
		}

		bool must_answer = answer_message(&message, &answer, filefd, filepath);
		if(must_answer){
			printf("Answering message with code %d\n", answer.message_type);
			char* buffer = malloc(sizeof header_buffer + answer.content_length);
			struct message_header* header = (struct message_header*) buffer;
			char* body = buffer + sizeof header;

			header->message_type = answer.message_type;
			memset(&(header->reserved), 0, sizeof header->reserved);
			header->content_length = answer.content_length;
			if(answer.content_length != 0){
				memcpy(body, answer.body, answer.content_length);
			}
			int bytes_sent = send(clientfd, buffer, sizeof header_buffer + answer.content_length, 0);
			if(bytes_sent == -1) handle_error("send");
		}
	}
}

int main(int argc, char** argv){
	if(argc != 3){
		printf("Error: Wrong number of arguments\n");
		printf("Usage: %s [socket] [file]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char* socketpath = argv[1];
	int sockfd = open_socket(socketpath);
	printf("Opened socket\n");

	char* filename = argv[2];
	int filefd = open(filename, O_APPEND|O_CREAT|O_WRONLY);
	printf("Opened file\n");

	int clientfd = accept_client(sockfd);
	printf("Connected with client\n");

	handle_client(clientfd, &filefd, filename);
	printf("Ended connection\n");

	close(filefd);
	close_socket(sockfd, socketpath);
	return EXIT_SUCCESS;
}
