#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TCP_KILLER_BIN="/usr/local/bin/tcp_killer"

check_root_for_kill() {
    if [[ $1 == "--kill" ]] && [[ $EUID -ne 0 ]]; then
        echo -e "${RED}Ошибка: Для отправки RST-пакетов нужны root права${NC}"
        echo "Используйте: sudo $0 $*"
        exit 1
    fi
}

show_watch() {
    local pid=$1
    local port=$2
    local state=$3
    
    local cmd="$TCP_KILLER_BIN --list"
    [[ -n $pid ]] && cmd="$cmd --pid $pid"
    [[ -n $port ]] && cmd="$cmd --port $port"
    [[ -n $state ]] && cmd="$cmd --state $state"
    
    watch -n 1 $cmd
}

kill_by_pid_and_remote() {
    local target_pid=$1
    local remote_ip=$2
    local remote_port=$3
    
    echo -e "${YELLOW}Поиск соединения для PID=$target_pid, remote=$remote_ip:$remote_port${NC}"
    
    local conn_info=$($TCP_KILLER_BIN --list --pid $target_pid | grep "$remote_ip:$remote_port")
    
    if [[ -z $conn_info ]]; then
        echo -e "${RED}Соединение не найдено${NC}"
        exit 1
    fi
    
    local local_addr=$(echo $conn_info | awk '{print $3}')
    
    echo -e "${GREEN}Найдено соединение: $local_addr -> $remote_ip:$remote_port${NC}"
    echo -e "${YELLOW}Отправка RST...${NC}"
    
    sudo $TCP_KILLER_BIN --kill "$local_addr" "$remote_ip:$remote_port"
}

interactive_mode() {
    echo -e "${GREEN}=== TCP Connection Killer Interactive ===${NC}"
    echo "Вывод активных соединений:"
    $TCP_KILLER_BIN --list
    
    echo -e "\n${YELLOW}Введите PID для завершения соединения (или 0 для выхода):${NC}"
    read -r pid
    [[ $pid -eq 0 ]] && exit 0
    
    echo "Введите удалённый IP:"
    read -r remote_ip
    echo "Введите удалённый порт:"
    read -r remote_port
    
    kill_by_pid_and_remote $pid $remote_ip $remote_port
}

main() {
    if [[ $# -eq 0 ]]; then
        interactive_mode
        exit 0
    fi
    
    check_root_for_kill "$@"
    
    case $1 in
        --list)
            shift
            $TCP_KILLER_BIN --list "$@"
            ;;
        --watch)
            shift
            show_watch "$@"
            ;;
        --kill)
            if [[ $# -eq 4 ]]; then
                sudo $TCP_KILLER_BIN --kill "$2" "$3:$4"
            else
                echo "Использование: $0 --kill <LOCAL_IP:PORT> <REMOTE_IP> <REMOTE_PORT>"
                echo "Или: $0 --pid <PID> --remote <IP:PORT>"
            fi
            ;;
        --pid)
            if [[ $# -ge 4 && $3 == "--remote" ]]; then
                remote_spec=$4
                remote_ip=$(echo $remote_spec | cut -d: -f1)
                remote_port=$(echo $remote_spec | cut -d: -f2)
                kill_by_pid_and_remote $2 $remote_ip $remote_port
            else
                $TCP_KILLER_BIN --list --pid "$2"
            fi
            ;;
        --interactive)
            interactive_mode
            ;;
        --help)
            echo "Использование: $0 [ОПЦИИ]"
            echo ""
            echo "Опции:"
            echo "  --list [--pid PID] [--port PORT] [--state STATE]    Показать соединения"
            echo "  --watch [--pid PID] [--port PORT] [--state STATE]   Обновлять в реальном времени"
            echo "  --kill LOCAL_IP:PORT REMOTE_IP REMOTE_PORT           Убить соединение"
            echo "  --pid PID --remote IP:PORT                           Убить по PID и удалённому адресу"
            echo "  --interactive                                        Интерактивный режим"
            echo "  --help                                               Эта справка"
            ;;
        *)
            echo -e "${RED}Неизвестная опция: $1${NC}"
            $0 --help
            exit 1
            ;;
    esac
}

main "$@"
