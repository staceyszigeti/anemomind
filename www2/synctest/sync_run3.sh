set -e
bash sync_init.sh 3
bash wait_a_moment.sh
bash wait_a_moment.sh
node ../utilities/RunRemoteScript.js 57f678e612063872e749d481 sample_script.sh | grep "node" > /tmp/calltmp.txt
#sed -i -e "s/node ViewRemoteScript.js//g" /tmp/calltmp.txt
echo "######################################################################################################################################################################################################################################################################################################################################################### Posted script"

