/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   confChecker.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gt-serst <gt-serst@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/06/25 12:15:30 by gt-serst          #+#    #+#             */
/*   Updated: 2024/06/26 10:09:55 by febonaer         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "confParser.hpp"

static bool	argsChecker(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cerr << "ERROR: Webserv needs a configuration file." << std::endl;
		return (0);
	}
	else if (argc > 2)
	{
		std::cerr << "ERROR: Webserv only needs one argument. (configuration file)" << std::endl;
		return (0);
	}
	int	i = 0;
	while (argv[1][i])
		i++;
	if (argv[1][i - 1] != 'f' || argv[1][i - 2] != 'n' || argv[1][i - 3] != 'o' || argv[1][i - 4] != 'c' || argv[1][i - 5] != '.')
	{
		std::cerr << "ERROR: Wrong configuration file format ('.conf' needed)" << std::endl;
		return (0);
	}
	return (1);
}

std::string	confChecker(int argc, char **argv)
{
	if (!argsChecker(argc, argv))
		return ("");
	std::ifstream	inputFile(argv[1]);
	if (inputFile.is_open())
	{
		std::string buffer;
		std::string stack;
		while (std::getline(inputFile, buffer))
		{
			stack += buffer;
			stack += '\n';
		}
		stack += '\0';
		inputFile.close();
		return (stack);
	}
	else
	{
		std::cerr << "Exception opening/reading/closing file" << std::endl;
		return ("");
	}
}
