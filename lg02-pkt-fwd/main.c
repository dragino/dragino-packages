
/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <signal.h>		/* sigaction */
#include <errno.h>		/* error messages */

#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>		/* gai_strerror */

#include <uci.h>

#include "radio.h"
#include "parson.h"
#include "base64.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define MSG(args...)	syslog(LOG_DEBUG, args) /* message that is destined to the user */
#define TRACE() 		fprintf(stderr, "@ %s %d\n", __FUNCTION__, __LINE__);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#ifndef VERSION_STRING
  #define VERSION_STRING "undefined"
#endif

#define DEFAULT_KEEPALIVE	5	/* default time interval for downstream keep-alive packet */
#define DEFAULT_STAT		30	/* default time interval for statistics */
#define PUSH_TIMEOUT_MS		100
#define PULL_TIMEOUT_MS		200
#define FETCH_SLEEP_MS		10	/* nb of ms waited when a fetch return no packets */

#define	PROTOCOL_VERSION	1

#define PKT_PUSH_DATA	0
#define PKT_PUSH_ACK	1
#define PKT_PULL_DATA	2
#define PKT_PULL_RESP	3
#define PKT_PULL_ACK	4

#define NB_PKT_MAX		8 /* max number of packets per fetch/send cycle */

#define MIN_LORA_PREAMB	6 /* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB	8
#define MIN_FSK_PREAMB	3 /* minimum FSK preamble length for this application */
#define STD_FSK_PREAMB	4

#define TX_BUFF_SIZE	((540 * NB_PKT_MAX) + 30)
#define STATUS_SIZE	    1024

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* pkt struct */
static struct pkt_rx_s pktrx[QUEUESIZE]; /* allocat queuesize struct of pkt_rx_s */

static int pt = 0, prev = 0;  /* pt is point of receive packet postion,  prev is point of process packet thread*/

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* packets filtering configuration variables */
static bool fwd_valid_pkt = true; /* packets with PAYLOAD CRC OK are forwarded */
static bool fwd_error_pkt = false; /* packets with PAYLOAD CRC ERROR are NOT forwarded */
static bool fwd_nocrc_pkt = false; /* packets with NO PAYLOAD CRC are NOT forwarded */

/* network configuration variables */
static uint64_t lgwm = 0; /* Lora gateway MAC address */
static char server[64] = "server"; /* address of the server (host name or IPv4/IPv6) */
static char port[8] = "port"; /* server port for upstream traffic */
static char serv_port_down[8] = "1700"; /* server port for downstream traffic */
static char serv_port_up[8] = "1700"; /* server port for downstream traffic */
static int keepalive_time = DEFAULT_KEEPALIVE; /* send a PULL_DATA request every X seconds, negative = disabled */
static char platform[16] = "LG02/OLG02";  /* platform definition */
static char description[16] = "";                        /* used for free form description */
static char email[32]  = "mail";                        /* used for contact email */
static char LAT[16] = "lati";
static char LON[16] = "long";
static char gatewayid[64] = "gateway_id";
static char rxsf[8] = "RXSF";
static char txsf[8] = "TXSF";
static char rxbw[8] = "RXBW";
static char txbw[8] = "TXBW";
static char rxcr[8] = "RXCR";
static char txcr[8] = "TXCR";
static char rxprlen[8] = "RXPRLEN";
static char txprlen[8] = "TXPRLEN";
static char rx_freq[16] = "rx_freq";    /* rx frequency of radio */
static char tx_freq[16] = "tx_freq";    /* tx frequency of radio */
static char pktdebug[4] = "yes";          /* debug info option */

/* Set location */
static float lat=0.0;
static float lon=0.0;
static int   alt=0;

/* values available for the 'modulation' parameters */
/* NOTE: arbitrary values */
#define MOD_UNDEFINED   0
#define MOD_LORA        0x10
#define MOD_FSK         0x20

/* values available for the 'tx_mode' parameter */
#define IMMEDIATE       0
#define TIMESTAMPED     1
#define ON_GPS          2

/* statistics collection configuration variables */
static unsigned stat_interval = DEFAULT_STAT; /* time interval (in sec) at which statistics are collected and displayed */

/* gateway <-> MAC protocol variables */
static uint32_t net_mac_h; /* Most Significant Nibble, network order */
static uint32_t net_mac_l; /* Least Significant Nibble, network order */

/* network sockets */
static int sock_stat; /* socket for upstream traffic */
static int sock_up; /* socket for upstream traffic */
static int sock_down; /* socket for downstream traffic */

/* network protocol variables */
static struct timeval push_timeout_half = {0, (PUSH_TIMEOUT_MS * 500)}; /* cut in half, critical for throughput */
static struct timeval pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}; /* non critical for throughput */

/* hardware access control and correction */
static pthread_mutex_t mx_concent = PTHREAD_MUTEX_INITIALIZER; /* control access to the concentrator */

/* measurements to establish statistics */
static pthread_mutex_t mx_meas_up = PTHREAD_MUTEX_INITIALIZER; /* control access to the upstream measurements */
static uint32_t meas_nb_rx_rcv = 0; /* count packets received */
static uint32_t meas_nb_rx_ok = 0; /* count packets received with PAYLOAD CRC OK */
static uint32_t meas_nb_rx_bad = 0; /* count packets received with PAYLOAD CRC ERROR */
static uint32_t meas_nb_rx_nocrc = 0; /* count packets received with NO PAYLOAD CRC */
static uint32_t meas_up_pkt_fwd = 0; /* number of radio packet forwarded to the server */
static uint32_t meas_up_network_byte = 0; /* sum of UDP bytes sent for upstream traffic */
static uint32_t meas_up_payload_byte = 0; /* sum of radio payload bytes sent for upstream traffic */
static uint32_t meas_up_dgram_sent = 0; /* number of datagrams sent for upstream traffic */
static uint32_t meas_up_ack_rcv = 0; /* number of datagrams acknowledged for upstream traffic */

static pthread_mutex_t mx_meas_dw = PTHREAD_MUTEX_INITIALIZER; /* control access to the downstream measurements */
static uint32_t meas_dw_pull_sent = 0; /* number of PULL requests sent for downstream traffic */
static uint32_t meas_dw_ack_rcv = 0; /* number of PULL requests acknowledged for downstream traffic */
static uint32_t meas_dw_dgram_rcv = 0; /* count PULL response packets received for downstream traffic */
static uint32_t meas_dw_network_byte = 0; /* sum of UDP bytes sent for upstream traffic */
static uint32_t meas_dw_payload_byte = 0; /* sum of radio payload bytes sent for upstream traffic */
static uint32_t meas_nb_tx_ok = 0; /* count packets emitted successfully */
static uint32_t meas_nb_tx_fail = 0; /* count packets were TX failed for other reasons */

/* auto-quit function */
static uint32_t autoquit_threshold = 0; /* enable auto-quit after a number of non-acknowledged PULL_DATA (0 = disabled)*/

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

#define UCI_CONFIG_FILE "/etc/config/lorawan"
static struct uci_context * ctx = NULL; 
static bool get_config(const char *section, char *option, int len);

static double difftimespec(struct timespec end, struct timespec beginning);

static void wait_ms(unsigned long a); 

/* radio devices */

radiodev *rxdev;
radiodev *txdev;

/* threads */
void thread_rec(void);
void thread_up(void);
void thread_down(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */
/*
 *  * Print a message and return to caller.
 *   * Caller specifies "errnoflag".
 *    */
static void err_doit(int errnoflag, int error, const char *fmt, va_list ap)
{
    char    buf[MAXLINE];

    vsnprintf(buf, MAXLINE-1, fmt, ap);
    if (errnoflag)
        snprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, ": %s",
            strerror(error));
    strcat(buf, "\n");
    fflush(stdout);     /* in case stdout and stderr are the same */
    fputs(buf, stderr);
    fflush(NULL);       /* flushes all stdio output streams */
}

static void err_quit(const char *fmt, ...)
{
    va_list     ap;

    va_start(ap, fmt);
    err_doit(0, 0, fmt, ap);
    va_end(ap);
    exit(1);
}

static void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = true;;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = true;
	}
	return;
}

static bool get_config(const char *section, char *option, int len) {
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

static double difftimespec(struct timespec end, struct timespec beginning) {
	double x;
	
	x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
	x += (double)(end.tv_sec - beginning.tv_sec);
	
	return x;
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

static struct pkt_rx_s *pkt_alloc(void) {
    return (struct pkt_rx_s *) malloc(sizeof(struct pkt_rx_s));
}

static void pktrx_clean(struct pkt_rx_s *rx) {
    rx->empty = 1;
    rx->size = 0;
    memset(rx->payload, 0, sizeof(rx->payload));
}

static void daemonize(const char *cmd)
{
	int					i, fd0, fd1, fd2;
	pid_t				pid;
	struct rlimit		rl;
	struct sigaction	sa;

	/*
	 * Clear file creation mask.
	 */
	umask(0);

	/*
	 * Get maximum number of file descriptors.
	 */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		err_quit("%s: can't get file limit", cmd);

	/*
	 * Become a session leader to lose controlling TTY.
	 */
	if ((pid = fork()) < 0)
		err_quit("%s: can't fork", cmd);
	else if (pid != 0) /* parent */
		exit(0);
	setsid();

	/*
	 * Ensure future opens won't allocate controlling TTYs.
	 */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err_quit("%s: can't ignore SIGHUP", cmd);
	if ((pid = fork()) < 0)
		err_quit("%s: can't fork", cmd);
	else if (pid != 0) /* parent */
		exit(0);

	/*
	 * Change the current working directory to the root so
	 * we won't prevent file systems from being unmounted.
	 */
	if (chdir("/") < 0)
		err_quit("%s: can't change directory to /", cmd);

	/*
	 * Close all open file descriptors.
	 */
	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	/*
	 * Attach file descriptors 0, 1, and 2 to /dev/null.
	 */
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

	/*
	 * Initialize the log file.
	 */
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		err_quit("%s: unexpected file descriptors %d %d %d",
		  fd0, fd1, fd2, cmd);
	}
}

#define LOCKFILE "/var/run/pkt_fwd.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

static int lockfile(int fd)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd, F_SETLK, &fl));
}

static int already_running(void)
{
	int		fd;
	char	buf[16];

	fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
	if (fd < 0) {
		syslog(LOG_ERR, "can't open %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return(1);
		}
		syslog(LOG_ERR, "can't lock %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	ftruncate(fd, 0);
	sprintf(buf, "%ld", (long)getpid());
	write(fd, buf, strlen(buf)+1);
	return(0);
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char *argv[])
{
	struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
	int i, j; /* loop variable and temporary variable for return value */
	
	/* threads */
	pthread_t thrid_rec;
	pthread_t thrid_up;
	pthread_t thrid_down;
	

	/* network socket creation */
	struct addrinfo hints;
	struct addrinfo *result; /* store result of getaddrinfo */
	struct addrinfo *q; /* pointer to move into *result data */
	char host_name[64];
	char port_name[64];
	
	/* variables to get local copies of measurements */
	uint32_t cp_nb_rx_rcv;
	uint32_t cp_nb_rx_ok;
	uint32_t cp_nb_rx_bad;
	uint32_t cp_nb_rx_nocrc;
	uint32_t cp_up_pkt_fwd;
	uint32_t cp_up_network_byte;
	uint32_t cp_up_payload_byte;
	uint32_t cp_up_dgram_sent;
	uint32_t cp_up_ack_rcv;
	uint32_t cp_dw_pull_sent;
	uint32_t cp_dw_ack_rcv;
	uint32_t cp_dw_dgram_rcv;
	uint32_t cp_dw_network_byte;
	uint32_t cp_dw_payload_byte;
	uint32_t cp_nb_tx_ok;
	uint32_t cp_nb_tx_fail;
	
	/* statistics variable */
	time_t t;
	char stat_timestamp[24];
	float rx_ok_ratio;
	float rx_bad_ratio;
	float rx_nocrc_ratio;
	float up_ack_ratio;
	float dw_ack_ratio;
	
    unsigned long long ull = 0;

	/* display version informations */
	//MSG("*** Basic Packet Forwarder for LG02 ***\nVersion: " VERSION_STRING "\n");
    
    char *cmd;

    if ((cmd = strrchr(argv[0], '/')) == NULL)
        cmd = argv[0];
    else
        cmd++;
	
    //daemonize(cmd);
    
    if (already_running()) {
        err_quit("%s has already running\n", cmd);
    }

	/* load configuration */
    if (!get_config("general", server, 64)){
        strcpy(server, "52.169.76.203");  /*set default:router.eu.thethings.network*/
        MSG("get option server=%s\n", server);
    }

    if (!get_config("general", port, 8)){
        strcpy(port, "1700");
        MSG("get option port=%s\n", port);
    }

    strcpy(serv_port_up, port);
    strcpy(serv_port_down, port);

    if (!get_config("general", email, 32)){
        strcpy(email, "dragino@dragino.com");
        MSG("get option email=%s\n", email);
    }

    if (!get_config("general", gatewayid, 64)){
        strcpy(gatewayid, "a84041ffff16c21c");  /*set default:router.eu.thethings.network*/
        MSG("get option gatewayid=%s\n", gatewayid);
    } 

    if (!get_config("general", LAT, 16)){
        strcpy(LAT, "0");
        MSG("get option lat=%s\n", LAT);
    }

    if (!get_config("general", LON, 16)){
        strcpy(LON, "0");
        MSG("get option lon=%s\n", LON);
    }

    /*
    if (!get_config("general", pfwd_debug, 4)){
        MSG("get option pfwd_debug=%s", pfwd_debug);
    }
    */

    if (!get_config("radio", rxsf, 8)){
        strcpy(rxsf, "7");
        MSG("get option rxsf=%s\n", rxsf);
    }

    if (!get_config("radio", txsf, 8)){
        strcpy(txsf, "9");
        MSG("get option txsf=%s\n", txsf);
    }

    if (!get_config("radio", rxcr, 8)){
        strcpy(rxcr, "5");
        MSG("get option coderate=%s\n", rxcr);
    }
    
    if (!get_config("radio", txcr, 8)){
        strcpy(txcr, "5");
        MSG("get option coderate=%s\n", txcr);
    }

    if (!get_config("radio", rxbw, 8)){
        strcpy(rxbw, "125000");
        MSG("get option rxbw=%s\n", rxbw);
    }

    if (!get_config("radio", txbw, 8)){
        strcpy(txbw, "125000");
        MSG("get option rxbw=%s\n", txbw);
    }

    if (!get_config("radio", rxprlen, 8)){
        strcpy(rxprlen, "8");
        MSG("get option rxbw=%s\n", rxprlen);
    }

    if (!get_config("radio", txprlen, 8)){
        strcpy(txprlen, "8");
        MSG("get option rxbw=%s\n", txprlen);
    }

    if (!get_config("radio", rx_freq, 16)){
        strcpy(rx_freq, "868100000"); /* default frequency*/
        MSG("get option rxfreq=%s ", rx_freq);
    }

    if (!get_config("radio", tx_freq, 16)){
        strcpy(tx_freq, "869525000"); /* default frequency*/
        MSG("get option txfreq=%s\n", tx_freq);
    }

    lat = atof(LAT);
    lon = atof(LON);

    sscanf(gatewayid, "%llx", &ull);
    lgwm = ull;


    /* init the queue of receive packets */
    for (i = 0; i < QUEUESIZE; i++) {  
        pktrx_clean(&pktrx[i]);
    }
	
    /* radio device init */

    rxdev = (radiodev *) malloc(sizeof(radiodev));

    txdev = (radiodev *) malloc(sizeof(radiodev));
    
    rxdev->nss = 15;
    rxdev->rst = 8;
    rxdev->dio[0] = 7;
    rxdev->dio[1] = 6;
    rxdev->dio[2] = 0;
    rxdev->spiport = lgw_spi_open(SPI_DEV_RX);
    if (rxdev->spiport < 0) { 
        printf("open spi_dev_tx error!\n");
        goto clean;
    }
    rxdev->freq = atol(rx_freq);
    rxdev->sf = atoi(rxsf);
    rxdev->bw = atol(rxbw);
    rxdev->cr = atoi(rxcr);
    rxdev->crc = 1;
    rxdev->prlen = atoi(rxprlen);
    rxdev->invertio = 0;
    strcpy(rxdev->desc, "RXRF");

    txdev->nss = 24;
    txdev->rst = 23;
    txdev->dio[0] = 22;
    txdev->dio[1] = 20;
    txdev->dio[2] = 0;
    txdev->spiport = lgw_spi_open(SPI_DEV_TX);
    if (txdev->spiport < 0) {
        syslog(LOG_ERR, "open spi_dev_tx error!\n");
        goto clean;
    }
    txdev->freq = atol(tx_freq);
    txdev->sf = atoi(txsf);
    txdev->bw = atol(txbw);
    txdev->cr = atoi(txcr);
    txdev->crc = 1;
    txdev->prlen = atoi(txprlen);
    txdev->invertio = 0;
    strcpy(txdev->desc, "TXRF");

    MSG("RX struct: spiport=%d, freq=%ld, sf=%d\n", rxdev->spiport, rxdev->freq, rxdev->sf);
    MSG("TX struct: spiport=%d, freq=%ld, sf=%d\n", txdev->spiport, txdev->freq, txdev->sf);

    if(get_radio_version(rxdev) == false) 
        goto clean;

    setup_channel(rxdev);

    setup_channel(txdev);

	/* sanity check on configuration variables */
	// TODO
	
	/* process some of the configuration variables */
	net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
	net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));
	
	/* prepare hints to open network sockets */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; /* should handle IP v4 or v6 automatically */
	hints.ai_socktype = SOCK_DGRAM;
	
	/* look for server address w/ upstream port */
    //MSG("Looking for server with upstream port......\n");
	i = getaddrinfo(server, serv_port_up, &hints, &result);
	if (i != 0) {
		MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", server, serv_port_up, gai_strerror(i));
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
		MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", server, serv_port_up);
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

	/* look for server address w/ status port */
    MSG("loor for server with status port......\n");
	i = getaddrinfo(server, port, &hints, &result);
	if (i != 0) {
		MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", server, serv_port_up, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	/* try to open socket for status traffic */
	for (q=result; q!=NULL; q=q->ai_next) {
		sock_stat = socket(q->ai_family, q->ai_socktype,q->ai_protocol);
		if (sock_stat == -1) continue; /* try next field */
		else break; /* success, get out of loop */
	}
	if (q == NULL) {
		MSG("ERROR: [stat] failed to open socket to any of server %s addresses (port %s)\n", server, port);
		i = 1;
		for (q=result; q!=NULL; q=q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
			MSG("INFO: [stat] result %i host:%s service:%s\n", i, host_name, port_name);
			++i;
		}
		exit(EXIT_FAILURE);
	}
	
	/* connect so we can send/receive packet with the server only */
	i = connect(sock_stat, q->ai_addr, q->ai_addrlen);
	if (i != 0) {
		MSG("ERROR: [stat] connect returned %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);

	/* look for server address w/ downstream port */
    //MSG("loor for server with downstream port......\n");
	i = getaddrinfo(server, serv_port_down, &hints, &result);
	if (i != 0) {
		MSG("ERROR: [down] getaddrinfo on address %s (port %s) returned %s\n", server, serv_port_up, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	
	/* try to open socket for downstream traffic */
	for (q=result; q!=NULL; q=q->ai_next) {
		sock_down = socket(q->ai_family, q->ai_socktype,q->ai_protocol);
		if (sock_down == -1) continue; /* try next field */
		else break; /* success, get out of loop */
	}
	if (q == NULL) {
		MSG("ERROR: [down] failed to open socket to any of server %s addresses (port %s)\n", server, serv_port_up);
		i = 1;
		for (q=result; q!=NULL; q=q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
			MSG("INFO: [down] result %i host:%s service:%s\n", i, host_name, port_name);
			++i;
		}
		exit(EXIT_FAILURE);
	}
	
	/* connect so we can send/receive packet with the server only */
	i = connect(sock_down, q->ai_addr, q->ai_addrlen);
	if (i != 0) {
		MSG("ERROR: [down] connect returned %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	
	/* spawn threads to manage radio receive queue*/
    MSG("Spawn threads to manage fifo payload...\n");
	i = pthread_create( &thrid_rec, NULL, (void * (*)(void *))thread_rec, NULL);
	if (i != 0) {
		MSG("ERROR: [main] impossible to create upstream thread\n");
		exit(EXIT_FAILURE);
	}

	/* spawn threads to manage upstream and downstream */
    MSG("spawn threads to manage upsteam and downstream...\n");
	i = pthread_create( &thrid_up, NULL, (void * (*)(void *))thread_up, NULL);
	if (i != 0) {
		MSG("ERROR: [main] impossible to create upstream thread\n");
		exit(EXIT_FAILURE);
	}

	i = pthread_create( &thrid_down, NULL, (void * (*)(void *))thread_down, NULL);
	if (i != 0) {
		MSG("ERROR: [main] impossible to create downstream thread\n");
		exit(EXIT_FAILURE);
	}
	
	/* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
	sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
	sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */
    
    MSG("Start lora packet forward daemon, server = %s, port = %s\n", server, port);
/* ------------------------------------------------------------------------------------ */
	/* main loop task : statistics collection and send status to server */

    static char status_report[STATUS_SIZE]; /* status report as a JSON object */

    int stat_index=0;

    /* pre-fill the data buffer with fixed fields */
    status_report[0] = PROTOCOL_VERSION;
    status_report[3] = PKT_PUSH_DATA;

    /* fill GEUI  8bytes */
    *(uint32_t *)(status_report + 4) = net_mac_h; 
    *(uint32_t *)(status_report + 8) = net_mac_l; 
	
	while (!exit_sig && !quit_sig) {
		
		/* get timestamp for statistics */
		t = time(NULL);
		strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));
		
		/* access upstream statistics, copy and reset them */
		pthread_mutex_lock(&mx_meas_up);
		cp_nb_rx_rcv       = meas_nb_rx_rcv;
		cp_nb_rx_ok        = meas_nb_rx_ok;
		cp_nb_rx_bad       = meas_nb_rx_bad;
		cp_nb_rx_nocrc     = meas_nb_rx_nocrc;
		cp_up_pkt_fwd      = meas_up_pkt_fwd;
		cp_up_network_byte = meas_up_network_byte;
		cp_up_payload_byte = meas_up_payload_byte;
		cp_up_dgram_sent   = meas_up_dgram_sent;
		cp_up_ack_rcv      = meas_up_ack_rcv;
		meas_nb_rx_rcv = 0;
		meas_nb_rx_ok = 0;
		meas_nb_rx_bad = 0;
		meas_nb_rx_nocrc = 0;
		meas_up_pkt_fwd = 0;
		meas_up_network_byte = 0;
		meas_up_payload_byte = 0;
		meas_up_dgram_sent = 0;
		meas_up_ack_rcv = 0;
		pthread_mutex_unlock(&mx_meas_up);
		if (cp_nb_rx_rcv > 0) {
			rx_ok_ratio = (float)cp_nb_rx_ok / (float)cp_nb_rx_rcv;
			rx_bad_ratio = (float)cp_nb_rx_bad / (float)cp_nb_rx_rcv;
			rx_nocrc_ratio = (float)cp_nb_rx_nocrc / (float)cp_nb_rx_rcv;
		} else {
			rx_ok_ratio = 0.0;
			rx_bad_ratio = 0.0;
			rx_nocrc_ratio = 0.0;
		}
		if (cp_up_dgram_sent > 0) {
			up_ack_ratio = (float)cp_up_ack_rcv / (float)cp_up_dgram_sent;
		} else {
			up_ack_ratio = 0.0;
		}
		
		/* access downstream statistics, copy and reset them */
		pthread_mutex_lock(&mx_meas_dw);
		cp_dw_pull_sent    =  meas_dw_pull_sent;
		cp_dw_ack_rcv      =  meas_dw_ack_rcv;
		cp_dw_dgram_rcv    =  meas_dw_dgram_rcv;
		cp_dw_network_byte =  meas_dw_network_byte;
		cp_dw_payload_byte =  meas_dw_payload_byte;
		cp_nb_tx_ok        =  meas_nb_tx_ok;
		cp_nb_tx_fail      =  meas_nb_tx_fail;
		meas_dw_pull_sent = 0;
		meas_dw_ack_rcv = 0;
		meas_dw_dgram_rcv = 0;
		meas_dw_network_byte = 0;
		meas_dw_payload_byte = 0;
		meas_nb_tx_ok = 0;
		meas_nb_tx_fail = 0;
		pthread_mutex_unlock(&mx_meas_dw);
		if (cp_dw_pull_sent > 0) {
			dw_ack_ratio = (float)cp_dw_ack_rcv / (float)cp_dw_pull_sent;
		} else {
			dw_ack_ratio = 0.0;
		}
		
		/* display a report */
        /*
		printf("\n##### %s #####\n", stat_timestamp);
		printf("### [UPSTREAM] ###\n");
		printf("# RF packets received by concentrator: %u\n", cp_nb_rx_rcv);
		printf("# CRC_OK: %.2f%%, CRC_FAIL: %.2f%%, NO_CRC: %.2f%%\n", 100.0 * rx_ok_ratio, 100.0 * rx_bad_ratio, 100.0 * rx_nocrc_ratio);
		printf("# RF packets forwarded: %u (%u bytes)\n", cp_up_pkt_fwd, cp_up_payload_byte);
		printf("# PUSH_DATA datagrams sent: %u (%u bytes)\n", cp_up_dgram_sent, cp_up_network_byte);
		printf("# PUSH_DATA acknowledged: %.2f%%\n", 100.0 * up_ack_ratio);
		printf("### [DOWNSTREAM] ###\n");
		printf("# PULL_DATA sent: %u (%.2f%% acknowledged)\n", cp_dw_pull_sent, 100.0 * dw_ack_ratio);
		printf("# PULL_RESP(onse) datagrams received: %u (%u bytes)\n", cp_dw_dgram_rcv, cp_dw_network_byte);
		printf("# RF packets sent to concentrator: %u (%u bytes)\n", (cp_nb_tx_ok+cp_nb_tx_fail), cp_dw_payload_byte);
		printf("# TX errors: %u\n", cp_nb_tx_fail);
		printf("##### END #####\n");
        */

        /* start composing datagram with the header */
        uint8_t token_h = (uint8_t)rand(); /* random token */
        uint8_t token_l = (uint8_t)rand(); /* random token */
        status_report[1] = token_h;
        status_report[2] = token_l;

        stat_index = 12; /* 12-byte header */

        /* get timestamp for statistics */
        t = time(NULL);
        strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

        j = snprintf((char *)(status_report + stat_index), STATUS_SIZE - stat_index, "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}", stat_timestamp, lat, lon, (int)alt, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, (float)0, 0, 0, platform, email, description);
        stat_index += j;
        status_report[stat_index] = 0; /* add string terminator, for safety */

        MSG("\nINFO (json): [stat update] %s\n", (char *)(status_report + 12)); /* DEBUG: display JSON stat */

        //send the update
        send(sock_stat, (void *)status_report, stat_index, 0);

		/* wait for next reporting interval */
		wait_ms(1000 * stat_interval);
	}
	
	/* wait for upstream thread to finish (1 fetch cycle max) */
	pthread_join(thrid_rec, NULL);
	pthread_join(thrid_up, NULL);
	pthread_cancel(thrid_down); /* don't wait for downstream thread */
	
	/* if an exit signal was received, try to quit properly */
	if (exit_sig) {
		/* shut down network sockets */
		shutdown(sock_stat, SHUT_RDWR);
		shutdown(sock_up, SHUT_RDWR);
		shutdown(sock_down, SHUT_RDWR);
	}

clean:
    free(rxdev);
    free(txdev);
	
	MSG("INFO: Exiting packet forwarder program\n");
	exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 1: RECEIVING PACKETS AND SAVE A QUEUE ---------------------- */

void thread_rec(void) {
    rxlora(rxdev->spiport, RXMODE_SCAN);
    MSG("Listening at SF%i on %.6lf Mhz. port%i\n", rxdev->sf, (double)(rxdev->freq)/1000000, rxdev->spiport);
	while (!exit_sig && !quit_sig) {
        if(digitalRead(rxdev->dio[0]) == 1) {
            digitalWrite(rxdev->dio[0], 0);
            printf("Catch RXDONE, receive = %s\n", pktrx[pt].empty ? "Ready" : "Full");
            if (pktrx[pt].empty) {
                if (received(rxdev->spiport, &pktrx[pt]) == true) 
                    if (++pt >= QUEUESIZE)
                        pt = 0;
            }

            wait_ms(FETCH_SLEEP_MS); /* wait after receive a packet */
        }
    }
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 2: RECEIVING PACKETS AND FORWARDING THEM ---------------------- */

void thread_up(void) {
	int i, j; /* loop variables */

	/* allocate memory for packet fetching and processing */
	int nb_pkt;
	
	/* local timestamp variables until we get accurate GPS time */
	struct timespec fetch_time;
	struct tm * x;
	char fetch_timestamp[28]; /* timestamp as a text string */
	
	/* data buffers */
	uint8_t buff_up[TX_BUFF_SIZE]; /* buffer to compose the upstream packet */
	int buff_index;
	uint8_t buff_ack[32]; /* buffer to receive acknowledges */

	/* protocol variables */
	uint8_t token_h; /* random token for acknowledgement matching */
	uint8_t token_l; /* random token for acknowledgement matching */
	
	/* ping measurement variables */
	struct timespec send_time;
	struct timespec recv_time;
	
	/* set upstream socket RX timeout */
	i = setsockopt(sock_up, SOL_SOCKET, SO_RCVTIMEO, (void *)&push_timeout_half, sizeof push_timeout_half);
	if (i != 0) {
		MSG("ERROR: [up] setsockopt returned %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* pre-fill the data buffer with fixed fields */
	buff_up[0] = PROTOCOL_VERSION;
	buff_up[3] = PKT_PUSH_DATA;
	*(uint32_t *)(buff_up + 4) = net_mac_h;
	*(uint32_t *)(buff_up + 8) = net_mac_l;
	
	while (!exit_sig && !quit_sig) {

        //MSG("INFO: [up] loop...\n");
		/* fetch packets */

        if (pktrx[prev].empty) {
            if (++prev >= QUEUESIZE)
                prev = 0;

            wait_ms(FETCH_SLEEP_MS); /* wait a short time if no packets */
            continue;
        }

		/* local timestamp generation until we get accurate GPS time */
		clock_gettime(CLOCK_REALTIME, &fetch_time);
		x = gmtime(&(fetch_time.tv_sec)); /* split the UNIX timestamp to its calendar components */
		snprintf(fetch_timestamp, sizeof fetch_timestamp, "%04i-%02i-%02iT%02i:%02i:%02i.%06liZ", (x->tm_year)+1900, (x->tm_mon)+1, x->tm_mday, x->tm_hour, x->tm_min, x->tm_sec, (fetch_time.tv_nsec)/1000); /* ISO 8601 format */
		
        /* get timestamp for statistics */
        struct timeval now;
        gettimeofday(&now, NULL);
        uint32_t tmst = (uint32_t)(now.tv_sec*1000000 + now.tv_usec);

		/* start composing datagram with the header */
		token_h = (uint8_t)rand(); /* random token */
		token_l = (uint8_t)rand(); /* random token */
		buff_up[1] = token_h;
		buff_up[2] = token_l;
		buff_index = 12; /* 12-byte header */

        j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, "{\"rxpk\":[{\"tmst\":%u,\"time\":\"%s\",\"chan\":7,\"rfch\":0,\"freq\":%u,\"stat\":1,\"modu\":\"LORA\",\"datr\":\"SF%dBW125\",\"codr\":\"4/%s\",\"lsnr\":7.8", tmst, fetch_timestamp, rxdev->freq, rxdev->sf, rxcr);
    
        buff_index += j;

        j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"rssi\":%f,\"size\":%u", pktrx[prev].rssi, pktrx[prev].size);
		
        buff_index += j;

        memcpy((void *)(buff_up + buff_index), (void *)",\"data\":\"", 9);
		buff_index += 9;
		
        j = bin_to_b64((uint8_t *)pktrx[prev].payload, pktrx[prev].size, (char *)(buff_up + buff_index), 341); /* 255 bytes = 340 chars in b64 + null char */

        buff_index += j;
        buff_up[buff_index] = '"';
        ++buff_index;
        buff_up[buff_index] = '}';
        ++buff_index;
        buff_up[buff_index] = ']';
        ++buff_index;
        buff_up[buff_index] = '}';
        ++buff_index;
        buff_up[buff_index] = '\0'; /* add string terminator, for safety */
		
	    MSG("\nINFO (JSON): [up] %s\n", (char *)(buff_up + 12)); /* DEBUG: display JSON payload */
		
		/* send datagram to server */
		send(sock_up, (void *)buff_up, buff_index, 0);
		clock_gettime(CLOCK_MONOTONIC, &send_time);
		pthread_mutex_lock(&mx_meas_up);
		meas_up_dgram_sent += 1;
		meas_up_network_byte += buff_index;
		
		/* wait for acknowledge (in 2 times, to catch extra packets) */
		for (i=0; i<2; ++i) {
			j = recv(sock_up, (void *)buff_ack, sizeof buff_ack, 0);
			clock_gettime(CLOCK_MONOTONIC, &recv_time);
			if (j == -1) {
				if (errno == EAGAIN) { /* timeout */
					continue;
				} else { /* server connection error */
					break;
				}
			} else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != PKT_PUSH_ACK)) {
				//MSG("WARNING: [up] ignored invalid non-ACL packet\n");
				continue;
			} else if ((buff_ack[1] != token_h) || (buff_ack[2] != token_l)) {
				//MSG("WARNING: [up] ignored out-of sync ACK packet\n");
				continue;
			} else {
				MSG("INFO: [up] PUSH_ACK received in %i ms\n", (int)(1000 * difftimespec(recv_time, send_time)));
				meas_up_ack_rcv += 1;
				break;
			}
		}
		pthread_mutex_unlock(&mx_meas_up);

        /* clear a packet */

        pktrx_clean(&pktrx[prev]);

        if (++prev <= QUEUESIZE)
            prev = 0;

        wait_ms(FETCH_SLEEP_MS); /* wait after receive a packet */
        //MSG("INFO: [up]return loop\n");
	}
	MSG("\nINFO: End of upstream thread\n");
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 3: POLLING SERVER AND EMITTING PACKETS ------------------------ */

void thread_down(void) {

	int i; /* loop variables */

	/* configuration and metadata for an outbound packet */
	struct pkt_tx_s txpkt;
	bool sent_immediate = false; /* option to sent the packet immediately */
	
	/* local timekeeping variables */
	struct timespec send_time; /* time of the pull request */
	struct timespec recv_time; /* time of return from recv socket call */
	
	/* data buffers */
	uint8_t buff_down[1024]; /* buffer to receive downstream packets */
	uint8_t buff_req[12]; /* buffer to compose pull requests */
	int msg_len;
	
	/* protocol variables */
	uint8_t token_h; /* random token for acknowledgement matching */
	uint8_t token_l; /* random token for acknowledgement matching */
	bool req_ack = false; /* keep track of whether PULL_DATA was acknowledged or not */
	
    /* JSON parsing variables */
    JSON_Value *root_val = NULL;
    JSON_Object *txpk_obj = NULL;
	JSON_Value *val = NULL; /* needed to detect the absence of some fields */
	const char *str; /* pointer to sub-strings in the JSON data */
	short x0, x1;
	
	/* auto-quit variable */
	uint32_t autoquit_cnt = 0; /* count the number of PULL_DATA sent since the latest PULL_ACK */
	
	/* set downstream socket RX timeout */
	i = setsockopt(sock_down, SOL_SOCKET, SO_RCVTIMEO, (void *)&pull_timeout, sizeof pull_timeout);
	if (i != 0) {
		MSG("ERROR: [down] setsockopt returned %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* pre-fill the pull request buffer with fixed fields */
	buff_req[0] = PROTOCOL_VERSION;
	buff_req[3] = PKT_PULL_DATA;
	*(uint32_t *)(buff_req + 4) = net_mac_h;
	*(uint32_t *)(buff_req + 8) = net_mac_l;
	

	while (!exit_sig && !quit_sig) {
		
		/* auto-quit if the threshold is crossed */
		if ((autoquit_threshold > 0) && (autoquit_cnt >= autoquit_threshold)) {
			exit_sig = true;
			MSG("INFO: [down] the last %u PULL_DATA were not ACKed, exiting application\n", autoquit_threshold);
			break;
		}
		
		/* generate random token for request */
		token_h = (uint8_t)rand(); /* random token */
		token_l = (uint8_t)rand(); /* random token */
		buff_req[1] = token_h;
		buff_req[2] = token_l;
		
		/* send PULL request and record time */
		send(sock_down, (void *)buff_req, sizeof buff_req, 0);
		clock_gettime(CLOCK_MONOTONIC, &send_time);
        //MSG("INFO: [down] send pull_data, %ld\n", send_time.tv_nsec);
		pthread_mutex_lock(&mx_meas_dw);
		meas_dw_pull_sent += 1;
		pthread_mutex_unlock(&mx_meas_dw);
		req_ack = false;
		autoquit_cnt++;
		
		/* listen to packets and process them until a new PULL request must be sent */
		recv_time = send_time;
		while ((int)difftimespec(recv_time, send_time) < keepalive_time) {
			
			/* try to receive a datagram */
			msg_len = recv(sock_down, (void *)buff_down, (sizeof buff_down)-1, 0);
			clock_gettime(CLOCK_MONOTONIC, &recv_time);
			
			/* if no network message was received, got back to listening sock_down socket */
			if (msg_len == -1) {
				//MSG("WARNING: [down] recv returned %s\n", strerror(errno)); /* too verbose */
				continue;
			}
			
			/* if the datagram does not respect protocol, just ignore it */
			if ((msg_len < 4) || (buff_down[0] != PROTOCOL_VERSION) || ((buff_down[3] != PKT_PULL_RESP) && (buff_down[3] != PKT_PULL_ACK))) {
				MSG("WARNING: [down] ignoring invalid packet\n");
				continue;
			}
			
			/* if the datagram is an ACK, check token */
			if (buff_down[3] == PKT_PULL_ACK) {
				if ((buff_down[1] == token_h) && (buff_down[2] == token_l)) {
					if (req_ack) {
						MSG("INFO: [down] duplicate ACK received :)\n");
					} else { /* if that packet was not already acknowledged */
						req_ack = true;
						autoquit_cnt = 0;
						pthread_mutex_lock(&mx_meas_dw);
						meas_dw_ack_rcv += 1;
						pthread_mutex_unlock(&mx_meas_dw);
						//MSG("INFO: [down] PULL_ACK received in %i ms\n", (int)(1000 * difftimespec(recv_time, send_time)));
					}
				} else { /* out-of-sync token */
					MSG("INFO: [down] received out-of-sync ACK\n");
				}
				continue;
			}
			
			/* the datagram is a PULL_RESP */
			buff_down[msg_len] = 0; /* add string terminator, just to be safe */
			printf("INFO: [down] PULL_RESP received :)\n"); /* very verbose */
			MSG("\nINFO (json): [down] %s\n", (char *)(buff_down + 4)); /* DEBUG: display JSON payload */
			
			/* initialize TX struct and try to parse JSON */
			memset(&txpkt, 0, sizeof txpkt);
			root_val = json_parse_string_with_comments((const char *)(buff_down + 4)); /* JSON offset */
			if (root_val == NULL) {
				MSG("WARNING: [down] invalid JSON, TX aborted\n");
				continue;
			}
			
			/* look for JSON sub-object 'txpk' */
			txpk_obj = json_object_get_object(json_value_get_object(root_val), "txpk");
			if (txpk_obj == NULL) {
				MSG("WARNING: [down] no \"txpk\" object in JSON, TX aborted\n");
				json_value_free(root_val);
				continue;
			}
			
			/* Parse payload length (mandatory) */
			val = json_object_get_value(txpk_obj,"size");
			if (val == NULL) {
				MSG("WARNING: [down] no mandatory \"txpk.size\" object in JSON, TX aborted\n");
				json_value_free(root_val);
				continue;
			}
			txpkt.size = (uint16_t)json_value_get_number(val);
			
			/* Parse payload data (mandatory) */
			str = json_object_get_string(txpk_obj, "data"); if (str == NULL) {
				MSG("WARNING: [down] no mandatory \"txpk.data\" object in JSON, TX aborted\n");
				json_value_free(root_val);
				continue;
			}
			i = b64_to_bin(str, strlen(str), txpkt.payload, sizeof(txpkt.payload));
			if (i != txpkt.size) {
				MSG("WARNING: [down] mismatch between .size and .data size once converter to binary\n");
			}

            txlora(txdev->spiport, txpkt.payload, txpkt.size);

            MSG("INFO: [down]txpk payload in hex(%dbyte): ", txpkt.size);
            for (i = 0; i < txpkt.size; i++) {
                printf("%2x", txpkt.payload[i]);
            }
            printf("\n");

			/* free the JSON parse tree from memory */
			json_value_free(root_val);
			
			/* select TX mode */
			if (sent_immediate) {
				txpkt.tx_mode = IMMEDIATE;
			} else {
				txpkt.tx_mode = TIMESTAMPED;
			}
			
			/* record measurement data */
			pthread_mutex_lock(&mx_meas_dw);
			meas_dw_dgram_rcv += 1; /* count only datagrams with no JSON errors */
			meas_dw_network_byte += msg_len; /* meas_dw_network_byte */
			meas_dw_payload_byte += txpkt.size;
            meas_nb_tx_ok += 1;
            pthread_mutex_unlock(&mx_meas_dw);
			
			/* transfer data and metadata to the concentrator, and schedule TX */
		}
	}
	MSG("\nINFO: End of downstream thread\n");
}

/* --- EOF ------------------------------------------------------------------ */