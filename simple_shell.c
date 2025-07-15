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

	char *cmd1_args[MAX_ARGS + 1];
	char *cmd2_args[MAX_ARGS + 1];
	int is_piped = 0;

	printf("--- Simon's Simple Shell - %s ---\n", shell_name);
	printf("Type a command (e.g., ls -l /tmp). Type 'exit' to quit.\n");
	printf("WARNING: Currently only support \"last one win\" redirection.\n");

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
		is_piped = 0;

		for (int i = 0; i < MAX_ARGS + 1; i++) {
			cmd1_args[i] = NULL;
			cmd2_args[i] = NULL;
		}

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
		int current_cmd = 1; // 1 for cmd1, 2 for cmd2

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
				if (token == NULL) {
					fprintf(stderr, "Syntax error: missing file for appending.\n");
					free(copy_line);
					continue;
				}
				output_append = 1;
				output_file = token;
			} else if (strcmp(token, "|") == 0) {
				current_cmd = 2;
				is_piped = 1;
				arg_idx = 0;

			} else {
				if (current_cmd == 1) {
					if (arg_idx < MAX_ARGS) {
						cmd1_args[arg_idx] = token;
						arg_idx++;
					} else {
						fprintf(stderr, "Too many arguments (max %d) for cmd1\n", MAX_ARGS);
						free(copy_line); goto next_command_prompt;
					}
				} else {
					if (arg_idx < MAX_ARGS) {
						cmd2_args[arg_idx] = token;
						arg_idx++;
					} else {
						fprintf(stderr, "Too many arguments (max %d) for cmd2\n", MAX_ARGS);
						free(copy_line); goto next_command_prompt;
					}
				}
			}
		}

		if (cmd1_args[0] == NULL) {
			fprintf(stderr, "No command entered\n");
			free(copy_line);
			continue;
		}

		if (cmd2_args[0] == NULL && is_piped == 1) {
			fprintf(stderr, "Syntax error: missing command after pipe\n");
			free(copy_line);
			continue;
		}

		if (strcmp(cmd1_args[0], "printenv") == 0) {
			if (cmd1_args[1] == NULL) {
				char **env_var = environ;
				for (; *env_var != NULL; env_var++) {
					printf("%s\n", *env_var);
				}
			} else {
				char *val = getenv(cmd1_args[1]);
				if (val != NULL) {
					printf("%s=%s\n", cmd1_args[1], val);
				} else {
					printf("%s: environment variable not found\n", cmd1_args[1]);
				}
			}
			free(copy_line);
			continue;
		}

		if (strcmp(cmd1_args[0], "setenv") == 0) {
			if (cmd1_args[1] == NULL) {
				printf("Usage: setenv NAME=VALUE");
			} else {
				char *name_value_pair = cmd1_args[1];
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

		if (is_piped) {
			int pipefd[2]; // first one for read, second one for write
			pid_t pid1, pid2;

			if (pipe(pipefd) == -1) {
				perror("pipe failed");
				free(copy_line);
				continue;
			}

			// Fork cmd1
			pid1 = fork();

			if (pid1 == -1) {
				perror("fork failed for cmd1");
				close(pipefd[0]);
				close(pipefd[1]);
				free(copy_line);
				continue;
			} else if (pid1 == 0) {
				// Child process for cmd1
				close(pipefd[0]);

				if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
					perror("Failed to redirect stdout to pipe");
					close(pipefd[1]);
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}

				close(pipefd[1]);

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

				execvp(cmd1_args[0], cmd1_args);
				perror("execvp failed for cmd1");
				free(copy_line);
				free(line);
				exit(EXIT_FAILURE);
			}

			// Fork cmd2
			pid2 = fork();

			if (pid2 == -1) {
				perror("fork failed for cmd2");
				close(pipefd[0]);
				close(pipefd[1]);
				free(copy_line);
				continue;
				waitpid(pid1, NULL, 0);
				continue;
			} else if (pid2 == 0) {
				// Child process for cmd2
				close(pipefd[1]);

				if (dup2(pipefd[0], STDIN_FILENO) == -1) {
					perror("Failed to redirect stdin to pipe");
					close(pipefd[0]);
					free(copy_line);
					free(line);
					exit(EXIT_FAILURE);
				}

				close(pipefd[0]);

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


				execvp(cmd2_args[0], cmd2_args); // args is NULL-terminated and the first element can be included.
				// non-return on success and overrides the current program

				perror("execvp failed for cmd2");
				free(copy_line);
				free(line);
				exit(EXIT_FAILURE);
			}

			close(pipefd[0]);
			close(pipefd[1]);

			waitpid(pid1, NULL, 0);
			waitpid(pid2, NULL, 0);
			free(copy_line);
		} else {
			pid_t pid = fork();

			if (pid == 1) {
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

				execvp(cmd1_args[0], cmd1_args);
				perror("execvp failed for cmd1");
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
	}
	next_command_prompt:;
	free(line);
	line = NULL;
	return 0;
}
