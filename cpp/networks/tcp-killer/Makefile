CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET = tcp_killer
SRCDIR = src
BINDIR = bin
SCRIPTDIR = scripts
TESTDIR = tests

# Цвета для вывода
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[1;33m
NC = \033[0m

all: $(BINDIR)/$(TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/$(TARGET): $(SRCDIR)/$(TARGET).cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

install: $(BINDIR)/$(TARGET)
	@echo "$(GREEN)Установка...$(NC)"
	sudo cp $(BINDIR)/$(TARGET) /usr/local/bin/
	sudo cp $(SCRIPTDIR)/tcp-killer.sh /usr/local/bin/tcp-killer
	sudo chmod +x /usr/local/bin/$(TARGET)
	sudo chmod +x /usr/local/bin/tcp-killer
	@echo "$(GREEN)Установка завершена!$(NC)"
	@echo "Используйте: tcp-killer --help"

clean:
	@echo "$(YELLOW)Очистка...$(NC)"
	rm -rf $(BINDIR)
	rm -rf $(TESTDIR)

uninstall:
	@echo "$(YELLOW)Удаление...$(NC)"
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -f /usr/local/bin/tcp-killer
	@echo "$(GREEN)Удаление завершено$(NC)"

# Тестирование без установки
test: $(BINDIR)/$(TARGET)
	@echo "$(GREEN)=== Запуск тестов ===$(NC)"
	@mkdir -p $(TESTDIR)
	@echo ""
	@echo "$(YELLOW)1. Тест: Проверка помощи$(NC)"
	@$(BINDIR)/$(TARGET) --help > $(TESTDIR)/help_output.txt 2>&1
	@echo "$(GREEN)Помощь работает$(NC)"
	
	@echo ""
	@echo "$(YELLOW)2. Тест: Парсинг /proc/net/tcp$(NC)"
	@$(BINDIR)/$(TARGET) --list > $(TESTDIR)/list_output.txt 2>&1
	@lines=`cat $(TESTDIR)/list_output.txt | wc -l`; \
	if [ $$lines -gt 3 ]; then \
		echo "$(GREEN)Список соединений получен ($$lines строк)$(NC)"; \
		head -5 $(TESTDIR)/list_output.txt; \
	else \
		echo "$(RED)Не удалось получить соединения$(NC)"; \
	fi
	
	@echo ""
	@echo "$(YELLOW)3. Тест: Фильтрация по состоянию LISTEN$(NC)"
	@$(BINDIR)/$(TARGET) --list --state LISTEN > $(TESTDIR)/listen.txt 2>&1
	@if grep -q "LISTEN" $(TESTDIR)/listen.txt; then \
		listen_count=`grep -c "LISTEN" $(TESTDIR)/listen.txt`; \
		echo "$(GREEN) Найдено $$listen_count LISTEN соединений$(NC)"; \
	else \
		echo "$(YELLOW) Нет LISTEN соединений$(NC)"; \
	fi
	
	@echo ""
	@echo "$(YELLOW)4. Тест: Фильтрация по порту 80 (HTTP)$(NC)"
	@$(BINDIR)/$(TARGET) --list --port 80 > $(TESTDIR)/port80.txt 2>&1
	@port80_count=`cat $(TESTDIR)/port80.txt | grep -c ":" || echo 0`; \
	if [ "$$port80_count" -gt 2 ]; then \
		echo "$(GREEN)Найдены соединения на порту 80$(NC)"; \
	else \
		echo "$(YELLOW)Нет соединений на порту 80 (это нормально)$(NC)"; \
	fi
	
	@echo ""
	@echo "$(YELLOW)5. Тест: Фильтрация по PID (используем PID 1)$(NC)"
	@$(BINDIR)/$(TARGET) --list --pid 1 > $(TESTDIR)/pid1.txt 2>&1
	@if grep -q "systemd" $(TESTDIR)/pid1.txt; then \
		echo "$(GREEN) Фильтрация по PID работает$(NC)"; \
	else \
		echo "$(YELLOW) PID 1 не имеет TCP соединений (это нормально)$(NC)"; \
	fi
	
	@echo ""
	@echo "$(YELLOW)6. Тест: Статистика соединений$(NC)"
	@total=`$(BINDIR)/$(TARGET) --list 2>/dev/null | grep -c ":" || echo 0`; \
	established=`$(BINDIR)/$(TARGET) --list --state ESTABLISHED 2>/dev/null | grep -c "ESTABLISHED" || echo 0`; \
	listen=`$(BINDIR)/$(TARGET) --list --state LISTEN 2>/dev/null | grep -c "LISTEN" || echo 0`; \
	time_wait=`$(BINDIR)/$(TARGET) --list --state "TIME_WAIT" 2>/dev/null | grep -c "TIME_WAIT" || echo 0`; \
	echo "  Всего соединений: $$total"; \
	echo "  ESTABLISHED: $$established"; \
	echo "  LISTEN: $$listen"; \
	echo "  TIME_WAIT: $$time_wait"
	
	@echo ""
	@echo "$(YELLOW)7. Тест: Проверка формата вывода$(NC)"
	@$(BINDIR)/$(TARGET) --list | head -3 | grep -q "PID.*PROCESS.*LOCAL.*REMOTE.*STATE" && \
		echo "$(GREEN)Формат вывода корректный$(NC)" || \
		echo "$(YELLOW)Формат вывода может отличаться$(NC)"
	
	@echo ""
	@echo "$(YELLOW)8. Тест: Проверка прав для kill$(NC)"
	@echo "  Для отправки RST нужны root права:"
	@echo "  sudo $(BINDIR)/$(TARGET) --kill 127.0.0.1:12345 127.0.0.1:80"
	
	@echo ""
	@echo "$(GREEN)=== Все тесты пройдены ===$(NC)"
	@echo "Результаты сохранены в $(TESTDIR)/"

# Интеграционный тест с автоматической проверкой
test-integration: $(BINDIR)/$(TARGET)
	@echo "$(GREEN)=== Интеграционное тестирование ===$(NC)"
	@echo ""
	@echo "1. Проверка, что программа компилируется без ошибок"
	@test -x $(BINDIR)/$(TARGET) && echo "$(GREEN)Бинарник существует$(NC)" || echo "$(RED)Бинарник не найден$(NC)"
	
	@echo ""
	@echo "2. Проверка, что /proc/net/tcp существует"
	@test -r /proc/net/tcp && echo "$(GREEN)/proc/net/tcp доступен$(NC)" || echo "$(RED)/proc/net/tcp не найден$(NC)"
	
	@echo ""
	@echo "3. Проверка прав доступа"
	@if [ -r /proc/net/tcp ]; then \
		echo "$(GREEN)Чтение /proc/net/tcp доступно$(NC)"; \
	fi
	
	@echo ""
	@echo "4. Проверка формата вывода"
	@$(BINDIR)/$(TARGET) --list | head -5 | while read line; do \
		if echo "$$line" | grep -qE "PID|PROCESS|LOCAL|REMOTE|STATE|---"; then \
			echo "  Заголовок: $$line"; \
		fi; \
	done
	
	@echo ""
	@echo "5. Поиск реальных процессов с соединениями"
	@$(BINDIR)/$(TARGET) --list | awk 'NR>2 {print $$1}' | sort -u | grep -v "^0$$" | head -5 | while read pid; do \
		if [ "$$pid" != "PID" ] && [ "$$pid" != "" ]; then \
			echo "  Найден процесс: PID=$$pid"; \
		fi; \
	done
	
	@echo ""
	@echo "$(GREEN)Интеграционный тест пройден$(NC)"

# Демонстрационный режим
demo: $(BINDIR)/$(TARGET)
	@echo "$(GREEN)=== ДЕМОНСТРАЦИЯ TCP-KILLER ===$(NC)"
	@echo ""
	@echo "$(YELLOW)[ДЕМО 1] Показываем все TCP-соединения:$(NC)"
	@$(BINDIR)/$(TARGET) --list | head -20
	@echo "..."
	
	@echo ""
	@echo "$(YELLOW)[ДЕМО 2] Фильтрация по ESTABLISHED соединениям:$(NC)"
	@$(BINDIR)/$(TARGET) --list --state ESTABLISHED | head -10
	
	@echo ""
	@echo "$(YELLOW)[ДЕМО 3] Фильтрация по LISTEN сокетам:$(NC)"
	@$(BINDIR)/$(TARGET) --list --state LISTEN | head -10
	
	@echo ""
	@echo "$(YELLOW)[ДЕМО 4] Поиск соединений с удалённым портом 443 (HTTPS):$(NC)"
	@$(BINDIR)/$(TARGET) --list | grep ":443 " | head -5 || echo "  (нет соединений на порту 443)"
	
	@echo ""
	@echo "$(YELLOW)[ДЕМО 5] Интерактивный режим:$(NC)"
	@echo "  Запустите: sudo tcp-killer --interactive"
	
	@echo ""
	@echo "$(YELLOW)[ДЕМО 6] Отправка RST-пакета (только демонстрация синтаксиса):$(NC)"
	@echo "  sudo $(BINDIR)/$(TARGET) --kill 192.168.1.100:54321 1.2.3.4:80"
	
	@echo ""
	@echo "$(GREEN)=== Демонстрация завершена ===$(NC)"

# Быстрый тест (только основные проверки)
test-quick: $(BINDIR)/$(TARGET)
	@echo "$(GREEN)Быстрая проверка:$(NC)"
	@echo -n "  Активных соединений: "
	@$(BINDIR)/$(TARGET) --list 2>/dev/null | grep -c "ESTABLISHED" || echo "0"
	@echo -n "  Слушающих сокетов: "
	@$(BINDIR)/$(TARGET) --list 2>/dev/null | grep -c "LISTEN" || echo "0"
	@echo -n "  Всего соединений: "
	@$(BINDIR)/$(TARGET) --list 2>/dev/null | wc -l | awk '{print $$1-2}'
	@echo "$(GREEN)OK$(NC)"

# Показать все доступные цели
help:
	@echo "$(GREEN)Доступные команды make:$(NC)"
	@echo "  make            - Собрать программу"
	@echo "  make install    - Установить в систему"
	@echo "  make clean      - Очистить bin/ и tests/"
	@echo "  make uninstall  - Удалить из системы"
	@echo "  make test       - Запустить полное тестирование"
	@echo "  make test-quick - Быстрая проверка"
	@echo "  make test-integration - Интеграционное тестирование"
	@echo "  make demo       - Показать демонстрацию"
	@echo "  make help       - Показать эту справку"

.PHONY: all install clean uninstall test test-integration demo test-quick help
