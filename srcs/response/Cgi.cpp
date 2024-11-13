/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/06/25 12:16:01 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 13:35:32 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <sstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <iostream>
#include <map>
#include <cstring>
#include <signal.h>
#include <fcntl.h>
#include <ctime>

static void	CgiError(Request& req, Response& res, int errorCode, std::string errorString)
{
	int	client_fd = req._server->getClientFd();
	std::map<int, std::string> _req = req._server->getRequests();
	res.errorResponse(errorCode, errorString, req._server->getConfig().error_page_paths);
	res.generateResponse();
	_req.erase(client_fd);
	_req.insert(std::make_pair(client_fd, res.getResponse()));
	req._server->setRequests(_req);
	req._server->sendResponse(client_fd);
	_req.clear();
}

static std::string intToString(int number) {
	std::ostringstream oss;
	oss << number;
	return oss.str();
}

void Response::handleCGI(std::string rootedpath, std::string path, Request& req, std::string exec_path, Response& res) {
	struct stat sb;
	std::cout << "CGI processing started" << std::endl;
	if (stat(exec_path.c_str(), &sb) == 0 && access(exec_path.c_str(), X_OK) == 0) {
		if (stat(rootedpath.c_str(), &sb) == 0 && access(rootedpath.c_str(), X_OK) == 0) {
			int pipefd[2];
			int output_pipe[2];
			if (pipe(pipefd) == -1 || pipe(output_pipe) == -1) {
				std::cerr << "ERROR: CGI: pipe() failed" << std::endl;
				CgiError(req, res, 502, "CGI pipe() error");
				return;
			}
			pid_t p = fork();
			int client_fd = req._server->getClientFd();
			if (p == 0) { // Child process
				close(pipefd[1]); // Close write end of the input pipe
				close(output_pipe[0]); // Close read end of the output pipe

				if (dup2(pipefd[0], STDIN_FILENO) == -1) {
					std::cerr << "ERROR: CGI: dup2() failed" << std::endl;
					close(pipefd[0]);
					close(output_pipe[1]);
				}
				if (dup2(output_pipe[1], STDOUT_FILENO) == -1) {
					std::cerr << "ERROR: CGI: dup2() failed" << std::endl;
					close(pipefd[0]);
					close(output_pipe[1]);
				}
				if (dup2(output_pipe[1], STDERR_FILENO) == -1) {
					std::cerr << "ERROR: CGI: dup2() failed" << std::endl;
					close(pipefd[0]);
					close(output_pipe[1]);
				}

				std::string path_info = "PATH_INFO=" + path;
				std::string path_translated = "PATH_TRANSLATED=" + rootedpath;
				std::string query_string = "QUERY_STRING=" + req.getQuerystr();
				std::string content_length = "CONTENT_LENGTH=" + intToString(req.getLen());
				std::string content_type = "CONTENT_TYPE=" + req.getHeader("Content-Type");
				std::string remote_host = "REMOTE_HOST=" + intToString((int)req._server->getClientAddr().sin_port);
				std::string remote_addr = "REMOTE_ADDR=" + intToString((int)req._server->getClientAddr().sin_addr.s_addr);
				std::string request_method = "REQUEST_METHOD=" + req.getRequestMethod();
				std::string server_name = "SERVER_NAME=" + req.getHost();
				std::string server_port = "SERVER_PORT=" + intToString(req._server->getConfig().port);
				std::string server_protocol = "SERVER_PROTOCOL=HTTP/" + req.getVersion();
				std::string upload_path = "UPLOAD_PATH=/" + req._server->getConfig().upload_path;
				char *envp[] = {
					ft_strdup(path_info.c_str()),
					ft_strdup(path_translated.c_str()),
					ft_strdup(query_string.c_str()),
					ft_strdup(content_length.c_str()),
					ft_strdup(content_type.c_str()),
					ft_strdup(remote_addr.c_str()),
					ft_strdup(remote_host.c_str()),
					ft_strdup(request_method.c_str()),
					ft_strdup(server_name.c_str()),
					ft_strdup(server_port.c_str()),
					ft_strdup(server_protocol.c_str()),
					ft_strdup(upload_path.c_str()),
					NULL
				};

				char *argv[] = {
					ft_strdup(exec_path.c_str()),
					ft_strdup(path.substr(path.find_last_of("/") + 1, path.length()).c_str()),
					NULL
				};
				std::string script_dir = rootedpath.substr(0, rootedpath.find_last_of("/"));
				if (chdir(script_dir.c_str()) == -1) {
					std::cerr << "ERROR: CGI: chdir() failed" << std::endl;
				}
				if (execve(exec_path.c_str(), argv, envp) == -1) {
					std::cerr << "ERROR: CGI: Failed to execute CGI script" << std::endl;
				}

				// Free dynamically allocated memory
				for (size_t i = 0; envp[i]; ++i) {
					free(envp[i]);
				}
				for (size_t i = 0; argv[i]; ++i) {
					free(argv[i]);
				}
				close(pipefd[0]);
				close(output_pipe[1]);
			} else if (p == -1) { // Fork failed
				std::cerr << "ERROR: CGI: fork() failed to open a new process" << std::endl;
				close(pipefd[0]);
				close(pipefd[1]);
				close(output_pipe[0]);
				close(output_pipe[1]);
				CgiError(req, res, 502, "CGI fork() error");
				return;
			} else { // Parent process
				close(pipefd[0]); // Close read end of the input pipe
				close(output_pipe[1]); // Close write end of the output pipe

				// Write the request body to the input pipe
				if (req.getRequestMethod() == "POST") {
					if (write(pipefd[1], req.getBody().c_str(), req.getBody().size()) == -1) {
						std::cerr << "ERROR: CGI: write() failed" << std::endl;
						close(pipefd[1]);
						close(output_pipe[0]);
						CgiError(req, res, 502, "CGI write() error");
						return;
					}
				}

				close(pipefd[1]); // Close the write end of the input pipe
				std::time_t seconds = std::time(nullptr);
				while (seconds + 2 > std::time(nullptr))
					continue ;
				kill(p, SIGKILL);
				// Wait for the child process to finish
				int status;
				pid_t result = waitpid(p, &status, 0);
				if (result == -1) {
					std::cerr << "ERROR: CGI: waitpid() failed" << std::endl;
					close(output_pipe[0]);
					CgiError(req, res, 502, "CGI waitpid() error");
					return;
				}

				bool hasError = false;
				// Check the status of the child process
				if (WIFEXITED(status)) {
					int exitStatus = WEXITSTATUS(status);
					if (exitStatus != 0) {
						std::cerr << "ERROR: CGI script exited with status " << exitStatus << std::endl;
						hasError = true;
					}
				} else if (WIFSIGNALED(status)) {
					int signalNumber = WTERMSIG(status);
					std::cerr << "ERROR: CGI script killed by signal " << signalNumber << std::endl;
					hasError = true;
				} else if (WIFSTOPPED(status)) {
					int stopSignal = WSTOPSIG(status);
					std::cerr << "ERROR: CGI script stopped by signal " << stopSignal << std::endl;
					hasError = true;
				}

				// Read the output from the CGI script to clear the buffer
				char buffer[1024];
				ssize_t bytesRead;
				while ((bytesRead = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
					if (!hasError) {
						if (write(client_fd, buffer, bytesRead) == -1) {
							std::cerr << "ERROR: CGI: write() to client failed" << std::endl;
							close(output_pipe[0]);
							CgiError(req, res, 502, "CGI write() to client error");
							return;
						}
					}
				}

				if (bytesRead == -1) {
					std::cerr << "ERROR: CGI: read() failed" << std::endl;
					close(output_pipe[0]);
					CgiError(req, res, 502, "CGI read() error");
					return;
				}

				close(output_pipe[0]); // Close the read end of the output pipe

				if (hasError) {
					CgiError(req, res, 502, "CGI script error");
					return;
				}
			}
			std::cout << "CGI processing finished" << std::endl;
		} else {
			std::cerr << "ERROR: CGI: Path to " << rootedpath << " executable is not accessible." << std::endl;
			CgiError(req, res, 502, "CGI executable path not accessible");
		}
	} else {
		std::cerr << "ERROR: CGI: Path to " << exec_path << " executable is not accessible." << std::endl;
		CgiError(req, res, 502, "CGI executable path not accessible");
	}
}

