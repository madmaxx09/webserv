/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   confParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/06/25 12:15:41 by gt-serst          #+#    #+#             */
/*   Updated: 2024/06/26 10:10:29 by febonaer         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFPARSER_HPP
# define CONFPARSER_HPP

# include "../webserv.hpp"
# include <string>
# include <iostream>
# include <fstream>
# include <cctype>
# include <vector>
# include <map>

//----------STRUCTS----------//

typedef struct s_locations
{
	std::string				root_path; //path where files should be searched from
	std::map<std::string, std::string>	redirections;
	std::vector<std::string>		default_path; //default path if request is a directory
	std::map<std::string, bool>		allowed_methods;
	bool					auto_index; //is directory listing allowed?
}	t_locations;

typedef struct s_server_scope
{
	int					port;
	int					max_body_size;
	std::vector<std::string>		server_name;
	std::map<int, std::string>		error_page_paths;
	std::map<std::string, t_locations>	locations;
	std::map<std::string, std::string>	cgi_path;
	std::string				upload_path;
}	t_server_scope;

//----------FUNCTIONS----------//

int						webserv(int argc, char **argv);
std::string					confChecker(int argc, char **argv);
t_server_scope					*confParser(std::string buffer, int *servers);

#endif
