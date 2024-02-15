SOURCE=major.c regex.c history.c
EXEC=wish
target:
	gcc $(SOURCE) -g -o $(EXEC)
address:
	gcc -fsanitize=address $(SOURCE) -lasan -o $(EXEC)
leak:
	gcc -fsanitize=leak $(SOURCE) -llsan -o $(EXEC)
