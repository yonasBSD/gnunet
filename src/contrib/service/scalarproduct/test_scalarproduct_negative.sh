#!/bin/bash
# compute a simple scalar product
# payload for this test:
INPUTALICE="-k CCC -e 'AB,10;RO,-3;FL,-3;LOL,1;'"
INPUTBOB="-k CCC -e 'BC,-20000;RO,1000;FL,100;LOL,24;'"

# necessary to make the testing prefix deterministic, so we can access the config files
unset XDG_DATA_HOME
unset XDG_CONFIG_HOME

PREFIX=/tmp/test-scalarproduct`date +%H%M%S`

# where can we find the peers config files?
CFGALICE="-c $PREFIX/0/config"
CFGBOB="-c $PREFIX/1/config"

which timeout >/dev/null 2>&1 && DO_TIMEOUT="timeout 15"

# launch two peers in line topology non-interactively
#
# interactive mode would terminate the test immediately
# because the rest of the script is already in stdin,
# thus redirecting stdin does not suffice)
#GNUNET_LOG='scalarproduct;;;;DEBUG'
GNUNET_TESTING_PREFIX=$PREFIX ../testbed/gnunet-testbed-profiler -n -c test_scalarproduct.conf -p 2 &
PID=$!
# sleep 1 is too short on most systems, 2 works on most, 5 seems to be safe
sleep 5

# get bob's peer ID, necessary for alice
PEERIDBOB=`${DO_TIMEOUT} gnunet-peerinfo -qs $CFGBOB`

#GNUNET_LOG=';;;;DEBUG'
${DO_TIMEOUT} gnunet-scalarproduct $CFGBOB $INPUTBOB &
#RESULT=`GNUNET_LOG=';;;;DEBUG'
RESULT=`${DO_TIMEOUT} gnunet-scalarproduct $CFGALICE $INPUTALICE -p $PEERIDBOB`

# terminate the testbed
kill $PID

EXPECTED="-0CCC"
if [ "$RESULT" = "$EXPECTED" ]
then
    	echo "OK"
        exit 0
else
    	echo "Result $RESULT, expected $EXPECTED NOTOK"
        exit 1
fi
