/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/22 16:28:16 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 14:15:49 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"
#include "../parser/confParser.hpp"
#include "../request/Request.hpp"
#include "Router.hpp"
#include <string>
#include <map>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sys/types.h>
#include <dirent.h>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <iostream>

Response::Response(void){

	this->_status_code = -1;
	this->_content_len = -1;
	this->_cgi = false;
	this->_redir = false;
	this->_default_file = false;
	//std::cout << "Response created" << std::endl;
}

Response::~Response(void){

	_response.clear();
	_http_version.clear();
	_status_message.clear();
	_location.clear();
	_content_type.clear();
	_body.clear();
	//std::cout << "Response destroyed" << std::endl;
}

bool	Response::checkContentType(std::string path){

	std::ifstream input(path);

	if (input.is_open())
	{
		std::string buffer;
		std::string stack;

		while (std::getline(input, buffer))
		{
			stack += buffer;
			buffer.clear();
			stack += '\n';
		}
		input.close();
		std::string type = getContentType(stack);
		if (type == "text/html" || type == "text/plain" || type == "image/png" || type == "image/jpeg"
				|| type == "image/svg+xml" || type == "image/gif" || type == "application/pdf"
				|| type == "application/zip" || type == "video/mp4")
			return (true);
	}
	return (false);
}

void	Response::handleDirective(std::string path, t_locations& loc, Request& req, Server& serv){

	std::string	rooted_path;

	std::cout << "Response processing started" << std::endl;
	// Catch POST method early because it depends on upload_path, not uri (=path)
	if (uploadMethod(loc, path, serv.getConfig().upload_path, serv.getConfig().error_page_paths, req) == true)
		return;
	if (attachRootToPath(path, loc.root_path) == true)
	{
		struct stat buf;

		rooted_path = path;
		if (stat(rooted_path.c_str(), &buf) != 0)
			errorResponse(404, "Not Found : Stat Failed", serv.getConfig().error_page_paths);
		// Path points towards a directory
		else if (getFileType(buf) == E_D)
		{
			// If directory but non / terminating, redirect the uri with / terminating
			if (rooted_path[rooted_path.length() - 1] != '/')
			{
				setRedir(true);
				setLocation(req.getPathToFile().append("/"));
				generateResponse();
			}
			else if (isMethodAllowed(loc, req) == true)
			{
				// If default file found, return and recall the routine, If redirection found return and generate response
				if ((findDefaultFile(rooted_path, loc, serv.getConfig().locations, req, serv.getConfig().error_page_paths) == true || getRedir() == true))
					return;
				// Else lets see if autoindex is activated
				else
					runDirMethod(rooted_path, serv.getConfig().error_page_paths, loc, req);
			}
			else
				errorResponse(405, "Method Not Allowed : Directory", serv.getConfig().error_page_paths);
		}
		// Path points towards a file
		else if (getFileType(buf) == E_F)
			fileRoutine(rooted_path, serv.getConfig().error_page_paths, loc, req);
		// Do not know what type of file it is
		else
			errorResponse(415, "Unsupported Media Type : Not a directory nor a file", serv.getConfig().error_page_paths);
	}
	std::cout << "Response processing finished" << std::endl;
}

void	Response::cleanPath(std::string& str){

	if (str[0] == '/')
		str.erase(0, 1);
	if (str[str.length() - 1] == '/')
		str.erase(str.length() - 1, 1);
}

bool	Response::uploadMethod(t_locations loc, std::string& path, std::string upload_path, std::map<int, std::string> error_paths, Request& req){

	if (req.getRequestMethod() == "POST")
	{
		std::string type = getContentType(req.getBody());
		if (type == "text/html" || type == "text/plain" || type == "image/png" || type == "image/jpeg"
				|| type == "image/svg+xml" || type == "image/gif")
		{
			cleanPath(upload_path);
			if (isMethodAllowed(loc, req) == true)
			{
				const std::map<std::string, std::string>& cgi_path = req._server->getConfig().cgi_path;
				if (findCGI(cgi_path, req.getPathToFile()) == true)
				{
					std::map<std::string, std::string>::const_iterator it = cgi_path.begin();
					std::string _path = req.getPathToFile();
					while (it != cgi_path.end())
					{
						if (path.compare(_path.length() - it->first.length(), it->first.length(), it->first) == 0 && !it->second.empty())
						{
							if (attachRootToPath(path, loc.root_path) == true)
								handleCGI(path, req.getPathToFile(), req, it->second, *this);
						}
						++it;
					}
					this->_cgi = true;
				}
				else if (req.getMulti().empty() == false)
					uploadMultiformFile(upload_path, error_paths, req.getMulti());
				else if (req.getQuery_args().empty() == false)
					uploadQueryFile(upload_path, error_paths, req.getQuery_args(), req.getBody());
				else
					errorResponse(400, "Bad Request", error_paths);
			}
			else
				errorResponse(405, "Method Not Allowed : File", error_paths);
		}
		else
			errorResponse(415, "Unsupported Media Type : File", error_paths);
		return (true);
	}
	return (false);
}

bool	Response::attachRootToPath(std::string& path, std::string root){

	if (root.empty() == false)
	{
		cleanPath(root);
		if (path[0] != '/')
			path.insert(0, "/");
		path.insert(0, root);
		return (true);
	}
	return (fileNotFound(), false);
}

int	Response::getFileType(struct stat buf){

	if (S_ISDIR(buf.st_mode) == true)
		return (E_D);
	else if (S_ISREG(buf.st_mode) == true)
		return (E_F);
	return (E_U);
}

bool	Response::findDefaultFile(std::string& rooted_path, t_locations& loc, std::map<std::string, t_locations> routes, Request& req, std::map<int, std::string> error_paths){

	Router	router;

	if(loc.default_path.empty() == false)
	{
		for (int i = loc.default_path.size() - 1; i >= 0; i--)
		{
			struct stat	buf;
			std::string	tmp;
			std::string	rooted_default_path;

			tmp = rooted_path;
			if (tmp[tmp.length() - 1] != '/')
				tmp.append("/");
			rooted_default_path = tmp.append(loc.default_path[i]);
			if (stat(rooted_default_path.c_str(), &buf) == 0)
			{
				if (access(rooted_default_path.c_str(), R_OK) == 0)
				{
					std::string default_path;

					default_path = req.getPathToFile();
					if (default_path[default_path.length() - 1] != '/')
						default_path.append("/");
					default_path.append(loc.default_path[i]);
					req.setPathToFile(default_path);
					if (router.routeRequest(default_path, loc, routes, *this) == true)
					{
						if (getRedir() == true)
							generateResponse();
						else
						{
							this->_default_file = true;
							req.setPathToFile(default_path);
						}
					}
					else
						return (errorResponse(500, "Internal Server Error", error_paths), false);
				}
				else
					errorResponse(403, "Forbidden", error_paths);
				return (true);
			}
		}
	}
	return (false);
}

void	Response::fileRoutine(std::string rooted_path, std::map<int, std::string> error_paths, t_locations loc, Request& req){

	const std::map<std::string, std::string>& cgi_path = req._server->getConfig().cgi_path;
	if (findCGI(cgi_path, req.getPathToFile()) == true)
	{
		if (checkContentType(rooted_path) == true)
		{
			if (isMethodAllowed(loc, req) == true)
			{
				std::map<std::string, std::string>::const_iterator it = cgi_path.begin();
				std::string path = req.getPathToFile();
				while (it != cgi_path.end())
				{
					if (path.compare(path.length() - it->first.length(), it->first.length(), it->first) == 0 && !it->second.empty())
						handleCGI(rooted_path, req.getPathToFile(), req, it->second, *this);
					++it;
				}
				this->_cgi = true;
			}
			else
				errorResponse(405, "Method Not Allowed : File", error_paths);
		}
		else
			errorResponse(415, "Unsupported Media Type : File", error_paths);
	}
	else if (checkFileAccess(rooted_path, error_paths, "R") == true)
		runFileMethod(rooted_path, loc, error_paths, req);
}

bool	Response::findCGI(std::map<std::string, std::string> cgi_path, std::string path_to_file){

	if (cgi_path.empty() == false)
	{
		std::map<std::string, std::string>::iterator it = cgi_path.begin();
		while (it != cgi_path.end())
		{
			if (path_to_file.length() >= it->first.length() && path_to_file.compare(path_to_file.length() - it->first.length(), it->first.length(), it->first) == 0)
				return (true);
			it++;
		}
		return (false);
	}
	else
		return (false);
}

bool	Response::isMethodAllowed(t_locations loc, Request& req){

	for (size_t i = 0; i < loc.allowed_methods.size(); i++)
	{
		if (loc.allowed_methods[req.getRequestMethod()] == true)
			return (true);
	}
	return (false);
}

void	Response::runDirMethod(std::string rooted_path, std::map<int, std::string> error_paths, t_locations loc, Request& req){

	if (req.getRequestMethod() == "GET")
	{
		if (loc.auto_index == true)
			isAutoIndex(rooted_path, error_paths, loc, req);
		else
			errorResponse(403, "Forbidden", error_paths);
	}
	else
		errorResponse(405, "Method Not Allowed : Directory", error_paths);
}

void	Response::isAutoIndex(std::string rooted_path, std::map<int, std::string> error_paths, t_locations loc, Request& req){

	::DIR			*dr;
	struct dirent	*de;
	std::string		dir_list;

	if (checkFileAccess(rooted_path, error_paths, "R") == true)
	{
		if ((dr = opendir(rooted_path.c_str())) != NULL)
		{
			if (loc.auto_index == true)
			{
				while ((de = readdir(dr)) != NULL)
				{
					dir_list.append(de->d_name);
					dir_list += '\n';
				}
				autoIndexResponse(rooted_path, dir_list, req);
				closedir(dr);
			}
			else
			{
				closedir(dr);
				errorResponse(403, "Forbidden : Cannot access the directory", error_paths);
			}
		}
		else
			errorResponse(404, "Not Found : Opendir failed", error_paths);
	}
}

void	Response::runFileMethod(std::string rooted_path, t_locations loc, std::map<int, std::string> error_paths, Request& req){

	if (req.getRequestMethod() == "DELETE" && isMethodAllowed(loc, req) == true)
	{
		if (req.getPathToFile().find("upload", 0) != std::string::npos)
			deleteFile(rooted_path, error_paths);
		else
			errorResponse(400, "Bad Request : delete forbidden outside of upload path", error_paths);
	}
	else
	{
		if (checkContentType(rooted_path) == true)
		{
			if (isMethodAllowed(loc, req) == true)
			{
				if (req.getRequestMethod() == "GET")
					downloadFile(rooted_path, error_paths);
				else
					errorResponse(405, "Method Not Allowed : File", error_paths);
			}
			else
					errorResponse(405, "Method Not Allowed : File", error_paths);
		}
		else
			errorResponse(415, "Unsupported Media Type : File", error_paths);
	}
}

void	Response::downloadFile(std::string rooted_path, std::map<int, std::string> error_paths){

	std::ifstream input(rooted_path);

	if (input.is_open())
	{
		std::string buffer;
		std::string stack;

		while (std::getline(input, buffer))
		{
			stack += buffer;
			buffer.clear();
			if (!input.eof())
				stack += '\n';
		}
		input.close();
		downloadFileResponse(stack);
	}
	else
		errorResponse(404, "Not Found : Open input failed", error_paths);
}

void	Response::uploadQueryFile(std::string upload_path, std::map<int, std::string> error_paths, std::map<std::string, std::string> query, std::string body){

	for (std::map<std::string, std::string>::iterator it = query.begin(); it != query.end(); ++it)
	{
		if (checkFileAccess(upload_path, error_paths, "W") == true)
		{
			std::string whole_path;

			if (it->second[0] != '/')
				it->second.insert(0, "/");
			whole_path = upload_path + it->second;

			std::ofstream output(whole_path);

			if (output.is_open())
			{
				output << body;
				output.close();
				uploadFileResponse();
			}
			else
				errorResponse(404, "Not Found : Open output failed", error_paths);
		}

	}
}

void	Response::uploadMultiformFile(std::string upload_path, std::map<int, std::string> error_paths, std::map<int, t_multi> multiform){

	for (std::map<int, t_multi>::iterator it = multiform.begin(); it != multiform.end(); ++it)
	{
		if (checkFileAccess(upload_path, error_paths, "W") == true)
		{
			std::string whole_path;

			if (it->second.filename[0] != '/')
				it->second.filename.insert(0, "/");
			whole_path = upload_path + it->second.filename;

			std::ofstream output(whole_path);

			if (output.is_open())
			{
				output << it->second.content;
				output.close();
				uploadFileResponse();
			}
			else
				errorResponse(404, "Not Found : Open output failed", error_paths);
		}

	}
}

void	Response::deleteFile(std::string rooted_path, std::map<int, std::string> error_paths){

	if (std::remove(rooted_path.c_str()) < 0)
	{
		errorResponse(500, "Internal Server Error", error_paths);
		return;
	}
	deleteResponse();
}

void Response::autoIndexResponse(std::string rooted_path, std::string dir_list, Request& req) {

	size_t j = 0;
	std::vector<std::string> vec_dir_list;

	for (size_t i = 0; i < dir_list.length(); i++) {
		if (dir_list[i] == '\n') {
			vec_dir_list.push_back(dir_list.substr(j, i - j));
			j = i + 1;
		}
	}
	vec_dir_list.erase(vec_dir_list.begin());

	this->_body = "<!DOCTYPE html>\n"
				  "<html>\n"
				  "<head>\n"
				  "<title>Index of " + req.getPathToFile() + "</title>\n"
				  "<style>\n"
				  "body {\n"
				  "    font-family: Arial, sans-serif;\n"
				  "    margin: 0;\n"
				  "    padding: 20px;\n"
				  "}\n"
				  ".file-info {\n"
				  "    display: flex;\n"
				  "    justify-content: space-between;\n"
				  "    align-items: center;\n"
				  "    width: 100%;\n"
				  "    border: 1px solid black;\n"
				  "    padding: 10px;\n"
				  "    margin-bottom: 10px;\n"
				  "    box-sizing: border-box;\n"
				  "}\n"
				  ".file-info button {\n"
				  "    flex: 0 0 auto;\n"
				  "    margin-right: 10px;\n"
				  "}\n"
				  ".file-info .creation-date {\n"
				  "    flex: 1;\n"
				  "    text-align: center;\n"
				  "}\n"
				  ".file-info .char-count {\n"
				  "    flex: 0 0 auto;\n"
				  "    text-align: right;\n"
				  "    white-space: nowrap;\n"
				  "}\n"
				  "</style>\n"
				  "</head>\n"
				  "<body>\n"
				  "<h1>Index of " + req.getPathToFile() + "</h1>\n";

	for (std::vector<std::string>::iterator it = vec_dir_list.begin(); it != vec_dir_list.end(); ++it) {
		struct stat buf;
		std::string whole_path = rooted_path + "/" + *it;
		if (stat(whole_path.c_str(), &buf) != 0) {
			continue;
		}

		std::string unrooted_path;

		unrooted_path = req.getPathToFile();
		if (unrooted_path[unrooted_path.length() - 1] != '/')
			unrooted_path.append("/");

		std::string redirect_url = unrooted_path + *it;
		std::string txt_button = *it;
		std::string char_count = getCharCount(buf);

		insertHtmlIndexLine(redirect_url, txt_button, char_count);
	}

	this->_body += "</body>\n</html>";
	this->_status_code = 200;
	this->_status_message = "OK";
	this->_content_type = "text/html";
	this->_content_len = this->_body.length();
	vec_dir_list.clear();
	generateResponse();
}

void Response::insertHtmlIndexLine(std::string redirect_url, std::string txt_button, std::string char_count) {

	this->_body += "<div class=\"file-info\">\n"
				   "    <button onclick=\"window.location.href='" + redirect_url + "'\">" + txt_button + "</button>\n"
				   "    <div class=\"char-count\">" + char_count + "</div>\n"
				   "</div>\n";
}

std::string Response::getCharCount(struct stat buf) {

	if (S_ISDIR(buf.st_mode))
		return ("-");
	else if (S_ISREG(buf.st_mode))
		return (std::to_string(buf.st_size));
	return ("?");
}

void	Response::deleteResponse(void){

	this->_status_code = 200;
	this->_status_message = "OK";
	generateResponse();
}

void	Response::downloadFileResponse(std::string stack){

	this->_status_code = 200;
	this->_status_message = "OK";
	this->_content_type = getContentType(stack);
	this->_content_len = stack.length();
	this->_body = stack;
	generateResponse();
}

std::string	Response::getContentType(std::string stack){

	std::stringstream	ss;
	std::stringstream	ss_hex;
	std::vector<char>	bytes(8);
	std::string			octets;
	bool				text = true;

	ss << stack;
	ss.read(&bytes[0], bytes.size());
	for (size_t i = 0; i < bytes.size(); i++)
		ss_hex << std::hex << (static_cast<int>(bytes[i]) & 0xFF) << " ";
	octets = ss_hex.str();
	ss.clear();
	ss_hex.clear();
	bytes.clear();
	switch (stringToEnum(octets))
	{
		case E_PNG:
			return ("image/png");
		case E_JPEG:
			return ("image/jpeg");
		case E_SVG:
			return ("image/svg+xml");
		case E_GIF:
			return ("image/gif");
		case E_PDF:
			return ("application/pdf");
		case E_ZIP:
			return ("application/zip");
		case E_MP4:
			return ("video/mp4");
		default:
			break;
	}

	if (stack.compare(0, 15, "<!DOCTYPE html>") == 0)
		return ("text/html");

	for (size_t i = 0; i < stack.length(); i++)
	{
		if (!isprint(stack[i]) && !isspace(stack[i]))
		{
			text = false;
			break;
		}
	}

	if (text)
		return ("text/plain");

	return ("application/octet-stream");
}

t_file_type	Response::stringToEnum(std::string const& str){

	if (str.compare(0, 20, "89 50 4e 47 d a 1a a") == 0) return (E_PNG);
	if (str.compare(0, 5, "ff d8") == 0) return (E_JPEG);
	if (str.compare(0, 23, "3c 3f 78 6d 6c 20 76 65") == 0) return (E_SVG);
	if (str.compare(0, 17, "47 49 46 38 39 61") == 0) return (E_GIF);
	if (str.compare(0, 11, "25 50 44 46") == 0) return (E_PDF);
	if (str.compare(0, 18, "50 4b 3 4 14 0 8 0") == 0) return (E_ZIP);
	if (str.compare(0, 20, "0 0 0 20 66 74 79 70") == 0) return (E_MP4);
	else
		return (E_DEFAULT);
}

void	Response::uploadFileResponse(void){

	this->_status_code = 201;
	this->_status_message = "Created";
	generateResponse();
}

void	Response::errorResponse(int error_code, std::string message, std::map<int, std::string> error_paths){

	std::string error_path;

	error_path = matchErrorCodeWithPage(error_code, error_paths);
	cleanPath(error_path);
	if (error_path.empty() == true)
	{
		createHtmlErrorPage(error_code, message);
		generateResponse();
	}
	else
	{
		if (checkErrorFileAccess(error_code, message, error_path) == true)
		{
			if (checkContentType(error_path) == true)
			{
				this->_status_code = error_code;
				this->_status_message = message;
				this->_content_type = "text/html";

				std::ifstream	input(error_path);

				if (input.is_open())
				{
					std::string	buffer;
					std::string	stack;

					while (std::getline(input, buffer))
					{
						stack += buffer;
						buffer.clear();
						stack += '\n';
					}
					input.close();
					this->_content_len = stack.length();
					this->_body = stack;
					generateResponse();
				}
				else
					fileNotFound();
			}
			else
			{
				error_paths.clear();
				errorResponse(error_code, message, error_paths);
			}
		}
	}
}

std::string	Response::matchErrorCodeWithPage(int error_code, std::map<int, std::string> error_paths){

	for (std::map<int, std::string>::iterator it = error_paths.begin(); it != error_paths.end(); ++it)
	{
		if (it->first == error_code)
			return (it->second);
	}
	return ("");
}

void	Response::createHtmlErrorPage(int error_code, std::string message){

	int			i;
	int			integer[] = {400, 403, 404, 405, 413, 414, 415, 500, 501, 502, 503, 505};
	std::string	error_headers[] = {
	"The server could not understand the request due to invalid syntax.",
	"You do not have permission to access this resource on this server.",
	"The requested resource could not be found on this server.",
	"The method specified in the request is not allowed for the resource identified by the request URI.",
	"The request entity is larger than the server is willing or able to process.",
	"The requested URI is too long for the server to process.",
	"The media format of the requested data is not supported by the server.",
	"The server encountered an internal error or misconfiguration and was unable to complete your request.",
	"The server does not support the functionality required to fulfill the request.",
	"The server received an invalid response from the upstream server while trying to fulfill the request.",
	"The server is currently unable to handle the request due to a temporary overloading or maintenance of the server. Please try again later.",
	"The server does not support the HTTP protocol version used in the request."};

	i = 0;
	while (i < 7 && error_code != integer[i])
		i++;
	std::string header = error_headers[i];

	this->_body = "<!DOCTYPE html>\n"
				  "<html lang=\"en\">\n"
				  "<head>\n"
				  "<meta charset=\"UTF-8\">\n"
				  "<title>" + std::to_string(error_code) + " " + message + "</title>\n"
				  "</head>\n"
				  "<body>\n"
				  "<h1>" + std::to_string(error_code) + " " + message + "</h1>\n"
				  "<p>" + header + "</p>\n"
				  "</body>\n"
				  "</html>\n";
	this->_status_code = error_code;
	this->_status_message = message;
	this->_content_type = "text/html";
	this->_content_len = _body.length();
}

void	Response::fileNotFound(void){

	this->_status_code = 404;
	this->_status_message = "Not Found";
	this->_content_type = "text/plain";
	this->_body = "File not found.";
	this->_content_len = this->_body.length();
	generateResponse();
}

bool	Response::checkFileAccess(std::string path, std::map<int, std::string> error_paths, std::string perm){

	struct stat buf;

	if (stat(path.c_str(), &buf) != 0)
	{
		if (errno == ENOENT)
			errorResponse(404, "Not Found", error_paths);
		else
			errorResponse(500, "Internal Server Error", error_paths);
		return (false);
	}
	if (perm == "R" && access(path.c_str(), R_OK) != 0)
	{
		if (errno == EACCES)
			errorResponse(403, "Forbidden", error_paths);
		else
			errorResponse(500, "Internal Server Error", error_paths);
		return (false);
	}
	if (perm == "W" && access(path.c_str(), W_OK) != 0)
	{
		if (errno == EACCES)
			errorResponse(403, "Forbidden", error_paths);
		else
			errorResponse(500, "Internal Server Error", error_paths);
		return (false);
	}
	return (true);
}

bool	Response::checkErrorFileAccess(int error_code, std::string message, std::string error_path){

	struct stat buf;

	if (stat(error_path.c_str(), &buf) != 0 || access(error_path.c_str(), R_OK) != 0)
	{
		createHtmlErrorPage(error_code, message);
		generateResponse();
		return (false);
	}
	return (true);
}

void	Response::generateResponse(void){

	std::string	first_line;
	std::string	headers;
	std::string	body;

	if (this->_redir == true && this->_status_code < 400)
	{
		this->_status_code = 307;
		this->_status_message = "OK";
		first_line = std::string("HTTP/") +\
				this->_http_version + std::string(" ") +\
				std::to_string(this->_status_code) + std::string(" ") +\
				std::string(this->_status_message);
		headers += std::string("Location: ") + this->_location + std::string("\r\n");
	}
	else
	{
		first_line = std::string("HTTP/") +\
				this->_http_version + std::string(" ") +\
				std::to_string(this->_status_code) + std::string(" ") +\
				this->_status_message;
		if (this->_content_type.empty() == false)
			headers += std::string("Content-Type: ") + this->_content_type + std::string("\r\n");
		if (this->_content_len != -1)
			headers += std::string("Content-Length: ") + std::to_string(this->_content_len) + std::string("\r\n");
		if (this->_body.empty() == false)
			body += this->_body;
	}
	this->_response = first_line + std::string("\r\n") + headers + std::string("\r\n") + body + std::string("\r\n\r\n");
}

void	Response::setVersion(std::string version){

	_http_version = version;
}

void	Response::setLocation(std::string location){

	_location = location;
}

void	Response::setRedir(bool redir){

	_redir = redir;
}

void	Response::setDefaultFile(bool default_file){

	_default_file = default_file;
}

std::string	Response::getResponse(void) const{

	return _response;
}

std::string	Response::getLocation(void) const{

	return _location;
}

bool	Response::getRedir(void) const{

	return _redir;
}

bool	Response::getCGI(void) const{

	return _cgi;
}

bool	Response::getDefaultFile(void) const{

	return _default_file;
}
