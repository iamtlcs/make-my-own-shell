#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 20

extern char **environ; // global variable that holds the program's environment

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

	char *args[MAX_ARGS + 1];
	int i;

	char *input_file = NULL;
	char *output_file = NULL;
	int output_append = 0;

	printf("--- Simon's Simple Shell ---\n");
	printf("Type a command (e.g., ls -l /tmp). Type 'exit' to quit.\n");

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

		input_file = NULL;
		output_file = NULL;
		output_append = NULL;

		char *copy_line = NULL;
		copy_line = (char *)malloc(strlen(line) + 1);

		if (copy_line == NULL) {
			perror("malloc failed for copy_line");
			free(line);
			line = NULL;
			exit(EXIT_FAILURE);
		}
		strcpy(copy_line, line);

		args[0] = strtok(copy_line, " ");
		printf("Token 0: \"%s\"\n", args[0]);

		int i = 1;
		while (args[i - 1] != NULL && i < MAX_ARGS) {
			args[i] = strtok(NULL, " "); // retrieves its previously saved internal static pointer
			printf("\tToken %d: \"%s\"\n", i, args[i]);
			i++;
		}
		args[i] = NULL;

		if (strcmp(args[0], "printenv") == 0) {
			if (args[1] == NULL) {
				char **env_var = environ;
				for (; *env_var != NULL; env_var++) {
					printf("%s\n", *env_var);
				}
			} else {
				char *val = getenv(args[1]);
				if (val != NULL) {
					printf("%s=%s\n", args[1], val);
				} else {
					printf("%s: environment variable not found\n", args[1]);
				}
			}
			free(copy_line);
			continue;
		}

		pid_t pid = fork();

		if (pid == -1) {
			perror("fork failed");
			free(copy_line);
			continue;
		} else if (pid == 0) {
			// Child process
			execvp(args[0], args); // args is NULL-terminated and the first element can be included.
			// non-return on success and overrides the current program
			
			perror("execvp failed");
			free(copy_line);
			free(line);
			exit(EXIT_FAILURE);
		} else {
			// Parent process
			int status;
			// Wait for the child process to complete
			if (waitpid(pid, &status, 0) == -1) {
				perror("waitpid failed");
			} else {
				// Check the child's exit status
				if (WIFEXITED(status)) {
					printf("Child exited with status %d\n", WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					printf("Command '%s' terminated by signal: %d\n", args[0], WTERMSIG(status));
				} else {
					printf("Child terminated abnormally\n");
				}
			}
			free(copy_line);
		}
	}
	free(line);
	line = NULL;
	return 0;
}
