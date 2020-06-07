rm -f /tmp/verify_$$.cnf

#echo "run $1 with $2" >> tmp.dat

timeout 10 $1 $2 > /tmp/verify_$$.cnf 2> /tmp/verify2_$$.cnf;
status=$?
# sleep 0.01

#echo "finish with state= $status" >> tmp.dat

if [ "$status" -eq "124" ]
then
	# we got a timeout here!
	rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
	exit 124
fi

# cat /tmp/verify.cnf
if [ "$status" -eq "10" ]
then
	./verify SAT /tmp/verify_$$.cnf $2 # 2>&1 > /dev/null
	vstat=$?

	if [ "$vstat" -eq "1" ]
	then
		rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
		exit 10
	else
		echo "solver out:"
		cat /tmp/verify_$$.cnf
		echo "solver err:"
		cat /tmp/verify2_$$.cnf
		rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
		exit 15
	fi
else
	# if the instance is unsatisfiable or another error occurred, report it
#	echo "exit with $status" >> tmp.dat


     if [ "$status" -ne "20" ]
     then
         echo "solver output:"
         cat /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
	rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
	exit $status
     fi


     timeout 30 picosat $2 2> /dev/null 1> /dev/null
     lstat=$?

     if [ "$status" -eq "124" ]
     then
        # we got a timeout here!
	mkdir -p picosatTimeout # collect instances where lingeling performs badly
        mv $2 picosatTimeout
	rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
        exit 124
     fi

     if [ $lstat -ne "20" ]
     then
       echo "solver output:"
       cat /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
       rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
       exit 25
     fi
    rm -f /tmp/verify_$$.cnf /tmp/verify2_$$.cnf
    exit 20
fi

