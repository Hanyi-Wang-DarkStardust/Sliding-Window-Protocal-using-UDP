//
// Created by Hanyi Wang on 3/6/20.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include "iostream"
#include "utils.h"


using namespace std;

int main(int argc, char **argv) {
    if (argc != 3) {
        perror("You have to input 3 arguments! \n");
        exit(0);
    }

    // Get parameters 1
    char *command_to_check = argv[1];
    if (strcmp(command_to_check, "-p") != 0) {
        perror("Invalid command 1, it should be -p! \n");
        exit(0);
    }

    // Get parameters 2
    char *recv_port_str = argv[2];
    if (!recv_port_str) {
        perror("Invalid recv_port input! \n");
        exit(0);
    }
    int recv_port = atoi(recv_port_str);
    if (recv_port < 18000 || recv_port > 18200) {
        perror("Receiver port number should be within 18000 and 18200! \n");
        exit(0);
    }

    int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0) {
        perror("Unable to create socket! \n");
        exit(1);
    }

    struct sockaddr_in server_sin, client_sin;
    socklen_t client_len = sizeof(client_sin);
    memset(&server_sin, 0, sizeof(server_sin));
    server_sin.sin_family = AF_INET;
    server_sin.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sin.sin_port = htons(recv_port);

    if (::bind(server_sock, (struct sockaddr *) &server_sin, sizeof(server_sin)) < 0) {
        perror("binding socket to address error!! \n");
        exit(1);
    }
    fcntl(server_sock, F_SETFL, O_NONBLOCK);

    char buff[BUFFER_SIZE];
    char ack_buff[ACK_BUFF_LEN];
    ofstream outFile;

    int last_seq = -1;
    int curr_seq = -1;

    int acc_recv_seq = -1;
    bool first_recv_meta = true;

    int send_count = 0;
    int recv_count = 0;

    vector<receiver_window_node *> window;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        receiver_window_node *node = (receiver_window_node *) malloc(
                2 * sizeof(int) + 2 * sizeof(bool) + PACKET_DATA_LEN);
        node->isReceived = false;
        node->packet_len = 0;
        node->data = (char *) malloc(PACKET_DATA_LEN);
        window.push_back(node);
    }

    // Metadata stop and wait with timeout
    bool meta_OK = false;

    while (true) {
        memset(buff, 0, BUFFER_SIZE);
        int recv_len = recvfrom(server_sock, buff, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *) &client_sin,
                                &client_len);
        if (recv_len <= 0) continue;    // Failed receiving data

        int seq_num = *(int *) (buff + sizeof(unsigned short));
        int acc_seq_num = *(int *) (buff + sizeof(unsigned short) + sizeof(int));
        int packet_len = *(int *) (buff + sizeof(unsigned short) + 2 * sizeof(int));

        if (seq_num == META_DATA_FLAG) {    // Meta Data
            if (meta_OK) continue;

            int file_path_len = *(int *) (buff + sizeof(unsigned short) + 2 * sizeof(int));

            if (!check_meta_checksum(buff, file_path_len)) {  // OR PUT IT IN FRONT AND USE MAX_META_LEN???
                cout << "[recv corrupt data]" << endl;
                continue;
            } else {
                cout << "[recv data] meta data about file information received" << endl;
            }

            char file_path[MAX_FILE_PATH_LEN];
            memcpy(file_path, buff + 3 * sizeof(int) + sizeof(unsigned short), MAX_FILE_PATH_LEN);
            // create an ACK packet
            *(int *) (ack_buff + sizeof(unsigned short)) = META_DATA_FLAG;    // Ack num
            *(int *) (ack_buff + sizeof(unsigned short) + sizeof(int)) = META_DATA_FLAG;
            *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int)) = true; // is_meta
            *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int) + sizeof(bool)) = false; // is_last
            *(unsigned short *) ack_buff = get_checksum(ack_buff + sizeof(unsigned short),
                                                        ACK_BUFF_LEN - sizeof(unsigned short));   // Checksum

            sendto(server_sock, &ack_buff, ACK_BUFF_LEN, 0, (struct sockaddr *) &client_sin, client_len);
            if (first_recv_meta) {
                char tmp_file_path[MAX_FILE_PATH_LEN];
                memcpy(tmp_file_path, file_path, MAX_FILE_PATH_LEN);
                char * dir = strtok(tmp_file_path, "/");
                createDir(dir); // Create directory if not exists yet
                outFile.open(strcat(file_path, ".recv"), ios::out | ios::binary);
                first_recv_meta = false;
                cout << "File path is:" << file_path << " File length is: " << file_path_len << " Created file is: "
                     << file_path << endl;
            }
            curr_seq = 0;
        } else if (seq_num == END_DATA_FLAG) {
            meta_OK = true;
            if (!check_ack_checksum(buff)) {
                cout << "[recv corrupt end flag]" << endl;
            } else {
                cout << "[complete] sent " << send_count << " ack packet and received: " << recv_count << " data packet" << endl;
                break;
            }
        } else {
            meta_OK = true;
            bool is_last_packet = *(bool *) (buff + sizeof(unsigned short) + 3 * sizeof(int));
            if (!check_checksum(buff)) {
                cout << "[recv corrupt data]" << endl;
                continue;
            }

            if (is_last_packet) last_seq = seq_num;

            // Check if this is out of the range of window, ignore and do not send ack back!
            if (!inWindow(seq_num, curr_seq - 1)) {
                cout << "[recv data] " << (long) acc_seq_num * PACKET_DATA_LEN << " (" << packet_len << ") IGNORED" << endl;
                // create an ACK packet
                *(int *) (ack_buff + sizeof(unsigned short)) = seq_num;    // Ack num
                *(int *) (ack_buff + sizeof(unsigned short) + sizeof(int)) = acc_seq_num;
                *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int)) = false; // is_meta
                *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int) +
                           sizeof(bool)) = is_last_packet;  // is_last
                *(unsigned short *) ack_buff = get_checksum(ack_buff + sizeof(unsigned short),
                                                            ACK_BUFF_LEN - sizeof(unsigned short));   // Checksum

                sendto(server_sock, &ack_buff, ACK_BUFF_LEN, 0, (struct sockaddr *) &client_sin, client_len);
                continue;
            } else {
                // If the packet is received? (duplicate) Do not send ack back!
                receiver_window_node *packet_in_window = window[seq_num % (WINDOW_SIZE)];
                if (packet_in_window->isReceived) {
                    *(int *) (ack_buff + sizeof(unsigned short)) = seq_num;    // Ack num
                    *(int *) (ack_buff + sizeof(unsigned short) + sizeof(int)) = acc_seq_num;
                    *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int)) = false; // is_meta
                    *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int) + sizeof(bool)) = is_last_packet;  //is_last
                    *(unsigned short *) ack_buff = get_checksum(ack_buff + sizeof(unsigned short),
                                                                ACK_BUFF_LEN - sizeof(unsigned short));   // Checksum
                    sendto(server_sock, ack_buff, ACK_BUFF_LEN, 0, (struct sockaddr *) &client_sin, client_len);
                    continue;
                } else {

                    if (acc_seq_num <= acc_recv_seq) {
                        cout << "[recv data] " << (long) acc_seq_num * PACKET_DATA_LEN << " (" << packet_len
                             << ") IGNORED" << endl;
                        continue;
                    }

                    // Copy data into the node
                    packet_in_window->isReceived = true;
                    packet_in_window->packet_len = packet_len;
                    packet_in_window->seq_num = seq_num;
                    packet_in_window->is_last = is_last_packet;

                    memset(packet_in_window->data, 0, PACKET_DATA_LEN * sizeof(char));
                    memcpy(packet_in_window->data, (buff + PACKET_HEADER_LEN), PACKET_DATA_LEN);
                    // Update window
                    if (seq_num == curr_seq) {  // If matches, write that to file and move window
                        // write back and move
                        int curr_idx = curr_seq % (WINDOW_SIZE);
                        receiver_window_node *currNode = window[curr_idx];
                        cout << "[recv data] " << (long) acc_seq_num * PACKET_DATA_LEN << " (" << packet_len
                             << ") ACCEPTED (in-order)" << endl;
                        while (currNode->isReceived) {
                            outFile.write(currNode->data, currNode->packet_len);
                            recv_count += 1;
                            if (curr_seq == last_seq) break;
                            curr_seq = (curr_seq + 1) % (MAX_SEQ_LEN);
                            acc_recv_seq += 1;
                            currNode->isReceived = false;
                            curr_idx = (curr_idx + 1) % (WINDOW_SIZE);
                            currNode = window[curr_idx];
                        }
                    } else {    // If fall in window, store it.
                        cout << "[recv data] " << (long) acc_seq_num * PACKET_DATA_LEN << " (" << packet_len
                             << ") ACCEPTED (out-of-order)" << endl;
                    }
                    // Create ack packet
                    *(int *) (ack_buff + sizeof(unsigned short)) = seq_num;    // Ack num
                    *(int *) (ack_buff + sizeof(unsigned short) + sizeof(int)) = acc_seq_num;
                    *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int)) = false; // is_meta
                    *(bool *) (ack_buff + sizeof(unsigned short) + 2 * sizeof(int) + sizeof(bool)) = is_last_packet;
                    *(unsigned short *) ack_buff = get_checksum(ack_buff + sizeof(unsigned short),
                                                                ACK_BUFF_LEN - sizeof(unsigned short));   // Checksum
                    sendto(server_sock, ack_buff, ACK_BUFF_LEN, 0, (struct sockaddr *) &client_sin, client_len);
                    send_count += 1;
                }
            }
        }
    }

    outFile.close();
    close(server_sock);
    for (int i = 0; i < WINDOW_SIZE; i++) {
        receiver_window_node *node = window[i];
        free(node->data);
        free(node);
    }
    return 0;
}