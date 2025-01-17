#!/bin/sh

set -ex

SERVER1=ec2-18-188-120-82.us-east-2.compute.amazonaws.com
SERVER2=ec2-18-226-251-77.us-east-2.compute.amazonaws.com
SERVER3=ec2-18-118-255-189.us-east-2.compute.amazonaws.com

cat <<EOF > ~/logcabin/conf1.conf
serverId = 1
listenAddresses = $SERVER1
clusterUUID = foo
storagePath = storage1
logPolicy = NOTICE
snapshotMinLogSize = 99999999999
EOF

cat <<EOF > ~/logcabin/conf2.conf
serverId = 2
listenAddresses = $SERVER2
clusterUUID = foo
storagePath = storage2
logPolicy = NOTICE
snapshotMinLogSize = 99999999999
EOF

cat <<EOF > ~/logcabin/conf3.conf
serverId = 3
listenAddresses = $SERVER3
clusterUUID = foo
storagePath = storage3
logPolicy = NOTICE
snapshotMinLogSize = 99999999999
EOF

sleep 5

echo 'SERVER 1 ================================'

ssh -v -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER1 <<EOF
set +x
cd logcabin
killall -9 perf LogCabin Reconfigure || true
rm -rf storage1 1.log
./build/LogCabin --config conf1.conf --bootstrap
echo DONE BOOTSTRAPPING
#nohup perf record -o perf1.data --call-graph=dwarf ./build/LogCabin --config conf1.conf --log 1.log >1.out 2>&1 </dev/null & 
nohup ./build/LogCabin --config conf1.conf --log 1.log >1.out 2>&1 </dev/null & 
ps aux | grep LogCabin
EOF

echo 'SERVER 2 ================================'

ssh -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER2 <<EOF
set +x
cd logcabin
killall -9 perf LogCabin Reconfigure || true
rm -rf storage2 2.log
#nohup perf record -o perf2.data --call-graph=dwarf ./build/LogCabin --config conf2.conf --log 2.log >2.out 2>&1 </dev/null & 
nohup ./build/LogCabin --config conf2.conf --log 2.log >2.out 2>&1 </dev/null & 
ps aux | grep LogCabin
EOF

echo 'SERVER 3 ================================'

ssh -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER3 <<EOF
set +x
cd logcabin
killall -9 perf LogCabin Reconfigure || true
rm -rf storage3 3.log
#nohup perf record -o perf3.data --call-graph=dwarf ./build/LogCabin --config conf3.conf --log 3.log >3.out 2>&1 </dev/null & 
nohup ./build/LogCabin --config conf3.conf --log 3.log >3.out 2>&1 </dev/null & 
ps aux | grep LogCabin
EOF

sleep 5

echo 'RECONFIGURE =============================='

~/logcabin/build/Examples/Reconfigure --cluster=$SERVER1 set $SERVER1 $SERVER2 $SERVER3
~/logcabin/build/Examples/HelloWorld --cluster=$SERVER1,$SERVER2,$SERVER3

LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH ./ycsbc -db logcabin -threads 100 -P ./workloads/workloadd.spec -host $SERVER1,$SERVER2,$SERVER3 | tee "$(date +%s).log"

sleep 5

ssh -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER1 <<EOF
killall -9 perf LogCabin Reconfigure || true
EOF

ssh -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER2 <<EOF
killall -9 perf LogCabin Reconfigure || true
EOF

ssh -o StrictHostKeyChecking=no -i ~/.ssh/jesse-2024.pem $SERVER3 <<EOF
killall -9 perf LogCabin Reconfigure || true
EOF
