/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/06/25 12:15:50 by gt-serst          #+#    #+#             */
/*   Updated: 2024/07/03 14:09:00 by gt-serst         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Request.hpp"
#include "../exec/Server.hpp"

bool Request::multiform_headers(std::stringstream& ss, std::streampos& pos, int i)
{
	ss.seekg(pos);
	std::string line;
	std::getline(ss, line);
	size_t end_pos;
	while (line.find(":") != std::string::npos)
	{
		size_t start_pos = line.find("Content-Disposition:");
		if (start_pos != std::string::npos)
		{
			start_pos = line.find("filename=\"");
			if (start_pos != std::string::npos)
			{
				end_pos = line.find("\"", start_pos + 10);
				_multiform[i].filename = line.substr(start_pos + 10, end_pos - (start_pos + 10));
				for (size_t j = 0; j < _multiform[i].filename.length(); j++)
				{
					if (std::isspace(_multiform[i].filename[j]))
					{
						state = R_error;
						_error_code = 400;
						_error_msg = "Bad request the name of the file you are trying to upload contains a whitespace";
						return true;
					}
				}
			}
		}
		start_pos = line.find("Content-Type:");
		if (start_pos != std::string::npos)
		{
			_multiform[i].type = line.substr(start_pos + 13, line.length() - (start_pos + 13) - 1);
		}
		std::getline(ss, line);
	}
	pos = ss.tellg();
	return false;
}

void Request::parse_multiform(std::stringstream& ss, std::streampos pos)
{
	if (_boundary.length() > 70)
	{
		_error_code = 400;
		_error_msg = "The provided boundary is longer than 70 characters";
		state = R_error;
		return ;
	}
	ss.seekg(pos);
	std::string line;
	int i = 0;
	std::string bound_morph = _boundary.erase(_boundary.size() - 1);
	_boundary += "\r";
	while (std::getline(ss, line))
	{
		if(line.compare("--" + _boundary) == 0)
		{
			i++;
			t_multi value;
			_multiform[i] = value;
			pos = ss.tellg();
			if (multiform_headers(ss, pos, i))
				return ;
			ss.seekg(pos);
			std::getline(ss, line);
		}
		else if (line.compare("--" + bound_morph + "--\r") == 0)
		{
			return ;
		}
		line += "\n";
		_multiform[i].content += line;
	}
}

bool Request::getBoundary()
{
	std::string content = getHeader("Content-Type");
	if (content.empty())
		return 0;
	if (_headers["Content-Type"].find("multipart/form-data") != std::string::npos)
	{
		size_t pos = _headers["Content-Type"].find("boundary=", 0);
		if (pos != std::string::npos)
		{
			_boundary = _headers["Content-Type"].substr(pos + 9, _headers["Content-Type"].size());
			multiform = true;
			return 1;
		}
	}
	return 0;
}

bool Request::handle_query()
{
	_query_args.clear();
	size_t start = 0;

	while (start < _query_str.size())
	{
		size_t equal_pos = _query_str.find('=', start);
		size_t amp_pos = _query_str.find('&', start);
		if (equal_pos == std::string::npos)
			return false;
		std::string key = _query_str.substr(start, equal_pos - start);
		std::string value;
		if (amp_pos == std::string::npos || amp_pos + 1 > _query_str.length())
		{
			value = _query_str.substr(equal_pos + 1);
			_query_args[key] = value;
			break;
		}
		else
		{
			value = _query_str.substr(equal_pos + 1, amp_pos - equal_pos - 1);
			_query_args[key] = value;
			start = amp_pos + 1;
		}
	}
	return true;
}

void	Request::manage_chunks(const char *chunk)
{
	char ch;
	int len = std::strlen(chunk);
	for (int i = 0; i <= len; i++)
	{
		ch = chunk[i];
 		switch (state)
		{
			case R_chunked_start:
			{
				if (ch >= '0' && ch <= '9')
				{
					state = R_chunk_size;
					chunk_size = ch - '0';
					break ;
				}
				ch = (ch | 0x20);
				if (ch >= 'a' && ch <= 'z')
				{
					state = R_chunk_size;
					chunk_size = ch - 'a' + 10;
					break ;
				}
				else
				{
					state = R_error;
					_error_msg = "Chunk syntax";
					_error_code = 400;
					return ;
				}
			}
			case R_chunk_size:
			{
				if (ch == '\r')
				{
					state = R_chunk_cr;
					break ;
				}
				if (ch >= '0' && ch <= '9')
				{
					chunk_size = chunk_size * 16 + (ch - '0');
					break ;
				}
				ch = (ch | 0x20);
				if (ch >= 'a' && ch <= 'z')
				{
					chunk_size = chunk_size * 16 + (ch - 'a' + 10);
					break;
				}
			}
			case R_chunk_cr:
			{
				if (ch == '\n')
				{
					state = R_chunk_lf;
					break ;
				}
				state = R_error;
				_error_msg = "Chunk syntax";
				_error_code = 400;
				return ;
			}
			case R_chunk_lf:
			{
				if (chunk_size == 0)
				{
					state = R_chunk_done;
					chunked = false;
					return ;
				}
				else
				{
					state = R_chunk_content;
					break ;
				}
			}
			case R_chunk_content:
			{
				if (ch == '\r')
				{
					state = R_chunk_content_cr;
				}
				else
					_body.push_back(ch);
				break ;
			}
			case R_chunk_content_cr:
			{
				if (ch == '\n')
					state = R_chunk_content_lf;
				else
				{
					state = R_error;
					return ;
				}
				break ;
			}
			case R_chunk_content_lf:
			{
				if (chunk_size == 0)
				{
					state = R_chunk_done;
					chunked = false;
				}
				else
					state = R_chunked_start;
				return ;
			}
		}
	}
	state = R_error;
}

void	Request::validity_checks()
{
	if (_body_len != -1)
	{
		std::string hlen = getHeader("Content-Length");
		if (hlen.empty())
		{
			_error_code = 400;
			_error_msg = "Content-Length header is missing";
			return ;
		}
		if (ft_atoi(hlen.c_str()) != _body_len)
		{
			_error_code = 400;
			_error_msg = "Body length does not match with Content-Length header";
			return ;
		}
	}
	if (_body.empty() == false)
	{
		if (getHeader("Content-Type").empty())
		{
			_error_code = 400;
			_error_msg = "Content-Type header is missing";
			return ;
		}
	}
}

int hexDigitToInt(char ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

int convert_hexa(std::string &str, size_t &i, size_t &len)
{
	if ((i + 2) >= len) return 1;

	if (std::isalnum(str[i + 1]) && std::isalnum(str[i + 2]))
	{
		int high = hexDigitToInt(str[i + 1]);
		int low = hexDigitToInt(str[i + 2]);
		if (high == -1 || low == -1) return 1;
		char convertedChar = (high << 4) | low;
		str.replace(i, 3, 1, convertedChar);
		len -= 2;
	}
	return 0;
}

std::string toLowerCase(const std::string& str)
{
	std::string result = str;
	for (size_t i = 0; i < result.length(); ++i)
		result[i] = std::tolower(result[i]);
	return result;
}

std::string	Request::standardise(std::string str)
{
	size_t len = str.length();
	for (size_t i = 0; i < len; i++)
	{
 		if (str[i] == '%')
			if (convert_hexa(str, i, len) != 0)
			{
				state = R_error;
				_error_code = 400;
				_error_msg = "Bad request hexa";
				return str;
			}
	}
	return toLowerCase(str);
}

int ip_checker(std::string& ip)
{
	int valid = 0;
	int test = 0;
	size_t len = ip.length();
	size_t pos;
	size_t tmp = 0;

	while ((pos = ip.find(".", tmp)) != std::string::npos)
	{
		test = ft_atoi(ip.substr(tmp, pos - tmp).c_str());
		if (test > 255 || test < 0)
			return 1;
		tmp = pos + 1;
		valid++;
	}
	if (valid == 3)
	{
		test = ft_atoi(ip.substr(tmp, len - tmp).c_str());
		if (test <= 255 && test >= 0)
			valid++;
	}
	return (valid == 4) ? 0 : 1;
}

std::string convert_charptr_string(const char *line, int start, int end)
{
	char *extract = new char[end - start + 1];
	strncpy(extract, &line[start], end - start);
	extract[end - start] = '\0';
	std::string retval(extract);
	delete[] extract;
	return retval;
}

bool	unreserved_char(char ch)
{
	if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '!'
		|| ch == '~' || ch == '*' || ch == '\'' || ch == '(' || ch == ')' || ch == '=')
		return true;
	else
		return false;
}

bool	allowedCharURI(char ch)
{
	if ((ch >= '#' && ch <= ';') || (ch >= '?' && ch <= '[') || (ch >= 'a' && ch <= 'z') ||
		ch == '!' || ch == '=' || ch == ']' || ch == '_' || ch == '~')
		return (true);
	return (false);
}

Request::Request(){}

Request::Request(std::string& buffer, Server& server)
{
	std::cout << "Request parsing started" << std::endl;
	_server = &server;
	_version = "1.1";
	_path_to_file = "/";
	_hostname = "";
	_litteral_ip = "";
	_body = "";
	_query_str = "";
	_error_code = -1;
	_error_msg = "";
	_port = server.getConfig().port;
	chunk_size = 0;
	_body_len = -1;
	_fragment = "";
	chunked = false;
	multiform = false;
	_boundary = "";
	state = R_line;
	setRequest(buffer);
	if (state != R_error)
		_hostname = standardise(_hostname);

	if (state != R_error)
		_path_to_file = standardise(_path_to_file);
	if (state != R_error)
	{
		_query_str = standardise(_query_str);
		if (!handle_query())
		{
			_error_code = 400;
			_error_msg = "Bad query format";
			state = R_error;
			return ;
		}
	}
	if (state != R_error)
		validity_checks();
	std::cout << "Request parsing finished" << std::endl;
}

Request::~Request()
{
	_request.clear();
	_request_method.clear();
	_hostname.clear();
	_path_to_file.clear();
	_version.clear();
	_headers.clear();
	_query_args.clear();
	_body.clear();
	_query_str.clear();
	_error_msg.clear();
	_litteral_ip.clear();
	_boundary.clear();
	_fragment.clear();
	_multiform.clear();
	_error_code = -1;
	_port = _server->getConfig().port;
	chunk_size = 0;
	_body_len = -1;
	chunked = false;
	multiform = false;
	//std::cout << "Request destroyed" << std::endl;
}

void Request::setRequest(std::string& buffer)
{
	_request = buffer;
	std::stringstream ss(_request);
	std::string extract_line;
	std::getline(ss, extract_line, '\n');
	extract_line += '\n';
	char *toparse = new char[extract_line.size() + 1];
	std::strcpy(toparse, extract_line.c_str());
	parseRequestLine(toparse);
	delete[] toparse;
	if (state == R_error)
		return ;
	std::streampos pos = ss.tellg();
	pos = setHeader(ss, pos);
	if (state == R_error)
	{
		ss.flush();
		return ;
	}
	if (handle_headers())
	{
		state = R_error;
		ss.flush();;
		return ;
	}
	if (getHeader("Transfer-Encoding").compare("chunked\r") == 0)
	{
		std::string chunk;
		state = R_chunked_start;
		chunked = true;
		while (chunked)
		{
			std::getline(ss, chunk);
			if (!ss.eof())
				chunk += '\n';
			manage_chunks(chunk.c_str());
		}
		return ;
	}
	if (getBoundary())
		parse_multiform(ss, pos);
	else
		setBody(ss, pos);
	ss.flush();
}

void Request::parseRequestLine(char *line)
{
	int len = std::strlen(line);
	int start = 0;
	if (len > MAX_URI_SIZE)
	{
		_error_msg = "Request-URI Too Large";
		_error_code = 414;
		state = R_error;
		return ;
	}

	for (int i = 0; i <=len; i++)
	{
		switch (state)
		{
			case R_line:
			{
				if (std::strncmp(line, "GET", 3) == 0)
				{
					_request_method = "GET";
					i += 2;
					state = R_method;
				}
				else if (std::strncmp(line, "POST", 4) == 0)
				{
					_request_method = "POST";
					i += 3;
					state = R_method;
				}
				else if (std::strncmp(line, "DELETE", 6) == 0)
				{
					_request_method = "DELETE";
					i += 5;
					state = R_method;
				}
				else
				{
					_error_msg = "Method Not Allowed";
					_error_code = 405;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_method:
			{
				if (line[i] == ' ')
					state = R_first_space;
				else
				{
					_error_msg = "Bad request : first space";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_first_space:
			{
				if (line[i] == '/')
				{
					start = i;
					state = R_abs_path;
				}
				else if (std::strncmp(&line[i], "http://", 7) == 0)
				{
					i += 6;
					state = R_abs_slashes;
				}
				else if (std::strncmp(&line[i], "https://", 8) == 0)
				{
					i += 7;
					state = R_abs_slashes;
				}
				else
				{
					_error_msg = "Bad request : scheme";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_abs_slashes:
			{
				if (std::isdigit(line[i]))
				{
					start = i;
					state = R_abs_literal_ip;
				}
				else if (std::isalpha(line[i]) || line[i] == '_' || line[i] == '-')
				{
					state = R_abs_host_start;
					start = i;
				}
				else
				{
					state = R_error;
					_error_code = 400;
					_error_msg = "Bad request : hostname not valid";
					return ;
				}
				break ;
			}
			case R_abs_literal_ip:
			{
				if (line[i] == ':')
				{
					_litteral_ip = convert_charptr_string(line, start, i);
					if (ip_checker(_litteral_ip))
					{
						state = R_error;
						_error_code = 400;
						_error_msg = "Bad IP adress, IPv6 is not supported";
						return ;
					}
					state = R_abs_port;
					start = i;
				}
				else if (line[i] == '/')
				{
					_litteral_ip = convert_charptr_string(line, start, i);
					if (ip_checker(_litteral_ip))
					{
						state = R_error;
						_error_code = 400;
						_error_msg = "Bad IP adress, IPv6 is not supported";
						return ;
					}
					start = i;
					state = R_abs_path;
				}
				else if (!std::isdigit(line[i]) && line[i] != '.')
				{
					_error_msg = "Bad request : wrong ip";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_abs_host_start:
			{
				if (line[i] == ' ')
				{
					_hostname = convert_charptr_string(line, start, i);
					state = R_second_space;
				}
				else if (!allowedCharURI(line[i]))
				{
					_error_msg = "Bad request : host";
					_error_code = 400;
					state = R_error;
					return ;
				}
				else if (line[i] == '/')
				{
					_hostname = convert_charptr_string(line, start, i);
					start = i;
					state = R_abs_path;
				}
				else if (line[i] == ':')
				{
					_hostname = convert_charptr_string(line, start, i);
					start = i + 1;
					state = R_abs_port;
				}
				break ;
			}
			case R_abs_host_end:
			{
				if (line[i] == ':')
				{
					start = i + 1;
					state = R_abs_port;
				}
				else
				{
					_error_msg = "Bad request : host";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_abs_port:
			{
				if (line[i] == '/' && line[i - 1] != ':')
				{
					_port = ft_atoi(convert_charptr_string(line, start, i).c_str());
					start = i;
					state = R_abs_path;
				}
				else if (line[i] == ' ')
				{
					_port = ft_atoi(convert_charptr_string(line, start, i).c_str());
					state = R_second_space;
				}
				else if(!std::isdigit(line[i]))
				{
					_error_msg = "Bad request : port format";
					state = R_error;
					return ;
				}
				break ;
			}
			case R_abs_path:
			{
				if (line[i] == '?')
				{
					_path_to_file = convert_charptr_string(line, start, i);
					start = i + 1;
					state = R_uri_query;
				}
				else if (line[i] == '#')
				{
					_path_to_file = convert_charptr_string(line, start, i);
					start = i + 1;
					state = R_fragment ;
				}
				else if (line[i] == ' ')
				{
					_path_to_file = convert_charptr_string(line, start, i);
					start = 0;
					state = R_second_space;
				}
				else if (!allowedCharURI(line[i]))
				{
					_error_msg = std::string("Bad character : '") + line[i] + '\'';
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_second_space:
			{
				if (std::strncmp(&line[i], "HTTP/", 5) != 0)
				{
					_error_msg = "Bad request : this server only handles HTTP requests";
					_error_code = 501;
					state = R_error;
					return ;
				}
				else
				{
					i += 4;
					start = i;
					state = R_version_major;
				}
				break ;
			}
			case R_version_major:
			{
				if (line[i] != '1' &&
					line[i] != '2' &&
					line[i] != '3')
				{
					_error_code = 505;
					_error_msg = "HTTP Version not supported";
					state = R_error;
					return ;
				}
				start = i;
				state = R_version_dot;
				break ;
			}
			case R_version_minor:
			{
				if (!std::isdigit(line[i]))
				{
					_error_code = 400;
					_error_msg = "Bad request : version";
					state = R_error;
					return ;
				}
				state = R_version_done;
				break ;
			}
			case R_version_dot:
			{
				if (line[i] == '.')
				{
					state = R_version_minor;
					break ;
				}
				else
					state = R_version_done;
			}
			case R_version_done:
			{
				if (line[i] == '\r')
				{
					_version = convert_charptr_string(line, start, i);
					state = R_cr;
				}
				else
				{
					_error_msg = "Bad request : CR";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_cr:
			{
				if (line[i] == '\n' )
					state = R_crlf;
				else
				{
					_error_msg = "Bad request : LF";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_crlf:
			{
				if (i == len)
				{
					return ;
				}
				else
				{
					_error_msg = "Bad request : fatal";
					_error_code = 400;
					state = R_error;
					return ;
				}
			}
			case R_uri_query:
			{
				if (line[i] == ' ')
				{
					_query_str = convert_charptr_string(line, start, i);
					start = 0;
					state = R_second_space;
				}
				else if (line[i] == '#')
				{
					_query_str = convert_charptr_string(line, start, i);
					start = i + 1;
					state = R_fragment;
				}
				else if (unreserved_char(line[i]) || line[i] == '&' || line[i] == '/')
				{
					break ;
				}
				else
				{
					_error_msg = "Bad request : query format";
					_error_code = 400;
					state = R_error;
					return ;
				}
				break ;
			}
			case R_fragment:
			{
				if (line[i] == ' ')
				{
					_fragment = convert_charptr_string(line, start, i);
					start = 0;
					state = R_second_space;
				}
				else if (unreserved_char(line[i]))
					break;
				else
				{
					_error_code = 400;
					_error_msg = "Bad request : fragment format";
					state = R_error;
					return ;
				}
			}
		}
	}
	return ;
}

void	Request::setPathToFile(const std::string& path_to_file)
{
	_path_to_file = path_to_file;
}

bool	Request::handle_headers()
{
	std::string line = getHeader("Host");
	if (line.empty())
	{
		_error_code = 400;
		_error_msg = "Host header is missing";
		return true;
	}
	size_t pos = line.find(":");
	if (pos != std::string::npos)
	{
		if (_hostname.empty() == 0  && line.substr(0, pos).compare(_hostname))
		{
			_error_code = 400;
			_error_msg = "Hostname in the URI does not match the hostname in the Host header";
			return true;
		}
		_hostname = line.substr(0, pos);
		_port = ft_atoi(line.substr(pos + 1, line.length()).c_str());
		if (_server->getConfig().port != _port)
		{
			_error_code = 400;
			_error_msg = "The port in the host header does not match the port of the request";
			return true;
		}
	}
	else
	{
		if (_hostname.empty() == 0  && line.compare(_hostname + "\r"))
		{
			_error_code = 400;
			_error_msg = "Hostname in the URI does not match the hostname in the Host header";
			return true;
		}
		_hostname = line;
		if (_hostname[_hostname.length() - 1] == '\r' || _hostname[_hostname.length() - 1] == '\n')
			_hostname.erase(_hostname.length() - 1, 1);
	}
	int i = 0;
	std::vector<std::string> tmp = _server->getConfig().server_name;
	for (std::vector<std::string>::iterator it = tmp.begin(); it != tmp.end(); ++it)
	{
		if (_hostname.compare(*it) == 0 || (*it == "localhost" && _hostname == "127.0.0.1"))
			break;
		i++;
	}
	if (tmp.begin() + i == tmp.end())
	{
		tmp.clear();
		_error_code = 400;
		_error_msg = "Hostname in the request does not match the hostname in the server config";
		return true;
	}
	tmp.clear();
	line = getHeader("Content-Length");
	if (!line.empty())
	{
		if (!getHeader("Transfer-Encoding").empty())
		{
			_error_code = 400;
			_error_msg = "A request cant have both the transfer encoding and the content length headers";
			return true;
		}
		if (ft_atoi(line.c_str()) > _server->getConfig().max_body_size)
		{
			_error_code = 413;
			_error_msg = "The body is greater than the max body size accepted by the server";
			return true;
		}
	}
	return false;
}

std::streampos Request::setHeader(std::stringstream& ss, std::streampos startpos)
{
	ss.seekg(startpos);
	std::string line;

	while (std::getline(ss, line, '\n'))
	{
		if (line.compare("\r") == 0)
		{
			state = R_headers;
			return(ss.tellg());
		}
		size_t pos = line.find(':');
		if (pos != std::string::npos)
		{
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);
			key.erase(0, key.find_first_not_of(" \t"));
			key.erase(key.find_last_not_of(" \t") + 1);
			value.erase(0, value.find_first_not_of(" \t"));
			value.erase(value.find_last_not_of(" \t") + 1);
			_headers[key] = value;
		}
		else
		{
			state = R_error;
			_error_msg = "Bad request : wrong header format";
			_error_code = 400;
			return 0;
		}
	}
	state = R_done;
	return(ss.tellg());
}

void Request::setBody(std::stringstream& ss, std::streampos startpos)
{
	ss.seekg(startpos);
	std::string line;
	while (std::getline(ss, line))
	{
		if (!ss.eof())
			line += "\n";
		_body += line;
	}
	if (_body.length() != 0)
		_body_len = _body.length();
	state = R_done;
	return ;
}

//GET FUNCTIONS

std::string	Request::getRequest() const
{
	return _request;
}

std::string Request::getRequestMethod() const
{
	return _request_method;
}

std::string Request::getPathToFile() const
{
	return _path_to_file;
}

std::string Request::getVersion() const
{
	return _version;
}

std::string Request::getHeader(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(key);
	if (it != _headers.end())
	{
		return it->second;
	}
	else
	{
		return "";
	}
}

std::string Request::getBody() const
{
	return _body;
}

std::string	Request::getErrorMsg() const
{
	return _error_msg;
}

int	Request::getErrorCode() const
{
	return _error_code;
}

int Request::getPort() const
{
	return _port;
}

std::string Request::getHost() const
{
	return _hostname;
}

std::string Request::getQuerystr() const
{
	return _query_str;
}

std::string Request::getIp() const
{
	return _litteral_ip;
}

int	Request::getLen() const
{
	return _body_len;
}

std::map<int, t_multi> Request::getMulti() const
{
	return _multiform;
}

std::map<std::string, std::string> Request::getQuery_args() const
{
	return _query_args;
}
