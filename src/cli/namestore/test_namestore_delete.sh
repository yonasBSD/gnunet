#!/bin/bash
CONFIGURATION="test_namestore_api.conf"
trap "gnunet-arm -e -c $CONFIGURATION" SIGINT

LOCATION=$(which gnunet-config)
if [ -z $LOCATION ]
then
  LOCATION="gnunet-config"
fi
$LOCATION --version 1> /dev/null
if test $? != 0
then
	echo "GNUnet command line tools cannot be found, check environmental variables PATH and GNUNET_PREFIX"
	exit 77
fi

rm -rf `$LOCATION -c $CONFIGURATION -s PATHS -o GNUNET_HOME`
TEST_DOMAIN_PLUS="www.gnu"
TEST_DOMAIN_DNS="www3.gnu"
TEST_IP_PLUS="127.0.0.1"
TEST_IP_DNS="131.159.74.67"
TEST_RECORD_CNAME_SERVER="server"
TEST_RECORD_CNAME_PLUS="server.+"
TEST_RECORD_CNAME_DNS="gnunet.org"
TEST_RECORD_NAME_SERVER="server"
TEST_RECORD_NAME_PLUS="www"
TEST_RECORD_NAME_DNS="www3"
which timeout &> /dev/null && DO_TIMEOUT="timeout 5"

function start_peer
{
	gnunet-arm -s -c $CONFIGURATION
	gnunet-identity -C testego -c $CONFIGURATION
}

function stop_peer
{
	gnunet-identity -D testego -c $CONFIGURATION
	gnunet-arm -e -c $CONFIGURATION
}


start_peer
# Create a public record
gnunet-namestore -p -z testego -a -n $TEST_RECORD_NAME_DNS -t A -V $TEST_IP_PLUS -e never -c $CONFIGURATION
# Delete record
gnunet-namestore -p -z testego -d -n $TEST_RECORD_NAME_DNS -t A -V $TEST_IP_PLUS -e never -c $CONFIGURATION
# List all records
OUTPUT=`gnunet-namestore -p -z testego -D`
FOUND_IP=false
FOUND_NAME=false
for LINE in $OUTPUT ;
 do
	if echo "$LINE" | grep -q "$TEST_RECORD_NAME_DNS"; then
		FOUND_NAME=true;
	fi
	if echo "$LINE" | grep -q "$TEST_IP_PLUS"; then
		FOUND_IP=true;
	fi
 done
stop_peer


if [ $FOUND_IP = true ]
then
  echo "FAIL: Delete name in namestore: IP returned"
  exit 1
fi
