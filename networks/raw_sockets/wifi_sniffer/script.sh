gcc -o main main.c

sudo ip link set wlp2s0 down
sudo iw dev wlp2s0 set type monitor
sudo ip link set wlp2s0 up

sudo ./main
