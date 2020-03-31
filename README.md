Project: Reliable File Transfer Protocal

Group Members:
Hanyi Wang (hw48), Donghui Zhang (dz26), Hongyu Ji (hj29)

In this project, we implemented TCP protocal using UDP socket. Sliding window protocal is used to ensure reliable transmission of files. Our implementation is with C++ language where we maintained both sender window and receiver window for the transmission of both data packet as well as ack packet.

1. Packet format:

Data packet

| checksum | seq_num | acc_seq_num | packet_len | is_last_packet | data |

checksum (unsigned short): calculated by all rest elements, used to check error
seq_num (int): sequence number ranging from [0 to 2 * WINDOW_SIZE - 1]
acc_seq_num (int): accumulative sequence number to prevent out-of-window situation
packet_len (int): size of data in this current packet
is_last_packet (bool): indicator of the packet carrying the last portion of the file
data (char *): pointer to the content of file

Ack packet

| checksum | ack | acc_ack | is_meta | is_last |

checksum (unsigned short): calculated by all rest elements, used to check error
ack (int): ack sequence number ranging from [0 to 2 * WINDOW_SIZE - 1]
acc_ack (int): accumulative ack sequence number to prevent out-of-window situation
is_meta (bool): indicator of the packet carrying the file information
is_last (bool): indicator of the last ack packet

meta data packet

| checksum | seq_num | file_len | file_dir_name_len | file_dir_name |

checksum (unsigned short): calculated by all rest elements, used to check error
seq_num (int): sequence number of meta data packet, always will be 65535
file_len (int): the total size of the file (in bytes)
file_dir_name_len (int): the size of the directory + file name
file_dir_name (char *): pointer to the name of the file


2. Important takeaway in our implementation:

We implemented sliding window protocal to ensure transfer reliability as well as efficiency. We implemented different rules for moving the windows for sender and receiver. For sender, we only move when the head of the window received its ack, while for receiver, the window moves whenever the head of the window send ack back.

Also, we reuse the ack data structure to send the "end of procedure" flag from sender to receiver. Giving it the ack number of 65534, we send 1000 consecutive flag ack packet to receiver so that the probablity of not receiving this end flag is very low even in the highly unreliable environment. To make sure that the recevier stop properly and gracefully, we also add a timeout when all receiver sent all its ack to ensure it will eventually stop.

Finally, we used cummulative sequence number and ack number to keep track of the process. This ensures that some tricky situation under high delay will not affect our program. This situation may be a "last-round packet" arrives with the same ack/sequence number matching the head of window, which will move the window but not getting the correct data.


3. How to execute the program?
Sender:
./sendfile -r <recv host>:<recv port> -f <subdir>/<filename>

eg:
./sendfile -r onyx.clear.rice.edu:18021 -f testFiles/30mb.txt

a test folder is in the zip file containing several test files that can be transferred 


Receiver: (Run first)
recvfile -p <recv port>

eg: 
./recvfile -p 18021
