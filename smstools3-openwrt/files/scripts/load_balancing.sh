#!/bin/bash
# ---------------------------------------------------------------------------------------
# This example is for three modems, named GSM1, GSM2 and GSM3 using queues Q1, Q2 and Q3.
# In the global part of smsd.conf, enable message counters:
# stats = /var/spool/sms/stats
# Use zero value for interval if statistics files are not used:
# stats_interval = 0
#
# Enable checkhandler (this script):
# checkhandler = /usr/local/bin/load_balancing.sh
#
# Define queues:
# [queues]
# Q1 = /var/spool/sms/Q1
# Q2 = /var/spool/sms/Q2
# Q3 = /var/spool/sms/Q3
#
# With smsd >= 3.1.7 providers are not needed to define,
# with previous versions define the following:
# [providers]
# Q1 = 0,1,2,3,4,5,6,7,8,9,s
# Q2 = 0,1,2,3,4,5,6,7,8,9,s
# Q3 = 0,1,2,3,4,5,6,7,8,9,s
#
# Add queue definition for each modem:
# [GSM1]
# queues = Q1
# etc...
# ---------------------------------------------------------------------------------------

# Settings for this script:
STATSDIR=/var/spool/sms/stats
MODEMS=( GSM1 GSM2 GSM3 )
QUEUES=( Q1 Q2 Q3 )

# ---------------------------------------------------------------------------------------

NUMBER_OF_MODEMS=${#MODEMS[@]}
NUMBER_OF_QUEUES=${#QUEUES[@]}
if [ $NUMBER_OF_MODEMS -ne $NUMBER_OF_QUEUES ]; then
  echo "ERROR: Number of queues does not match number of modems."
  exit 1 # Message is rejected.
fi

read_counter()
{
  local RESULT=0
  local FILE=$STATSDIR/$1.counter
  local COUNTER=0

  if [[ -e $FILE ]]
  then
    COUNTER=`formail -zx $1: < $FILE`
    if [ "$COUNTER" != "" ]; then
      RESULT=$COUNTER
    fi
  fi

  return $RESULT
}

# If there is Queue (or Provider) defined, load balancing is ignored:
QUEUE=`formail -zx Queue: < $1`
if [ -z "$QUEUE" ]; then
  QUEUE=`formail -zx Provider: < $1`
  if [ -z "$QUEUE" ]; then

    # Read current counters:
    for ((i = 0; i < $NUMBER_OF_MODEMS; i++)); do
      read_counter ${MODEMS[${i}]}
      eval COUNTER_${MODEMS[${i}]}=$?
    done

    QUEUE=${QUEUES[0]}
    tmp=COUNTER_${MODEMS[0]}
    COUNTER=${!tmp}
    for ((i = 1; i < $NUMBER_OF_MODEMS; i++)); do
      tmp=COUNTER_${MODEMS[${i}]}
      tmp=${!tmp}
      if [ $tmp -lt $COUNTER ]; then
        QUEUE=${QUEUES[${i}]}
        tmp=COUNTER_${MODEMS[${i}]}
        COUNTER=${!tmp}
      fi
    done

    TMPFILE=`mktemp /tmp/smsd_XXXXXX`
    cp $1 $TMPFILE
    formail -f -I "Queue: $QUEUE" < $TMPFILE > $1
    unlink $TMPFILE
  fi
fi
exit 0
