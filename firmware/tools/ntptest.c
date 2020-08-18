/*
 *
 * To compile: $ gcc ntptest.c -lm -o ntptest
 *
 * (C) 2014 David Lettier. http://www.lettier.com/
 * (C) 2020 Phil Crump <phil@philcrump.co.uk> 
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
#include <math.h>

#define SHORTNTPFS_TO_MS  (1000.0 / 65536.0)

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

// Structure that defines the 48 byte NTP packet protocol.

typedef struct
{

  uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                           // li.   Two bits.   Leap indicator.
                           // vn.   Three bits. Version number of the protocol.
                           // mode. Three bits. Client will pick mode 3 for client.

  uint8_t stratum;         // Eight bits. Stratum level of the local clock.
  uint8_t poll;            // Eight bits. Maximum interval between successive messages.
  int8_t precision;       // Eight bits. Precision of the local clock.

  uint16_t rootDelay_s;    // 16 bits. Total round trip delay time seconds.
  uint16_t rootDelay_f;    // 16 bits. Total round trip delay time fraction of a second.

  uint16_t rootDispersion_s; // 16 bits. Max error aloud from primary clock source seconds.
  uint16_t rootDispersion_f; // 16 bits. Max error aloud from primary clock source fraction of a second.

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

void error( char* msg )
{
    perror( msg ); // Print the error message to stderr.
    exit( 0 ); // Quit the process.
}

int main( int argc, char* argv[ ] )
{
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.
  char target_name[128];

  if(argc != 2)
  {
    printf("Usage: ./ntptest <hostname_or_ip>\n");
    return 1;
  }

  strncpy(target_name, argv[1], 127);

  if(strlen(target_name) < 1)
  {
    printf("Usage: ./ntptest <hostname_or_ip>\n");
    return 1;
  }

  printf("Target: %s\n", target_name);

  ntp_packet packet;
  memset( &packet, 0, sizeof( ntp_packet ) );
  //li = 0, vn = 3, and mode = 3.
  *((char *)&packet + 0) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.

  struct sockaddr_in serv_addr; // Server address data structure.
  struct hostent *server;      // Server data structure.

  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.

  if ( sockfd < 0 )
    error( "ERROR opening socket" );

  server = gethostbyname(target_name); // Convert URL to IP.

  if ( server == NULL )
    error( "ERROR, no such host" );

  bzero( ( char* ) &serv_addr, sizeof( serv_addr ) );
  serv_addr.sin_family = AF_INET;

  // Copy the server's IP address to the server address structure.
  bcopy( ( char* )server->h_addr, ( char* ) &serv_addr.sin_addr.s_addr, server->h_length );

  // Convert the port number integer to network big-endian style and save it to the server address structure.
  serv_addr.sin_port = htons(123);

  // Call up the server using its IP address and port number.
  if ( connect( sockfd, ( struct sockaddr * ) &serv_addr, sizeof( serv_addr) ) < 0 )
    error( "ERROR connecting" );

  // Set Origin timestamp in packet
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

  // Set Received Timestamp
  clock_gettime(CLOCK_REALTIME, &tv);

  if ( n < 0 )
    error( "ERROR reading from socket" );

  // Convert all fields from network byte order back to host byte order
  packet.refId = ntohl( packet.refId );

  packet.refTm_s = ntohl( packet.refTm_s );
  packet.refTm_f = ntohl( packet.refTm_f );

  packet.origTm_s = ntohl( packet.origTm_s );
  packet.origTm_f = ntohl( packet.origTm_f );

  packet.rxTm_s = ntohl( packet.rxTm_s );
  packet.rxTm_f = ntohl( packet.rxTm_f );

  packet.txTm_s = ntohl( packet.txTm_s );
  packet.txTm_f = ntohl( packet.txTm_f );

  packet.rootDelay_s = ntohl( packet.rootDelay_s );
  packet.rootDelay_f = ntohl( packet.rootDelay_f );

  packet.rootDispersion_s = ntohl( packet.rootDispersion_s );
  packet.rootDispersion_f = ntohl( packet.rootDispersion_f );

  packet.refId = ntohl( packet.refId );

  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)

  time_t txTm = ( time_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );

  char gpsRefId[5];
  strncpy(gpsRefId, (char *)&(packet.refId), 4);
  gpsRefId[4] = '\0';

  printf(" - Server Stratum:                %"PRIu8"\n", packet.stratum);
  printf(" - Reference Identifier:          %s\n", gpsRefId);
  printf(" - Reference Sync Timestamp:      %"PRIu32".%"PRIu32"\n", packet.refTm_s, packet.refTm_f);
  float precision_raw = pow(2, packet.precision);
  printf(" - Server Clock Precision:        ");
  if(precision_raw < 1e-7)
  {
    printf("%.1fns\n", precision_raw * 1e9);
  }
  else if(precision_raw < 1e-4)
  {
    printf("%.3fus\n", precision_raw * 1e6);
  }
  else
  {
    printf("%.3fms\n", precision_raw * 1e3);
  }
  printf(" - Server Clock Root Delay:       %.2fms\n", (1000 * packet.rootDelay_s) + (SHORTNTPFS_TO_MS * packet.rootDelay_f));
  printf(" - Server Clock Dispersion:       %.2fms\n", (1000 * packet.rootDispersion_s) + (SHORTNTPFS_TO_MS * packet.rootDispersion_f));
  printf("\n");
  printf(" - Client Origin Timestamp:       %"PRIu32".%"PRIu32"\n", packet.origTm_s, packet.origTm_f);
  printf(" - Server Received Timestamp:     %"PRIu32".%"PRIu32"\n", packet.rxTm_s, packet.rxTm_f);
  printf(" - Server Transmitted Timestamp:  %"PRIu32".%"PRIu32"\n", packet.txTm_s, packet.txTm_f);
  printf(" - [Client Received Timestamp]:   %"PRIu32".%"PRIu32"\n", (int32_t)(tv.tv_sec - DIFF_SEC_1970_2036), (uint32_t)(tv.tv_nsec * NTP_NS_TO_FS ));
  printf("\n");
  printf(" - - Client -> Server:            %+.3fms\n", (1000 * (packet.rxTm_s - packet.origTm_s)) + NTPFS_TO_MS * (packet.rxTm_f - packet.origTm_f));
  printf(" - - Server Internal Delay:       %+.3fms\n", NTPFS_TO_MS * (packet.txTm_f - packet.rxTm_f));
  printf(" - - Server -> Client:            %+.3fms\n", NTPFS_TO_MS * ((int64_t)(tv.tv_nsec * NTP_NS_TO_FS ) - packet.txTm_f));
  printf("\n");

  int64_t t1, t2, t3, t4, offset;

  t2 = NTP_SEC_FRAC_TO_S64(packet.rxTm_s, packet.rxTm_f);
  t1 = NTP_SEC_FRAC_TO_S64(packet.origTm_s, packet.origTm_f);
  t3 = NTP_SEC_FRAC_TO_S64((int32_t)(tv.tv_sec - DIFF_SEC_1970_2036), (uint32_t)(tv.tv_nsec * NTP_NS_TO_FS ));
  t4 = NTP_SEC_FRAC_TO_S64(packet.txTm_s, packet.txTm_f);
  /* Clock offset calculation according to RFC 4330 */
  offset = ((t2 - t1) + (t3 - t4)) / 2;

  int32_t sec  = (int32_t)((uint64_t)offset >> 32);
  int32_t frac = (uint32_t)((uint64_t)offset);

  printf(" - SNTP Calculated Clock Offset:  %+.3fms\n", (sec * 1000) + (NTPFS_TO_MS * frac));

  return 0;
}
