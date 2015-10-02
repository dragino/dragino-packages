/*
SMS Server Tools 3
Copyright (C) 2006- Keijo Kasvi
http://smstools3.kekekasvi.com/

Based on SMS Server Tools 2 from Stefan Frings
http://www.meinemullemaus.de/
SMS Server Tools version 2 and below are Copyright (C) Stefan Frings.

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.
*/

#ifndef SMSD_CFG_H
#define SMSD_CFG_H

#include <limits.h>
#include <sys/types.h>
#include <time.h>

#ifndef __FreeBSD__
#define DEFAULT_CONFIGFILE "/etc/smsd.conf"
#else
#define DEFAULT_CONFIGFILE "%%PREFIX%%/etc/smsd.conf"
#endif

#define DATETIME_DEFAULT "%y-%m-%d %H:%M:%S"
#define LOGTIME_DEFAULT "%Y-%m-%d %H:%M:%S"
#define DATE_FILENAME_DEFAULT "%Y-%m-%d"

#define CONCATENATED_DIR_FNAME "%s/%s-concatenated"

#define MM_CORE_FNAME "/tmp/mm_smsd_%i" /* %i is PID */

#define NUMS 64
#define SIZE_NUM 16

#define DEVICE devices[process_id]
#define DEVICE_IS_SOCKET (devices[process_id].device[0] == '@')
#define DEVICE_X_IS_SOCKET (devices[x].device[0] == '@')
#define STATISTICS statistics[process_id]

// Maximum size of a message text
#define MAXTEXT 39016

// Maxmum size of a single sms, can be 160/140 or less
#define maxsms_pdu 160
#define maxsms_ucs2 140
#define maxsms_binary 140

// Sizes for some buffers:
#define SIZE_TO 100
#define SIZE_FROM 100
#define SIZE_SMSC 100
#define SIZE_QUEUENAME 100
#define SIZE_UDH_DATA 500
#define SIZE_UDH_TYPE 4096
#define SIZE_RR_CMD 513
#define SIZE_MACROS 4096
#define SIZE_HEADER 101
#define SIZE_MESSAGEIDS 4096
#define SIZE_IDENTITY 100
#define SIZE_TB 1024
#define SIZE_LOG_LINE 16384
#define SIZE_PRIVILEDGED_NUMBERS 512
#define SIZE_SMSD_DEBUG 100
#define SIZE_SHARED_BUFFER 256
#define SIZE_FILENAME_PREVIEW 256
#define SIZE_PB_ENTRY 101

#define SIZE_CHECK_MEMORY_BUFFER 512
// 3.1.12: Changed size from 16384 when CMGL* method is used:
#define SIZE_CHECK_MEMORY_BUFFER_CMGL 65536

// Check memory methods:
#define CM_NO_CPMS 0
#define CM_S_NO_CPMS "Fixed values are used because CPMS does not work."
#define CM_CPMS 1
#define CM_S_CPMS "CPMS is used."
#define CM_CMGD 2
#define CM_S_CMGD "CMGD is used."
#define CM_CMGL 3
#define CM_S_CMGL "CMGL is used."
#define CM_CMGL_DEL_LAST 4
#define CM_S_CMGL_DEL_LAST "CMGL is used and messages are deleted after all messsages are read."
#define CM_CMGL_CHECK 31
#define CM_S_CMGL_CHECK "CMGL is used and messages are taken from the list."
#define CM_CMGL_DEL_LAST_CHECK 41
#define CM_S_CMGL_DEL_LAST_CHECK "CMGL is used and messages are taken from the list, messages are deleted after all messages are read."
#define CM_CMGL_SIMCOM 5
#define CM_S_CMGL_SIMCOM "CMGL is used. SIM600 compatible, see the manual for details."

// 3.1.12:
#define select_check_memory_buffer_size() (value_in(DEVICE.check_memory_method, 5, CM_CMGL, CM_CMGL_DEL_LAST, CM_CMGL_CHECK, CM_CMGL_DEL_LAST_CHECK, CM_CMGL_SIMCOM))? SIZE_CHECK_MEMORY_BUFFER_CMGL : SIZE_CHECK_MEMORY_BUFFER

#define LENGTH_PDU_DETAIL_REC 70

// For put_command() calls:
#define EXPECT_OK_ERROR "(OK)|(ERROR)"

#define TELNET_LOGIN_PROMPT_DEFAULT "login:"
#define TELNET_LOGIN_PROMPT_IGNORE_DEFAULT "Last login:"
#define TELNET_PASSWORD_PROMPT_DEFAULT "Password:"

#define isdigitc(ch) isdigit((int)(ch))
#define isalnumc(ch) isalnum((int)(ch))

char process_title[32];         // smsd for main task, name of a modem for other tasks.
int process_id;                 // -1 for main task, all other have numbers starting with 0.
                                // This is the same as device, can be used like devices[process_id]...

time_t process_start_time;

int modem_handle;               // Handle for modem.

int put_command_timeouts;

typedef struct
{
  char name[32]; 		// Name of the queue
  char numbers[NUMS][SIZE_NUM];	// Phone numbers assigned to this queue
  char directory[PATH_MAX];		// Queue directory
} _queue;

typedef struct
{
  char name[32];		// Name of the modem
  char number[32];              // 3.1.4: SIM card's telephone number.
  char device[PATH_MAX];	// Serial port name
  int device_open_retries;      // 3.1.7: Defines count of retries when opening a device fails.
  int device_open_errorsleeptime; // 3.1.7: Sleeping time after opening error.
  int device_open_alarm_after;  // 3.1.7: Defines after how many failures an alarmhandler is called.
  char identity[SIZE_IDENTITY]; // Identification asked from the modem (CIMI)
  char conf_identity[SIZE_IDENTITY]; // Identification set in the conf file (CIMI)
  //char identity_header[SIZE_TO];// Title of current identification (IMSI)
  char queues[NUMBER_OF_MODEMS][32]; // Assigned queues
  int incoming; 		// Try to receive SMS. 0=No, 1=Low priority, 2=High priority
  int outgoing;                 // = 0 if a modem is not used to send messages.
  int report; 			// Ask for delivery report 0 or 1 (experimental state)
  int phonecalls;               // Check for phonebook status for calls, 0 or 1. 3.1.7: value 2 = +CLIP report is handled.
  char phonecalls_purge[32];    // Defines if a purge command should be used. yes / no / command to use. yes = AT^SPBD="MC"
  int phonecalls_error_max;     // 3.1.7: Max nr of errors before phonecalls are ignored.
  char pin[16];			// SIM PIN
  int pinsleeptime;             // Number of seconds to sleep after a PIN is entered.
  char mode[10]; 		// Command set version old or new
  char smsc[16];		// Number of SMSC
  int baudrate;			// Baudrate
  int send_delay;		// Makes sending characters slower (milliseconds)
  int send_handshake_select;    // 3.1.9.
  int cs_convert; 		// Convert character set  0 or 1 (iso-9660)
  char initstring[100];		// first Init String
  char initstring2[100];        // second Init String
  char eventhandler[PATH_MAX];	// event handler program or script
  char eventhandler_ussd[PATH_MAX]; // 3.1.7: event handler program or script for USSD answers
  int ussd_convert;             // 3.1.7: Convert string from USSD answer
  int rtscts;			// hardware handshake RTS/CTS, 0 or 1
  int read_memory_start;	// first memory space for sms
  char primary_memory[10];      // primary memory, if dual-memory handler is used
  char secondary_memory[10];    // secondary memory, if dual-memory handler is used
  int secondary_memory_max;     // max value for secondary memory, if dual-memory handler is used and modem does not tell correct max value
  char pdu_from_file[PATH_MAX]; // for testing purposes: incoming pdu can be read from file if this is set.
  int sending_disabled;         // 1 = do not actually send a message. For testing purposes.
  int modem_disabled;           // 1 = disables modem handling. For testing purposes. Outgoing side acts like with sending_disabled,
                                // incoming side reads messages only from file (if defined). NOTE: device name should still be defined
                                // as it's opened and closed. You can use some regular file for this. Must be an existing file.
  int decode_unicode_text;      // 1 if unicode text is decoded internally.
  int internal_combine;         // 1 if multipart message is combined internally.
  int internal_combine_binary;  // 1 if multipart binary message is combined internally. Defaults to internal_combine.
  int pre_init;                 // 1 if pre-initialization is used with a modem.
  int check_network;            // 0 if a modem does not support AT+CREG command.
  char admin_to[SIZE_TO];       // Destination number for administrative messages.
  int message_limit;            // Limit counter for outgoing messages. 0 = no limit.
  int message_count_clear;      // Period to automatically clear message counter. Value is MINUTES. 
  int keep_open;                // 1 = do not close modem while idle.
  char dev_rr[PATH_MAX];        // Script/program which is run regularly.
  char dev_rr_post_run[PATH_MAX]; // 3.1.7: Script/program which is run regularly (POST_RUN).
  int dev_rr_interval;          // Number of seconds between running a regular_run script/progdam.
  char dev_rr_cmdfile[PATH_MAX];// 
  char dev_rr_cmd[SIZE_RR_CMD]; //
  char dev_rr_logfile[PATH_MAX];
  int dev_rr_loglevel; // defaults to 5, LOG_NOTICE. Has only effect when a main log is used.
  char dev_rr_statfile[PATH_MAX];
  char logfile[PATH_MAX];       // Name or Handle of Log File
  int loglevel;                 // Log Level (9=highest). Verbosity of log file.
  int messageids;               // Defines how message id's are stored: 1 = first, 2 = last (default), 3 = all.
  int voicecall_vts_list;       // Defines how VTS command is sent: 1 = as a list like "1,2,3,4", 2 = single note with one VTS command (default).
  int voicecall_ignore_modem_response; // Delay defined with TIME: is not breaked even if modem gives some response.
  int voicecall_hangup_ath;     // If ATH is used instead of AT+CHUP.
  int voicecall_vts_quotation_marks; // Defines if AT+VTS="n" command is given with quotation marks.
  int voicecall_cpas;           // Defines if AT+CPAS is used to detect when a call is answered (phone returns OK after ATD).
  int voicecall_clcc;           // 3.1.12: Defines if AT+CLCC is used to detect when a call is answered (phone returns OK after ATD).
  int check_memory_method;      // 0 = CPMS not supported, 1 = CPMS supported and must work (default), 2 = CMGD used to check messages, 3 = CMGL is used.
  char cmgl_value[32];          // With check_memory_method 3, correct value for AT+CMGL= must be given here.
  char priviledged_numbers[SIZE_PRIVILEDGED_NUMBERS]; // Priviledged numbers in incoming messages.
  int read_timeout;             // Timeout for reading from a modem, in seconds.
  int ms_purge_hours;           // Wich check_memory_method 5 (SIM600), messages with missing part(s) are removed from a
  int ms_purge_minutes;         // modem after timeout defined with these two settings. Both values 0 disables this feature.
  int ms_purge_read;            // 1 if available parts are read when purge timeout is reached. 0 if parts are only deleted.
  int detect_message_routing;   // 0 if CMT/CDS detection is disabled.
  int detect_unexpected_input;  // 0 if if detection is disabled.
  int unexpected_input_is_trouble; // 0 if unexpected input / routed message should NOT activate trouble.log
  int adminmessage_limit;       // Limit counter for administrative alert messages. 0 = no limit.
  int adminmessage_count_clear; // Period to automatically clear administrative alert counter. Value is MINUTES. 
  int status_signal_quality;    // 1 = signal quality is written to status file.
  int status_include_counters;  // 1 = succeeded, failed and received counters are included in the status line.
  int communication_delay;      // Time between each put_command (milliseconds), some modems need this.
  int hangup_incoming_call;     // 1 = if detected unexpected input contains RING and we want to end call.
  int max_continuous_sending;   // Defines when sending is breaked to do check/do other tasks. Time in seconds.
  int socket_connection_retries; // 3.1.7: Defines count of retries when socket connection fails.
  int socket_connection_errorsleeptime; // 3.1.7: Sleeping time after socket connetcion error.
  int socket_connection_alarm_after; // 3.1.7: Defines after how many failures an alarmhandler is called.
  int report_device_details;    // Defines if device details are logged when modem process is starting.
  int using_routed_status_report; // Disables a warning about routed status reports.
  int routed_status_report_cnma; // Defines if +CNMA acknowledgement is needed to send.
  int needs_wakeup_at;          // After idle time, some modems may not answer to the first AT command.
  int keep_messages;            // Defines if messages are not deleted. Smsd continues running.
  char startstring[100];        // 3.1.7: Command(s) to send to the modem when a devicespooler is starting.
  int startsleeptime;           // 3.1.7: Second to wait after startstring is sent.
  char stopstring[100];         // 3.1.7: Command(s) to send to the modem when a devicespooler is stopping.
  int trust_spool;		// 3.1.9
  int smsc_pdu;			// 3.1.12: 1 if smsc is included in the PDU.
  char telnet_login[64];	// 3.1.12: Settings for telnet.
  char telnet_login_prompt[64];
  char telnet_login_prompt_ignore[64];
  char telnet_password[64];
  char telnet_password_prompt[64];
  int signal_quality_ber_ignore; // 3.1.14.
  int verify_pdu; // 3.1.14.
  int loglevel_lac_ci; // 3.1.14.
  int log_not_registered_after; // 3.1.14.
} _device;

// NOTE for regular run intervals: effective value is at least delaytime.

char configfile[PATH_MAX];	// Path to config file
char d_spool[PATH_MAX];		// Spool directory
char d_failed[PATH_MAX];	// Failed spool directory
char d_incoming[PATH_MAX];	// Incoming spool directory
char d_report[PATH_MAX];	// Incoming report spool directory
char d_phonecalls[PATH_MAX];    // Incoming phonecalls data directory
char d_saved[PATH_MAX];         // Directory for smsd's internal use, concatenation storage files etc.
char d_sent[PATH_MAX];		// Sent spool directory
char d_checked[PATH_MAX];	// Spool directory for checked messages (only used when no provider queues used)
char eventhandler[PATH_MAX];	// Global event handler program or script
char alarmhandler[PATH_MAX];	// Global alarm handler program or script
char checkhandler[PATH_MAX];    // Handler that checks if the sms file is valid.
int alarmlevel;			// Alarm Level (9=highest). Verbosity of alarm handler.
char logfile[PATH_MAX];		// Name or Handle of Log File
int  loglevel;			// Log Level (9=highest). Verbosity of log file.
_queue queues[NUMBER_OF_MODEMS]; // Queues
_device devices[NUMBER_OF_MODEMS]; // Modem devices
int delaytime;			// sleep-time after workless
int delaytime_mainprocess;      // sleep-time after workless, main process. If -1, delaytime is used.
int blocktime;			// sleep-time after multiple errors
int blockafter;                 // Block modem after n errors
int errorsleeptime;		// sleep-time after each error
int autosplit;			// Splitting of large text messages 0=no, 1=yes 2=number with text, 3=number with UDH
int receive_before_send;	// if 1 smsd tries to receive one message before sending
int store_received_pdu;         // 0=no, 1=unsupported pdu's only, 2=unsupported and 8bit/unicode, 3=all
int store_sent_pdu;             // 0=no, 1=failed pdu's only, 2=failed and 8bit/unicode, 3=all
int validity_period;            // Validity period for messages.
int decode_unicode_text;        // 1 if unicode text is decoded internally.
int internal_combine;           // 1 if multipart message is combined internally.
int internal_combine_binary;    // 1 if multipart binary message is combined internally. Defaults to internal_combine.
int keep_filename;              // 0 if unique filename is created to each directory when a message file is moved.
int store_original_filename;    // 1 if an original filename is saved to message file when it's moved from
                                // outgoing directory to spooler. Works together with keep_filename.
int date_filename;              // 1 or 2 if YYYYMMDD is included to the filename of incoming message.
char regular_run[PATH_MAX];     // Script/program which is run regularly.
int regular_run_interval;       // Number of seconds between running a regular_run script/progdam.
char admin_to[SIZE_TO];         // Destination number for administrative messages.
int filename_preview;           // Number of chars of message text to concatenate to filename.
int incoming_utf8;              // 1 if incoming files are saved using UTF-8 character set.
int outgoing_utf8;              // 1 if outgoing files are automatically converted from UTF-8 to ISO and GSM.
int log_charconv;               // 1 if character set conversion is logged.
int log_single_lines;           // 1 if linefeeds are removed from the modem response to be logged.
int executable_check;           // 0 if eventhandler and other executables are NOT checked during the startup checking.
int keep_messages;              // For testing purposes: messages are not deleted and smsd stops after first run.
char priviledged_numbers[SIZE_PRIVILEDGED_NUMBERS]; // Priviledged numbers in incoming messages.
int ic_purge_hours;             // If internal_combine is used, concatenation storage is checked every ic_purge_interval minutes
int ic_purge_minutes;           // and if there is message parts older than defined, they are handled or deleted.
int ic_purge_read;              // 1 = message parts are stored as single messages. 0 = parts are just deleted.
int ic_purge_interval;          // 
char shell[PATH_MAX];           // Shell used to run eventhandler, defaults to /bin/sh
char adminmessage_device[32];   // Name of device used to send administrative messages of mainspooler.
int smart_logging;              // 1 = if loglevel is less than 7, degug log is written is there has been any errors.
int status_signal_quality;      // 1 = signal quality is written to status file.
int status_include_counters;    // 1 = succeeded, failed and received counters are included in the status line.
int hangup_incoming_call;       // 1 = if detected unexpected input contains RING and we want to end call.
int max_continuous_sending;     // Defines when sending is breaked to do check/do other tasks. Time in minutes.
int voicecall_hangup_ath;       // If ATH is used instead of AT+CHUP.

// 3.1.5:
int trust_outgoing;             // 1 = it's _sure_ that files are created by rename AND permissions are correct. Speeds up spooling.

// 3.1.5:
int ignore_outgoing_priority;   // 1 = Priority: high header is not checked. Speeds up spooling.

// 3.1.7:
int ignore_exec_output;         // 1 = stdout and stderr of eventhandlers is _not_ checked.

// 3.1.7:
mode_t conf_umask;              // File mode creation mask for smsd and modem processes.

// 3.1.7:
int trim_text;                  // 1 = trailing whitespaces are removed from text:

// 3.1.7:
int use_linux_ps_trick;         // 1 = change argv[0] to "smsd: MAINPROCESS", "smsd: GSM1" etc.

// 3.1.7:
int log_unmodified;

// 3.1.7:
char suspend_filename[PATH_MAX];

// 3.1.9:
int spool_directory_order;

// 3.1.9: 1 if read_from_modem is logged.
int log_read_from_modem;

int message_count;              // Counter for sent messages. Multipart message is one message.

int terminate;                  // The current process terminates if this is 1

char username[65];              // user and group name which are used to run.
char groupname[65];             // (max length is just a guess)

char infofile[PATH_MAX];        // Hepler file for stopping the smsd smoothly.
char pidfile[PATH_MAX];         // File where a process id is stored.

// Command line arguments:
char arg_username[65];
char arg_groupname[65];
char arg_infofile[PATH_MAX];
char arg_pidfile[PATH_MAX];
char arg_logfile[PATH_MAX];
int arg_terminal;
// 3.1.7:
char arg_7bit_packed[512];
int do_encode_decode_arg_7bit_packed;

int terminal;                   // 1 if smsd is communicating with terminal.
pid_t device_pids[NUMBER_OF_MODEMS]; // Pid's of modem processes.
char run_info[PATH_MAX];        // Information about external script/program execution.

char communicate[32];           // Device name for terminal communication mode.

char international_prefixes[PATH_MAX +1];
char national_prefixes[PATH_MAX +1];

// Storage for startup errors:
char *startup_err_str;
int startup_err_count;

// Storage for PDU's:
char *incoming_pdu_store;
char *outgoing_pdu_store;
char *routed_pdu_store;

// Storage for getfile errors:
char *getfile_err_store;

// Text buffer for error messages:
char tb[SIZE_TB];

// Buffer for SIM memory checking:
char *check_memory_buffer;
size_t check_memory_buffer_size;

int os_cygwin;                  // 1 if we are on Cygwin.

char language_file[PATH_MAX];   // File name of translated headers.
char yes_chars[SIZE_HEADER];    // Characters which mean "yes" in the yesno() question.
char no_chars[SIZE_HEADER];     // See details inside read_translation() function.
char yes_word[SIZE_HEADER];     // "yes" printed as an output.
char no_word[SIZE_HEADER];      // "no"
char datetime_format[SIZE_HEADER]; // strftime format string for time stamps (not inside status reports).
char logtime_format[SIZE_HEADER]; // 3.1.7: strftime format string for logging time stamps
char date_filename_format[SIZE_HEADER]; // 3.1.7: strftime format string for date_filename
int translate_incoming;         // 0 if incoming message headers are NOT transtaled.

// 3.1.14:
int logtime_us;
int logtime_ms;

// 3.1.14:
int shell_test;

// Next two are for debugging purposes:
int enable_smsd_debug;
char smsd_debug[SIZE_SMSD_DEBUG]; // Header of an outgoing message file.

/* initialize all variable with default values */

void initcfg();


/* read the config file */

int readcfg();


/* Retuns the array-index and the directory of a queue or -1 if
   not found. Name is the name of the queue or a phone number. */

int getqueue(char* name, char* directory);


/* Returns the array-index of a device or -1 if not found */

int getdevice(char* name);


/* Show help */

void help();

/* parse arguments */

void parsearguments(int argc,char** argv);

int startup_check(int result);

void abnormal_termination(int all);

char *tb_sprintf(char* format, ...);

int savephonecall(char *entry_number, int entry_type, char *entry_text);

#endif
