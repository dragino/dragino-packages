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

#ifndef MODEMINIT_H
#define MODEMINIT_H

char *get_gsm_cme_error(int code);
char *get_gsm_cms_error(int code);
char *get_gsm_error(char *answer);

char *explain_csq_buffer(char *buffer, int short_form, int ssi, int ber, int signal_quality_ber_ignore);
void explain_csq(int loglevel, int short_form, char *answer, int signal_quality_ber_ignore);

int write_to_modem(char *command, int timeout, int log_command, int print_error);
int read_from_modem(char *answer, int max, int timeout);
char *change_crlf(char *str, char ch);

// Open the serial port, returns file handle or -1 on error
int openmodem();

// Setup serial port
void setmodemparams();

// Send init strings. 
// Returns 0 on success
//         1 modem is not ready
//         2 cannot enter pin
//         3 cannot enter init strings
//         4 modem is not registered
//         5 cannot enter pdu mode
//         6 cannot enter smsc number
//         7 seen that the thread is going to terminate
// 3.1.5: now private: int initmodem(char *new_smsc, int receiving);
int initialize_modem_sending(char *new_smsc);
int initialize_modem_receiving();

// Sends a command to the modem and waits max timout*0.1 seconds for an answer.
// The function returns the length of the answer.
// The function waits until a timeout occurs or the expected answer occurs. 
// modem is the serial port file handle
// command is the command to send (may be empty or NULL)
// answer is the received answer
// max is the maxmum allowed size of the answer
// timeout control the time how long to wait for the answer
// expect is an extended regular expression. If this matches the modem answer, 
// then the program stops waiting for the timeout (may be empty or NULL).
int put_command(char *command, char *answer, int max, int timeout_count, char *expect);
int put_command0(char *command, char *answer, int max, int timeout_count, char *expect, int silent);

int talk_with_modem();

int wait_network_registration(int waitnetwork_errorsleeptime, int retry_count);

int try_closemodem(int force);
int try_openmodem();

#endif
