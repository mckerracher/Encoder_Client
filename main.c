#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <netdb.h>
#include <time.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>

#define password "enc_pass"
#define empty "empty"
#define send_complete "complete"
#define msg_acknowledge "confirm"

int buff_size = 256;
int MAX = 100000;

/*
 * Function: error
 * -------------------------
 * Prints the exit_message to stderr and exits if exit_command equals one.
 *
 * Parameters:
 * -------------------------
 * *exit_message: the message to be printed to stderr.
 * num: the exit status value.
 * exit_command: if 1, this function will exit, otherwise it will not.
 *
 * Returns:
 * -------------------------
 * void
 */
void error(const char *exit_message, int num, int exit_command) {
    fprintf(stderr, exit_message);
    if (exit_command == 1) {
        exit(num);
    }
}

/*
 * Function: check_credentials
 * -------------------------
 * Compares the two supplied strings. If they don't match return 1, otherwise return 0.
 *
 * Parameters:
 * -------------------------
 * *one: one of the strings to be checked
 * *two: the other string to be checked
 *
 * Returns:
 * -------------------------
 * ret_num: 1 if the strings don't match, 0 otherwise.
 */
int check_credentials(char *one, char *two) {
    int ret_num = 0;

    if(strcmp(one, two)) {
        ret_num = 1;
    }

    return ret_num;
}

/*
 * Function: get_file_size
 * -------------------------
 * Finds and returns the size of the file passed into it.
 *
 * Parameters:
 * -------------------------
 * check_file: the file to be checked.
 *
 * Returns:
 * -------------------------
 * file_size: the size of checked_file in integer form.
 */
int get_file_size(FILE *check_file) {
    assert(check_file != NULL);

    // finds the end of the file
    fseek(check_file, 0L, SEEK_END);

    // calculates the size of the file.
    int file_size = (int)ftell(check_file); // also casts return value of ftell to int.

    // confirms the check worked
    if(file_size < 0){
        error("get_file_size() failed!", 1, 1);
    }

    // moves the file pointer back to the beginning of the file.
    fseek(check_file, 0L, SEEK_SET);

    return file_size;
}

/*
 * Function: verify_chars_in_file
 * -------------------------
 * Checks all characters in the file passed in for validity. Only ASCII values 65 to 90 or 32. Returns 1 if all chars are valid, 0 otherwise.
 *
 * Parameters:
 * -------------------------
 * check_file: the file to be checked.
 *
 * Returns:
 * -------------------------
 * check_value: this value will be 1 if check_file only contains valid chars, 0 otherwise.
 */
int verify_chars_in_file(FILE *check_file) {
    int check_value = 1;
    int check_char;

    // move pointer to start of file
    fseek(check_file, 0L, SEEK_SET);

    // Check that chars are 65 - 90 or 32
    while ((check_char = fgetc(check_file)) != EOF) {

        // ensure the char isn't higher than 90
        if(check_char > 90) {
            check_value = 0;
        }
        // ensure the char isn't lower than 65
        else if (check_char < 65) {
            // if it is, ensure it's not a newline (10) or a space (32).
            if ((check_char != 32) && (check_char != 10)) {
                check_value = 0;
            }
        }
    }

    // reset the file pointer to the beginning of the file.
    fseek(check_file, 0L, SEEK_SET);
    return check_value;
}

/*
 * Function: setupAddressStruct
 * -------------------------
 * Set up a sockaddr_in structure with the IP address and the port number for the server socket.
 *
 * Parameters:
 * -------------------------
 * *address: socket address structure.
 * portNumber: the port number to be associated with the address.
 * srvr: the associated server.
 *
 * Returns:
 * -------------------------
 * void
 */
void setupAddressStruct(struct sockaddr_in *address, int portNumber, struct hostent *srvr){

    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));
    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
    // Copy the address to the hostent struct server
    memmove((char *)&address->sin_addr.s_addr, (char *)srvr->h_addr, srvr->h_length);

    if (srvr == NULL){
        error("Couldn't find host in setupAddressStruct!\n", 1, 1);
    }
}

/*
 * Function:
 * -------------------------
 * Generates a new socket associated with the port number passed in.
 *
 * Parameters:
 * -------------------------
 * port: the port number to be associated with the new socket.
 *
 * Returns:
 * -------------------------
 * sock: the new socket number.
 */
int client_socket_init(int port) {
    int sock; // new socket variable
    struct sockaddr_in address_of_server;
    struct hostent *host_server = gethostbyname("localhost"); // look up the DNS entry for the server’s hostname

    // set up a sockaddr_in structure with the IP address (host_server) and the port number for the server socket.
    setupAddressStruct(&address_of_server, port, host_server);

    // Call socket() to make the new socket.
    sock = socket(AF_INET, SOCK_STREAM, 0); // IPv4 domain, TCP socket type, protocol 0.

    // ensure sock() worked.
    if (sock < 0) {
        error("Couldn't open the socket in client_socket_init()!\n", 1, 1);
    }

    // Connects client socket specified in the file descriptor "sock" to the server socket whose address is
    // specified by address_of_server.
    int check = connect(sock, (struct sockaddr *)&address_of_server, sizeof(address_of_server));

    // ensure connect() worked
    if (check < 0){
        error("Connect() failed!\n", 2, 1);
    }

    return sock;
}

/*
 * Function: read_from_socket
 * -------------------------
 * Reads text into buff from the socket provided as the _sock parameter.
 *
 * Parameters:
 * -------------------------
 * buff: the buffer into which the text is placed.
 * sock: the socket from which the text is read.
 *
 * Returns:
 * -------------------------
 * void
 */
void read_from_socket(char *buff, int _sock) {
    int val; // used to confirm read() worked.

    //ensures buff is ready for the text.
    memset(buff, '\0', buff_size);

    // read text into buff.
    val = read(_sock, buff, (buff_size - 1));

    if (val < 0){
        error("Couldn't read from socket!", 1, 1);
    }
}

/*
 * Function: send_to_socket
 * -------------------------
 * Sends the content of buff to the socket provided as the parameter "_sock". If msg_bool is 1, buff is erased and the contents of msg are added to it.
 *
 * Parameters:
 * -------------------------
 * _sock: the destination socket.
 * buff: the source buffer whose content is sent to _sock.
 * msg_bool: if this is 1, the contents of msg are sent. If this is 0, they are not.
 * msg: a message sent to _sock.
 *
 * Returns:
 * -------------------------
 * void
 */
void send_to_socket(char *buff, int _sock, int msg_bool, char *msg) {
    int val; // used to confirm write() worked.

    if (msg_bool == 0) {
        // send text to _sock
        val = write(_sock, buff, strlen(buff));
    } else if (msg_bool == 1) {
        // clear buffer and add msg to it
        memset(buff, '\0', buff_size);
        strcpy(buff, msg);
        // send text to _sock
        val = write(_sock, buff, strlen(buff));
    }

    // confirmed write() worked.
    if (val < 0){
        error("Couldn't write to socket!", 1, 1);
    }
}

/*
 * Function: correct_connection_verify
 * -------------------------
 * Verifies that this client is connected to the correct server (the enc_server).
 *
 * Parameters:
 * -------------------------
 * sock: the socket used to connect to the server.
 *
 * Returns:
 * -------------------------
 * void
 */
void correct_connection_verify(int sock) {
    char temp_buff[buff_size];

    // send pw
    send_to_socket(temp_buff, sock, 1, password);

    // read message
    read_from_socket(temp_buff, sock);

    // compare to pw
    if (check_credentials(temp_buff, password) == 1) {
        close(sock);
        error("Cannot connect to server!\n", 1, 1);
    }
}

/*
 * Function: file_transmit
 * -------------------------
 * Sends the content of file_name to the destination represented by sock.
 *
 * Parameters:
 * -------------------------
 * sock: the socket through which the contents of a file are sent.
 * file_name: the file whose contents are sent.
 *
 * Returns:
 * -------------------------
 * void
 */
void file_transmit(int sock, FILE *file_name) {
    char temp_buff[buff_size];
    memset(temp_buff, '\0', buff_size);

    while(fgets(temp_buff, (buff_size -  1), file_name)) {
        send_to_socket(temp_buff, sock, 0, empty); // send the contents pulled from file by fgets
        read_from_socket(temp_buff, sock); // receive confirmation
    }

    send_to_socket(temp_buff, sock, 1, send_complete); // send transmission complete message
    read_from_socket(temp_buff, sock); // receive confirmation
}

/*
 * Function: get_return_message
 * -------------------------
 * Gets text sent from server and adds it to content.
 *
 * Parameters:
 * -------------------------
 * sock: the socket through which communication with the server takes place.
 * content: the buffer receiving the text from the server.
 *
 * Returns:
 * -------------------------
 * void
 */
void get_return_message(int sock, char *content) {
    char temp_buffer[buff_size];
    int loop = 1;

    while (loop) {
        // clear the buffer and read the message into temp_buffer.
        memset(temp_buffer, '\0', buff_size);
        read_from_socket(temp_buffer, sock);

        // if the message indicates transmission is complete, exit the loop.
        if (strcmp(temp_buffer, send_complete) == 0) {
            loop = 0;
        }
        else {
            // otherwise add it to the content buffer.
            strcat(content, temp_buffer);
        }

        // clear buffer and send the acknowledgement message
        send_to_socket(temp_buffer, sock, 1, msg_acknowledge);
    }
}

/*
 * Function: send_files_to_server
 * -------------------------
 * Sends both the text and key files to the server.
 *
 * Parameters:
 * -------------------------
 * _text_file: the text file to be sent to the server.
 * _key_file: the key file to be sent to the server.
 * _new_sock: the socket through which the files are sent.
 *
 * Returns:
 * -------------------------
 * void
 */
void send_files_to_server(FILE *_text_file, FILE *_key_file, int _new_sock) {
    file_transmit(_new_sock, _text_file);
    file_transmit(_new_sock, _key_file);
}

/*
 * Function: run_file_checks
 * -------------------------
 * Checks:
 * 1. key and text files aren't NULL
 * 2. size of key file is not less than that of the text file
 * 3. key and text files don't contain characters that aren't allowed
 *
 * Parameters:
 * -------------------------
 * _text_file: the text file
 * _key_file: the key file
 *
 * Returns:
 * -------------------------
 * void
 */
void run_file_checks(FILE *_text_file, FILE *_key_file) {

    if (_text_file == NULL || _key_file == NULL) {
        if (_text_file == NULL) {
            error("Couldn't open the plain text file!\n", 1, 1);
        } else {
            error("Couldn't open the key file!\n", 1, 1);
        }
    }

    /* Check that the keytext file is at least as big as the plaintext file */
    int text_file_size = get_file_size(_text_file);
    int key_file_size = get_file_size(_key_file);

    if (key_file_size < text_file_size) {
        error("Key file must be at least as big as the text file!\n", 1, 1);
    }


    if (verify_chars_in_file(_text_file) != 1) {
        error("The text file has invalid characters!\n", 1, 1);
    }
    if (verify_chars_in_file(_key_file) != 1) {
        error("The key file has invalid characters!\n", 1, 1);
    }
}

int main(int argc, char *argv[]) {

    // Ensure the correct number of arguments are provided.
    if (argc < 3) {
        error("Usage: ./enc_client Text_file Key_file Port_number", 1, 1);
    }

    // Open files and gets the port number provided by the command line
    FILE *text_file = fopen(argv[1], "r");
    FILE *key_file = fopen(argv[2], "r");
    int port_number = atoi(argv[3]);

    // Checks the important things (see function definition)
    run_file_checks(text_file, key_file);

    // Makes a new socket associated with the port provided by the user
    int new_sock = client_socket_init(port_number);

    // Confirms connected to correct server
    correct_connection_verify(new_sock);

    // Sends the files via the new socket.
    send_files_to_server(text_file, key_file, new_sock);

    char cipher[MAX];
    assert(cipher != NULL);
    memset(cipher, '\0', MAX);

    // Receives return text from the server
    get_return_message(new_sock, cipher);

    // Close the things
    close(new_sock);
    fclose(text_file);
    fclose(key_file);

    printf("%s\n", cipher);
    return EXIT_SUCCESS;
}
