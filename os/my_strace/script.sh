clear
gcc test_code.c -o test_code
gcc main.c -o main
echo ==============================
echo test_code.c:
cat test_code.c
echo ==============================
echo main.c:
cat main.c
echo ==============================
gcc -o main main.c
echo ./main ./test_code:
valgrind --leak-check=full --show-leak-kinds=definite ./main ./test_code
