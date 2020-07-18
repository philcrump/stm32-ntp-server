/*
 *
 * (C) 2014 David Lettier.
 *
 * http://www.lettier.com/
 *
 * NTP client.
 *
 * Compiled with gcc version 4.7.2 20121109 (Red Hat 4.7.2-8) (GCC).
 *
 * Tested on Linux 3.8.11-200.fc18.x86_64 #1 SMP Wed May 1 19:44:27 UTC 2013 x86_64 x86_64 x86_64 GNU/Linux.
 *
 * To compile: $ gcc main.c -o ntpClient.out
 *
 * Usage: $ ./ntpClient.out
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define NTPFS_TO_MS  (1000.0 / 4294967296.0)
#define NTPFS_TO_US  (1000000.0 / 4294967296.0)
#define NTP_NS_TO_FS (4294967296.0 / 1000000000.0 )
#define DIFF_SEC_1970_2036          ((uint32_t)2085978496L)

#define NTP_SEC_FRAC_TO_S64(s, f)   ((int64_t)(((uint64_t)(s) << 32) | (uint32_t)(f)))
#define NTP_TIMESTAMP_TO_S64(t)     NTP_SEC_FRAC_TO_S64(ntohl((t).sec), ntohl((t).frac))

#define NTP_TIMESTAMP_DELTA 2208988800ull

#define LI(packet)   (uint8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li   & 11 000 000) >> 6
#define VN(packet)   (uint8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vn   & 00 111 000) >> 3
#define MODE(packet) (uint8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

  int portno = 123; // NTP UDP port number.

  char* host_name = "192.168.100.195"; // NTP server host-name.
  //char* host_name = "eshail.batc.org.uk"; // NTP server host-name.

  // Structure that defines the 48 byte NTP packet protocol.

  typedef struct
  {

    uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                             // li.   Two bits.   Leap indicator.
                             // vn.   Three bits. Version number of the protocol.
                             // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    uint8_t precision;       // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

  } ntp_packet;              // Total: 384 bits or 48 bytes.

  // Create and zero out the packet. All 48 bytes worth.

  ntp_packet packet;

void error( char* msg )
{
    perror( msg ); // Print the error message to stderr.

    exit( 0 ); // Quit the process.
}

int main( int argc, char* argv[ ] )
{
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.

  memset( &packet, 0, sizeof( ntp_packet ) );

  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.

  *( ( char * ) &packet + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.

  struct sockaddr_in serv_addr; // Server address data structure.
  struct hostent *server;      // Server data structure.

  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.

  if ( sockfd < 0 )
    error( "ERROR opening socket" );

  server = gethostbyname( host_name ); // Convert URL to IP.

  if ( server == NULL )
    error( "ERROR, no such host" );

  // Zero out the server address structure.

  bzero( ( char* ) &serv_addr, sizeof( serv_addr ) );

  serv_addr.sin_family = AF_INET;

  // Copy the server's IP address to the server address structure.

  bcopy( ( char* )server->h_addr, ( char* ) &serv_addr.sin_addr.s_addr, server->h_length );

  // Convert the port number integer to network big-endian style and save it to the server address structure.

  serv_addr.sin_port = htons( portno );

  // Call up the server using its IP address and port number.

  if ( connect( sockfd, ( struct sockaddr * ) &serv_addr, sizeof( serv_addr) ) < 0 )
    error( "ERROR connecting" );

  struct timespec tv;
  clock_gettime(CLOCK_REALTIME, &tv);
  packet.txTm_s = htonl( (int32_t)(tv.tv_sec - DIFF_SEC_1970_2036) );
  packet.txTm_f = htonl( tv.tv_nsec * NTP_NS_TO_FS );

  // Send it the NTP packet it wants. If n == -1, it failed.

  n = write( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );

  if ( n < 0 )
    error( "ERROR writing to socket" );

  // Wait and receive the packet back from the server. If n == -1, it failed.

  n = read( sockfd, ( char* ) &packet, sizeof( ntp_packet ) );

  clock_gettime(CLOCK_REALTIME, &tv);

  if ( n < 0 )
    error( "ERROR reading from socket" );

  // These two fields contain the time-stamp seconds as the packet left the NTP server.
  // The number of seconds correspond to the seconds passed since 1900.
  // ntohl() converts the bit/byte order from the network's to host's "endianness".

  packet.refId = ntohl( packet.refId );

  packet.refTm_s = ntohl( packet.refTm_s );
  packet.refTm_f = ntohl( packet.refTm_f );

  packet.origTm_s = ntohl( packet.origTm_s );
  packet.origTm_f = ntohl( packet.origTm_f );

  packet.rxTm_s = ntohl( packet.rxTm_s );
  packet.rxTm_f = ntohl( packet.rxTm_f );

  packet.txTm_s = ntohl( packet.txTm_s );
  packet.txTm_f = ntohl( packet.txTm_f );

  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)

  time_t txTm = ( time_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );

  // Print the time we got from the server, accounting for local timezone and conversion from UTC time.

  //printf("Time: %s", ctime( ( const time_t* ) &txTm ) );

  printf(" - Reference Identifier: %c%c%c%c\n", (char)(packet.refId >> 24), (char)(packet.refId >> 16), (char)(packet.refId >> 8), (char)(packet.refId));
  printf(" - Reference Timestamp:          %"PRIu32".%"PRIu32"\n", packet.refTm_s, packet.refTm_f);
  printf(" - Client Origin Timestamp:      %"PRIu32".%"PRIu32"\n", packet.origTm_s, packet.origTm_f);
  printf(" - Server Received Timestamp:    %"PRIu32".%"PRIu32"\n", packet.rxTm_s, packet.rxTm_f);
  printf(" - Server Transmitted Timestamp: %"PRIu32".%"PRIu32"\n", packet.txTm_s, packet.txTm_f);
  printf(" - [Client Received Timestamp]:  %"PRIu32".%"PRIu32"\n", (int32_t)(tv.tv_sec - DIFF_SEC_1970_2036), (uint32_t)(tv.tv_nsec * NTP_NS_TO_FS ));
  printf("\n");
  printf(" - - Client -> Server: %.3fms\n", (1000 * (packet.rxTm_s - packet.origTm_s)) + NTPFS_TO_MS * (packet.rxTm_f - packet.origTm_f));
  printf(" - - Server Delay:     %.3fms\n", NTPFS_TO_MS * (packet.txTm_f - packet.rxTm_f));
  printf(" - - Server -> Client: %.3fms\n", NTPFS_TO_MS * ((uint32_t)(tv.tv_nsec * NTP_NS_TO_FS ) - packet.txTm_f));
  printf("\n");

  int64_t t1, t2, t3, t4;

  t2 = NTP_SEC_FRAC_TO_S64(packet.rxTm_s, packet.rxTm_f);
  t1 = NTP_SEC_FRAC_TO_S64(packet.origTm_s, packet.origTm_f);
  t3 = NTP_SEC_FRAC_TO_S64((int32_t)(tv.tv_sec - DIFF_SEC_1970_2036), (uint32_t)(tv.tv_nsec * NTP_NS_TO_FS ));
  t4 = NTP_SEC_FRAC_TO_S64(packet.txTm_s, packet.txTm_f);
  /* Clock offset calculation according to RFC 4330 */
  t4 = ((t2 - t1) + (t3 - t4)) / 2;

  int32_t sec  = (int32_t)((uint64_t)t4 >> 32);
  int32_t frac = (uint32_t)((uint64_t)t4);

  printf(" - Calculated delay: %ds %.2fms\n", sec, NTPFS_TO_MS * frac);
  printf("\n");

  return 0;
}
