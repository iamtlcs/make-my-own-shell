#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 20

extern char **environ; // global variable that holds the environment

void remove_newline(char *str) {
	size_t len = strcspn(str, "\n");
	if (*(str + len) == '\n') {
		*(str + len) = '\0';
	}
}

const char *shell_name = "Simonell";

int main() {
	char *line = NULL;
	size_t len = 0;
	ssize_t read_bytes;

	char *args[MAX_ARGS + 1];
	int i;

	char *input_file = NULL;
	char *output_file = NULL;
	int output_append = 0;

	printf("--- Simon's Simple Shell - %s ---\n", shell_name);
	printf("Type a command (e.g., ls -l /tmp). Type 'exit' to quit.\n");
	printf("WARNING: Currently only support \"last one win\" redirection.");

	while (1) {
		printf("%s>", shell_name);

		read_bytes = getline(&line, &len, stdin);

		if (read_bytes == -1) {
			printf("\nExiting %s.\n", shell_name);
			break;
		}

		remove_newline(line);

		if (strcmp(line, "exit") == 0) {
			printf("Exiting %s.\n", shell_name);
			break;
		}

		input_file = NULL;
		output_file = NULL;
		output_append = 0;

		char *copy_line = NULL;
		copy_line = (char *)malloc(strlen(line) + 1);

		if (copy_line == NULL) {
			perror("malloc failed for copy_line");
			free(line);
			line = NULL;
			exit(EXIT_FAILURE);
		}
		strcpy(copy_line, line);

		char *token;
		int arg_idx = 0;
		char *rest_of_line = copy_line;

		while((token = strtok(rest_of_line, " ")) != NULL) {
			rest_of_line = NULL; // Subsequent calls

			if (strcmp(token, "<") == 0) {
				token = strtok(rest_of_line, " ");
				if (token == NULL) {
					fprintf(stderr, "Syntax error: missing input file.\n");
					free(copy_line);
					continue;
				}
				input_file = token;
			} else if (strcmp(token, ">") == 0) {
				token = strtok(rest_of_line, " ");
				if (token == NULL) {
					fprintf(stderr, "Syntax error: missing output file.\n");
					free(copy_line);
					continue;
				}
				output_file = token;
			} else if (strcmp(token, ">>") == 0) {
				token = strtok(rest_of_line, " ");
				if (token = NULL) {
					fprintf(stderr, "Syntax error: missing file for appending.\n");
					free(copy_line);
					continue;
				}
				output_append = 1;
				output_file = token;
			} else {
				if (arg_idx < MAX_ARGS) {
					args[arg_idx++] = token;
				} else {
					fprintf(stderr, "Too many arguments (max %d)\n", MAX_ARGS);
					free(copy_line);
					continue;
				}
			}
		}

		args[arg_idx] = NULL;

		if (args[0] == NULL) {
			fprintf(stderr, "No command enetered.\n");
			free(copy_line);
			continue;
		}


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

		if (strcmp(args[0], "setenv") == 0) {
			if (args[1] == NULL) {
				printf("Usage: setenv NAME=VALUE");
			} else {
				char *name_value_pair = args[1];
				char *equal_sign = strchr(name_value_pair, '=');
				if (equal_sign == NULL) {
					printf("Usage: setenv NAME=VALUE");
				} else {
					*equal_sign = '\0';
					char *name = name_value_pair;
					char *value = equal_sign + 1;

					if (setenv(name, value, 1) == 0) { // overwrite if exists
						printf("Environment variable '%s' set to '%s'.\n", name, value);
					} else {
						perror("setenv failed");
					}
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
			if (input_file != NULL) {
				int fd_in = open(input_file, O_RDONLY);
				if (fd_in == -1) {
					perror("Failed to open input file");
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}
				if (dup2(fd_in, STDIN_FILENO) == -1) { // redirect stdin (fd 0)
					perror("Failed to redirect stdin");
					close(fd_in);
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}
				close(fd_in);
			}
			
			if (output_file != NULL) {
				int flags = O_WRONLY | O_CREAT; // Write-only, Create if not exists
				if (output_append) {
					flags |= O_APPEND;
				} else {
					flags |= O_TRUNC;
				}

				int fd_out = open(output_file, flags, 0644); // rw-r--r--
				if (fd_out == -1) {
					perror("Failed to ope output file");
					close(fd_out);
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}
				if (dup2(fd_out, STDOUT_FILENO) == -1) {
					perror("Failed to redirect stdout");
					close(fd_out);
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}
				close(fd_out);
			}

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
