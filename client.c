#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



//function prototypes

int bitbot_op(int socket, char *buf, ssize_t bytes);

//----------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
        if (argc != 4)
        {
                fprintf(stderr, "Usage: %s <ID> <port> <ip_address>\n", argv[0]); //make specific
                exit(1);
        }

        char *id =              argv[1];
        char *port_num =        argv[2];
        char *ip_address =      argv[3];

        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd < 0)
        {
                perror("ERROR: Dead stub");
                exit(1);
        }

        struct sockaddr_in server;

        memset(&server, 0, sizeof(server)); //garbage removal

        server.sin_family = AF_INET;

        server.sin_port = htons(atoi(port_num)); //str to int to network B order

        if (inet_pton(AF_INET, ip_address, &server.sin_addr) <= 0)
        {
            perror("ERROR: inet_pton cannot be 0 or negative");
            close(client_fd);
            exit(1);
        }

        if (connect(client_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
                perror("ERROR: Server size cannot be negative");
                close(client_fd);
                exit(1);
        }

        char buf[128]; //where all

        // HELLO
        int n = snprintf(buf, sizeof(buf), "cs230 HELLO %s\n", id);
        if (n < 0 || n >= (int)sizeof(buf))
        {
                fprintf(stderr, "ERROR: HELLO too long\n");
                close(client_fd);
                exit(1);
        }

        ssize_t status_bytes = send(client_fd, buf, (size_t)n, 0);
        if (status_bytes < 0)
        {
                perror("ERROR: send HELLO");
                close(client_fd);
                exit(1);
        }

        status_bytes = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (status_bytes < 0)
        {
                perror("ERROR: recv");
                close(client_fd);
                exit(1);
        }
        if (status_bytes == 0)
        {
                fprintf(stderr, "Connection closed by server\n");
                close(client_fd);
                exit(1);
        }
        buf[status_bytes] = '\0';

        //BIT BOT:
        bitbot_op(client_fd, buf, status_bytes);

        close(client_fd);
        return 0;
}

//----------------------------------------------------------------------------------------------------------------------

//note: memset(buf, 0, sizeof(buf)) = wipe all buf contents by setting all 64B to 0.
int bitbot_op(int socket, char *buf, ssize_t bytes)
{       //part 1:
        //contents: "cs230 STATUS <OPERATION> <NUMBER>\n" , usually dont receive \n

        while (1)
        {
                if (bytes <= 0)
                {
                        fprintf(stderr, "Connection dropped or error in bitbot_op\n");
                        return -1;
                }

                buf[bytes] = '\0';

                // final message does NOT start with "cs230 STATUS "
                if (strncmp(buf, "cs230 STATUS ", 13) != 0)
                {
                        // this should be: cs230 <FLAG> BYE\n
                        fprintf(stdout, "%s", buf);
                        return 0;
                }

                //PARSIGN NUM AND OP
                char *start_of_str_buf = buf; //points to beginning of buf
                char op[16];


                //parse op

                start_of_str_buf = strchr(start_of_str_buf, ' ');
                if (!start_of_str_buf) { fprintf(stderr, "Parse error\n"); return -1; }
                start_of_str_buf++;

                start_of_str_buf = strchr(start_of_str_buf, ' ');
                if (!start_of_str_buf) { fprintf(stderr, "Parse error\n"); return -1; }
                start_of_str_buf++;

                char *irrelavent_beginning_str_length = start_of_str_buf;

                char *end_of_op_str = strchr(start_of_str_buf, ' ');
                if (!end_of_op_str) { fprintf(stderr, "Parse error\n"); return -1; }
                size_t length_of_op_str = (size_t)(end_of_op_str - irrelavent_beginning_str_length);

                if (length_of_op_str >= sizeof(op))
                        length_of_op_str = sizeof(op) - 1;

                strncpy(op, start_of_str_buf, length_of_op_str); //op now has the operator part
                op[length_of_op_str] = '\0';


                //parse num
                end_of_op_str++;
                char *num_start = end_of_op_str;

                char *num_str = strchr(num_start, '\n');
                if (num_str != NULL)
                {
                        *num_str = '\0'; //converts \n to string terminator
                }

                unsigned long num = strtoul(num_start, NULL, 10); //num = number to be operated on


                //SOLUTION GIVEN OPERATION + NUMBER

                //op: "LEFT" "RIGHT" "OR" "AND"
                //num: number to operate on

                uint32_t solution; //final answer

                if (strcmp(op, "LEFT") == 0)
                {
                        int x = (int)(num % 10);
                        solution = (uint32_t)num << x;
                }

                else if (strcmp(op, "RIGHT") == 0)
                {
                        int x = (int)(num % 10);
                        solution = (uint32_t)num >> x;
                }

                else if (strcmp(op, "OR") == 0)
                {
                        uint32_t nibble = (uint32_t)num & 0xF;
                        uint32_t mask = 0;
                        for (int i = 0; i < 8; i++)
                        {
                                mask = (mask << 4) | nibble;
                        }
                        solution = (uint32_t)num | mask;
                }

                else if (strcmp(op, "AND") == 0)
                {
                        uint32_t nibble = (uint32_t)num & 0xF;
                        uint32_t mask = 0;
                        for (int i = 0; i < 8; i++)
                        {
                                mask = (mask << 4) | nibble;
                        }

                        solution = (uint32_t)num & mask;
                }
                else
                {
                        fprintf(stderr, "Unknown operation: %s\n", op);
                        return -1;
                }

                // send solution  ->  cs230 <ANSWER>\n
                char outbuf[64];
                int len = snprintf(outbuf, sizeof(outbuf), "cs230 %u\n", solution);
                if (len < 0 || len >= (int)sizeof(outbuf))
                {
                        fprintf(stderr, "ERROR: answer too long\n");
                        return -1;
                }
                ssize_t sent_bytes = send(socket, outbuf, (size_t)len, 0);
                if (sent_bytes < 0)
                {
                        perror("ERROR: send");
                        return -1;
                }

                // get next message from server
                bytes = recv(socket, buf, 128 - 1, 0);
        }

 
}

