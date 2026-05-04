gcc server.c -o server -lpthread
gcc client.c -o client

echo 'rm -R uploads IN 10 SEC!!!!!!!!!!1'
sleep 5

rm -R uploads
mkdir -p uploads
ls -R
echo ''

./server 8080 & SERVER_PID=$!

# Time for the server to start
sleep 1

./client myfile.txt 127.0.0.1 8080

kill $SERVER_PID

ls -R
