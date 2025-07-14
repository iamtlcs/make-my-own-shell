#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void remove_newline(char *str) {
	size_t len = strcspn(str, "\n");
	if (*(str + len) == '\n') {
		*(str + len) = '\0';
	}
}

int main() {
	char *line = NULL;
	size_t len = 0;
	ssize_t read_bytes;

	char *token;
	char *copy_line = NULL;

	printf("--- Simple Shell Command Reader & Parser ---\n");
	printf("Type a command (e.g., ls -l /tmp) and press Enter. Type 'exit' to quit.\n");

	while (1) {
		printf("my_shell>");

		read_bytes = getline(&line, &len, stdin);

		if (read_bytes == -1) {
			printf("\nExiting my_shell.\n");
			break;
		}

		remove_newline(line);

		if (strcmp(line, "exit") == 0) {
			printf("Exiting my_shell.\n");
			break;
		}
		
		copy_line = (char *)malloc(strlen(line) + 1);

		if (copy_line == NULL) {
			perror("malloc failed for copy_line");
			free(line);
			line = NULL;
			exit(EXIT_FAILURE);
		}
		strcpy(copy_line, line);

		printf("Original line: \"%s\"\n", line);
		printf("Tokenisation: \n");

		token = strtok(copy_line, " ");

		int token_count = 0;
		while (token != NULL) {
			printf("\tToken %d: \"%s\"\n", token_count, token);
			token_count++;
			token = strtok(NULL, " "); // retrieves its previously saved internal static pointer
		}

		free(copy_line);
		copy_line = NULL;
	}

	free(line);
	line = NULL;

	return 0;
}

