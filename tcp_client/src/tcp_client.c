
/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>		/* C99 types */
#include <stdio.h>		/* printf, fprintf, snprintf, fopen, fputs */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>		/* memset */
#include <time.h>		/* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>	/* timeval */
#include <unistd.h>		/* getopt, access */
#include <stdlib.h>		/* atoi, exit */
#include <errno.h>		/* error messages */

#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>		/* gai_strerror */

#include <uci.h>


/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define STRINGIFY(x)	#x
#define STR(x)			STRINGIFY(x)
#define MSG(args...)	printf(args) /* message that is destined to the user */
#define TRACE() 		fprintf(stderr, "@ %s %d\n", __FUNCTION__, __LINE__);


/* network configuration variables */
static int sock_up; /* socket for upstream traffic */
static char server_address[64] = "server_address"; /* address of the server (host name or IPv4/IPv6) */
static char server_port[16] = "server_port"; /* server port for upstream traffic */

#define FETCH_SLEEP_MS		100	/* nb of ms waited when a fetch return no packets */

/* lora packages data */
static char datapath[32];

#define UCI_CONFIG_FILE "/etc/config/tcp_client"

static struct uci_context * ctx = NULL;
static bool get_lg01_config(const char *section, char *option, int len);

static void wait_ms(unsigned long a);


static bool get_lg01_config(const char *section, char *option, int len) {
    struct uci_package * pkg = NULL;
    struct uci_element *e;
    const char *value;
    bool ret = false;

    ctx = uci_alloc_context(); 
    if (UCI_OK != uci_load(ctx, UCI_CONFIG_FILE, &pkg))  
        goto cleanup;   /* load uci conifg failed*/

    uci_foreach_element(&pkg->sections, e)
    {
        struct uci_section *st = uci_to_section(e);

        if(!strcmp(section, st->e.name))  /* compare section name */ {
            if (NULL != (value = uci_lookup_option_string(ctx, st, option))) {
	                 memset(option, 0, len);
                     strncpy(option, value, len); 
                     //MSG("get config value here, option=%s, value=%s\n", option, value);
                     ret = true;
                     break;
            }
        }
    }
    uci_unload(ctx, pkg); /* free pkg which is UCI package */
cleanup:
    uci_free_context(ctx);
    ctx = NULL;
    return ret;
}


static void wait_ms(unsigned long a) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = a / 1000;
    dly.tv_nsec = ((long)a % 1000) * 1000000;

    //MSG("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //MSG("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
    }
    return;
}


/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(void)
{
	int i, j; /* loop variable and temporary variable for return value */
	
    int fd;
	
	/* network socket creation */
	struct addrinfo hints;
	struct addrinfo *result; /* store result of getaddrinfo */
	struct addrinfo *q; /* pointer to move into *result data */
	char host_name[128];
	char port_name[64];
	
    char up_data[256];
	
	/* load configuration */
    if (!get_lg01_config("general", server_address, 64)){
        strcpy(server_address, "52.169.76.203");  /*set default:router.eu.thethings.network*/
        MSG("get option server=%s", server_address);
    }
    MSG("get option server=%s", server_address);

    if (!get_lg01_config("general", server_port, 8)){
        strcpy(server_port, "1700");
        MSG("get option port=%s", server_port);
    }
    MSG("get option port=%s", server_port);

	/* prepare hints to open network sockets */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; /* should handle IP v4 or v6 automatically */
	hints.ai_socktype = SOCK_STREAM;
	
	/* look for server address w/ upstream port */
    MSG("Looking for server with upstream port......\n");
	i = getaddrinfo(server_address, server_port, &hints, &result);
	if (i != 0) {
		MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", server_address, server_port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	
	/* try to open socket for upstream traffic */
    //MSG("Try to open socket for upstream......\n");
	for (q=result; q!=NULL; q=q->ai_next) {
		sock_up = socket(q->ai_family, q->ai_socktype,q->ai_protocol);
		if (sock_up == -1) continue; /* try next field */
		else break; /* success, get out of loop */
	}
	if (q == NULL) {
		MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", server_address, server_port);
		i = 1;
		for (q=result; q!=NULL; q=q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
			MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, port_name);
			++i;
		}
		exit(EXIT_FAILURE);
	}
	
	/* connect so we can send/receive packet with the server only */
	i = connect(sock_up, q->ai_addr, q->ai_addrlen);
	if (i != 0) {
		MSG("ERROR: [up] connect returned %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

    while(1) {

        for (j = 1; j <= 5; j++) {

            sprintf(datapath, "/var/iot/data%d", j);

            if ((fd = open(datapath, O_RDONLY)) < 0 ){
                continue;
            } else {
                memset(up_data, 0, sizeof(up_data));
                if ((i = read(fd, up_data, sizeof(up_data))) < 0){  
                    //MSG("No content!");
                    if (close(fd) != 0) {
                        MSG("can't close up_data file!");
                    }
                    continue;
                }

            }

            if (close(fd) != 0) {
                MSG("can't close up_data file!");
            }

            if ((fd = open(datapath, O_WRONLY|O_TRUNC)) < 0 ){
                MSG("can't reopen data file!");
            } else
                close(fd);

            send(sock_up, (void *)up_data, i, 0);

            wait_ms(FETCH_SLEEP_MS); 
        }

    }
}

