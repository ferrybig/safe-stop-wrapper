#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

volatile sig_atomic_t signal_term = false;
volatile sig_atomic_t signal_chld = true;
void handle_signal(int sig) {
	if(sig == SIGTERM || sig == SIGINT) {
		signal_term = true;
	} else if (sig == SIGCHLD) {
		signal_chld = true;
	} else if (sig == SIGPIPE) {
		// Ignore SIGPIPE to prevent termination when writing to a closed pipe
	} else {
		fprintf(stderr, "Unknown signal: %d\n", sig);
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
		return EXIT_FAILURE;
	}

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (false
		|| sigaction(SIGTERM, &sa, NULL) == -1
		|| sigaction(SIGINT, &sa, NULL) == -1
		|| sigaction(SIGCHLD, &sa, NULL) == -1
		|| sigaction(SIGPIPE, &sa, NULL) == -1
	) {
		perror("sigaction");
		return EXIT_FAILURE;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		// Start the child process
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);

		// If this variable is true, it means that we got a sigterm before we forked. This means a shutdown was requested and we can exit immediately.
		if (signal_term) {
			return EXIT_SUCCESS;
		}

		execvp(argv[1], &argv[1]);
		perror("execvp");
		return EXIT_FAILURE;
	}

	close(pipefd[0]);

	// We set a small buffer, this might be a problem for performance, but there rarely is a lot of data to send to the child process.
	char buffer[128];
	ssize_t bytesRead = 0;
	int bytesOffset = 0;
	bool child_stop_send = false;
	bool read_stopped = false;
	bool ended_with_newline = true;

	while (true) {
		//fprintf(stderr, "debug: bytesRead=%zd, bytesOffset=%d, read_stopped=%d, child_stop_send=%d, signal_term=%d, signal_chld=%d, ended_with_newline=%d\n",
		//	bytesRead, bytesOffset, read_stopped, child_stop_send, signal_term, signal_chld, ended_with_newline
		//);
		if (bytesRead == 0 && signal_term && !child_stop_send) {
			const char *stopMessage = "\nstop\n";
			int length = strlen(stopMessage);
			memmove(&buffer, stopMessage, length);
			bytesRead = length;
			bytesOffset = ended_with_newline ? 1 : 0;
			child_stop_send = true;
		} else if (signal_chld || (read_stopped && bytesRead == 0)) {
			// Wait for a child to terminate
			signal_chld = false;
			int status;
			pid_t w_pid = waitpid(-1, &status, read_stopped ? 0 : WNOHANG);
			if (w_pid == -1) {
				if (errno == EINTR) continue;
				perror("waitpid");
			} else if (w_pid == pid) {
				//if (WIFEXITED(status)) {
				//	fprintf(stderr, "Child exited with status %d\n", WEXITSTATUS(status));
				//} else if (WIFSIGNALED(status)) {
				//	fprintf(stderr, "Child killed by signal %d\n", WTERMSIG(status));
				//} else if (WIFSTOPPED(status)) {
				//	fprintf(stderr, "Child stopped by signal %d\n", WSTOPSIG(status));
				//}
				return WEXITSTATUS(status); // Return the exit status of the child process
			} else if (w_pid != 0) {
				// Set it back to true in the case we have more than one child that exited
				signal_chld = true;

			}
		} else if (bytesRead == 0) {
			bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer));
			if (bytesRead == -1) {
				bytesRead = 0;
				if (errno == EINTR) continue;
				perror("read");
			} else if (bytesRead == 0) {
				read_stopped = true;
				close(STDIN_FILENO);
				if (child_stop_send) {
					close(pipefd[1]);
				}
			}
		} else {
			int bytesWritten = write(pipefd[1], buffer + bytesOffset, bytesRead - bytesOffset);
			if (bytesWritten == -1) {
				if (errno == EINTR) continue;
				perror("write");
				break;
			} else {
				bytesOffset += bytesWritten;
				if (bytesOffset >= bytesRead) {
					ended_with_newline = buffer[bytesRead - 1] == '\n';
					bytesOffset = 0;
					bytesRead = 0;
					if (child_stop_send && read_stopped) {
						close(pipefd[1]);
					}
				}
			}
		}
	}
}